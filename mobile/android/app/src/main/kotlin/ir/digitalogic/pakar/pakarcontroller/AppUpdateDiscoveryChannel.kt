package ir.digitalogic.pakar.pakarcontroller

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel

/** Adapted from PCController's actual DiscoveryChannel.kt registration, separate
 * from board discovery. TXT is an untrusted hint: Dart verifies the package-only
 * HTTP manifest. No selected board, installer, passwords or RPCs are accessed.
 */
class AppUpdateDiscoveryChannel(context: Context, messenger: BinaryMessenger) {
    private val manager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val wifi = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
    private val main = Handler(Looper.getMainLooper())
    private val channel = MethodChannel(messenger, "ir.digitalogic.pakar/app-update-discovery")
    private val serviceType = "_pakar-update._tcp."
    private var closed = false
    private var generation = 0
    private var scanResult: MethodChannel.Result? = null
    private var discovery: NsdManager.DiscoveryListener? = null
    private var registration: NsdManager.RegistrationListener? = null
    private var registrationResult: MethodChannel.Result? = null
    private var lock: WifiManager.MulticastLock? = null
    private var externalLock = false
    private var resolving = false
    private val queue = ArrayDeque<NsdServiceInfo>()
    private val found = linkedMapOf<String, Map<String, String>>()

    init {
        channel.setMethodCallHandler { call, result ->
            if (closed) result.error("closed", "Package discovery closed", null)
            else when (call.method) {
                "acquire" -> try {
                    acquireLock(); externalLock = true; result.success(null)
                } catch (_: Exception) { result.error("multicast_lock", "Multicast unavailable", null) }
                "release" -> { externalLock = false; releaseIfIdle(); result.success(null) }
                "scan" -> startScan(result)
                "advertise" -> {
                    val port = call.argument<Int>("port") ?: 0
                    val name = call.argument<String>("name") ?: ""
                    val instance = call.argument<String>("instance_id") ?: ""
                    if (port !in 1024..65535 || !safeText(name, 96) || !safeText(instance, 128)) {
                        result.error("invalid_advertisement", "Invalid package advertisement", null)
                    } else register(port, name, instance, result)
                }
                "unadvertise" -> { unadvertise(); result.success(null) }
                else -> result.notImplemented()
            }
        }
    }

    private fun safeText(text: String, limit: Int) = text.isNotEmpty() &&
        text.length <= limit && text.none { it.code < 32 || it.code == 127 }

    private fun acquireLock() {
        if (lock?.isHeld == true) return
        lock = wifi.createMulticastLock("pakar-package-discovery").apply {
            setReferenceCounted(false); acquire()
        }
    }

    private fun releaseIfIdle() {
        if (externalLock || scanResult != null || registration != null) return
        lock?.let { if (it.isHeld) it.release() }; lock = null
    }

    private fun startScan(result: MethodChannel.Result) {
        if (scanResult != null) { result.error("busy", "Package scan already running", null); return }
        scanResult = result
        val epoch = ++generation
        found.clear(); queue.clear(); resolving = false
        try {
            acquireLock()
            val listener = object : NsdManager.DiscoveryListener {
                override fun onDiscoveryStarted(type: String) {}
                override fun onDiscoveryStopped(type: String) {}
                override fun onServiceLost(info: NsdServiceInfo) {}
                override fun onStartDiscoveryFailed(type: String, code: Int) { main.post {
                    if (epoch == generation) finishScan("DNS-SD scan failed ($code)")
                } }
                override fun onStopDiscoveryFailed(type: String, code: Int) {}
                override fun onServiceFound(info: NsdServiceInfo) { main.post {
                    if (!closed && epoch == generation && queue.size < 64 && found.size < 128) {
                        queue.add(info); resolve(epoch)
                    }
                } }
            }
            discovery = listener
            manager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, listener)
            main.postDelayed({ if (epoch == generation) finishScan() }, 4000)
        } catch (_: Exception) { finishScan("DNS-SD unavailable") }
    }

    @Suppress("DEPRECATION")
    private fun resolve(epoch: Int) {
        if (closed || epoch != generation || resolving || queue.isEmpty()) return
        resolving = true
        val info = queue.removeFirst()
        try {
            manager.resolveService(info, object : NsdManager.ResolveListener {
                override fun onResolveFailed(service: NsdServiceInfo, code: Int) { main.post {
                    if (epoch == generation) { resolving = false; resolve(epoch) }
                } }
                override fun onServiceResolved(service: NsdServiceInfo) { main.post {
                    if (closed || epoch != generation) return@post
                    val address = service.host?.hostAddress
                    val product = service.attributes["product"]?.toString(Charsets.UTF_8)
                    val kind = service.attributes["kind"]?.toString(Charsets.UTF_8)
                    val instance = service.attributes["instance"]?.toString(Charsets.UTF_8) ?: ""
                    if (product == "pakarcontroller-mobile" && kind == "android-app" &&
                        address != null && !address.contains('%') && service.port in 1..65535 &&
                        safeText(instance, 128)) {
                        val host = if (address.contains(':')) "[$address]" else address
                        val endpoint = "http://$host:${service.port}"
                        found[endpoint] = mapOf("name" to service.serviceName.take(96),
                            "endpoint" to endpoint, "address" to address, "instance_id" to instance)
                    }
                    resolving = false; resolve(epoch)
                } }
            })
        } catch (_: Exception) { resolving = false; resolve(epoch) }
    }

    private fun finishScan(error: String? = null) {
        val result = scanResult ?: return
        scanResult = null; ++generation
        discovery?.let { try { manager.stopServiceDiscovery(it) } catch (_: Exception) {} }
        discovery = null; queue.clear(); resolving = false
        releaseIfIdle()
        if (error == null) result.success(found.values.toList())
        else result.error("discovery_failed", error, null)
    }

    private fun register(port: Int, name: String, instance: String, result: MethodChannel.Result) {
        if (registration != null) { result.error("busy", "Package listener already advertised", null); return }
        try { acquireLock() }
        catch (_: Exception) { result.error("multicast_lock", "Multicast unavailable", null); return }
        val service = NsdServiceInfo().apply {
            serviceName = name.take(48); serviceType = this@AppUpdateDiscoveryChannel.serviceType
            setPort(port); setAttribute("product", "pakarcontroller-mobile")
            setAttribute("kind", "android-app"); setAttribute("instance", instance)
            setAttribute("manifest", "/api/app-updates")
        }
        val listener = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(info: NsdServiceInfo) { main.post {
                if (registration === this && !closed) {
                    val reply = registrationResult; registrationResult = null
                    reply?.success(mapOf("name" to info.serviceName, "port" to info.port))
                }
            } }
            override fun onRegistrationFailed(info: NsdServiceInfo, code: Int) { main.post {
                if (registration === this) {
                    registration = null
                    val reply = registrationResult; registrationResult = null
                    reply?.error("registration_failed", "DNS-SD registration failed ($code)", null)
                    releaseIfIdle()
                }
            } }
            override fun onServiceUnregistered(info: NsdServiceInfo) {}
            override fun onUnregistrationFailed(info: NsdServiceInfo, code: Int) {}
        }
        registration = listener; registrationResult = result
        try {
            manager.registerService(service, NsdManager.PROTOCOL_DNS_SD, listener)
            main.postDelayed({
                if (registration === listener && registrationResult != null) {
                    unadvertise("DNS-SD registration timed out")
                }
            }, 6000)
        } catch (_: Exception) { unadvertise("DNS-SD registration unavailable") }
    }

    private fun unadvertise(error: String = "Package advertisement stopped") {
        val listener = registration; registration = null
        val result = registrationResult; registrationResult = null
        listener?.let { try { manager.unregisterService(it) } catch (_: Exception) {} }
        result?.error("registration_stopped", error, null)
        releaseIfIdle()
    }

    fun close() {
        if (closed) return
        closed = true; externalLock = false
        finishScan("Package discovery closed"); unadvertise(); releaseIfIdle()
        channel.setMethodCallHandler(null)
    }
}
