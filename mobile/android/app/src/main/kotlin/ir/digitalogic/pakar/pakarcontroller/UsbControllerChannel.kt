package ir.digitalogic.pakar.pakarcontroller

import android.app.Activity
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.*
import android.os.Build
import android.os.Handler
import android.os.Looper
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicInteger

/** Explicit CDC ACM host, not a UART-bridge driver. No device/reset guessing. */
class UsbControllerChannel(private val activity: Activity, messenger: BinaryMessenger) :
    MethodChannel.MethodCallHandler, EventChannel.StreamHandler {
    private val manager = activity.getSystemService(Context.USB_SERVICE) as UsbManager
    private val main = Handler(Looper.getMainLooper())
    private val writes = Executors.newSingleThreadExecutor()
    private val reads = Executors.newSingleThreadExecutor()
    private val methods = MethodChannel(messenger, "ir.digitalogic.pakar/usb")
    private val events = EventChannel(messenger, "ir.digitalogic.pakar/usb-events")
    private val generation = AtomicLong(0)
    private val permissionAction = "${activity.packageName}.USB_PERMISSION"
    private var sink: EventChannel.EventSink? = null
    private var connection: UsbDeviceConnection? = null
    private var selected: UsbDevice? = null
    private var control: UsbInterface? = null
    private var data: UsbInterface? = null
    private var output: UsbEndpoint? = null
    private var pending: MethodChannel.Result? = null
    private var pendingDevice: UsbDevice? = null
    private var pendingOwner: Long? = null
    private var currentOwner: Long? = null
    @Volatile private var closed = false
    private var queuedWrites = 0
    private val queuedReads = AtomicInteger(0)
    private data class Ports(val control: UsbInterface, val data: UsbInterface,
                             val input: UsbEndpoint, val output: UsbEndpoint)

    // Only the owned TinyUSB S3 image is supported. CP210x/CH34x and arbitrary
    // CDC modems are deliberately not claimed by this minimal safe driver.
    private fun ports(device: UsbDevice): Ports? {
        if (device.vendorId != 0x303a || device.productName != "PakarController") return null
        for (i in 0 until device.interfaceCount) {
            val c = device.getInterface(i)
            if (c.interfaceClass != UsbConstants.USB_CLASS_COMM || c.interfaceSubclass != 2) continue
            for (j in 0 until device.interfaceCount) {
                val d = device.getInterface(j)
                if (d.interfaceClass != UsbConstants.USB_CLASS_CDC_DATA || d.id != c.id + 1) continue
                val endpoints = (0 until d.endpointCount).map { d.getEndpoint(it) }
                val input = endpoints.firstOrNull { it.type == UsbConstants.USB_ENDPOINT_XFER_BULK && it.direction == UsbConstants.USB_DIR_IN }
                val output = endpoints.firstOrNull { it.type == UsbConstants.USB_ENDPOINT_XFER_BULK && it.direction == UsbConstants.USB_DIR_OUT }
                if (input != null && output != null) return Ports(c, d, input, output)
            }
        }
        return null
    }

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            @Suppress("DEPRECATION") val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE) ?: return
            if (intent.action == UsbManager.ACTION_USB_DEVICE_DETACHED && selected?.deviceId == device.deviceId) {
                retire("USB device detached")
            } else if (intent.action == permissionAction && pendingDevice?.deviceId == device.deviceId) {
                val reply = pending ?: return
                val owner = pendingOwner ?: return
                pending = null; pendingDevice = null; pendingOwner = null
                if (manager.hasPermission(device)) open(device, owner, reply)
                else reply.error("usb_permission", "USB permission was not granted", null)
            }
        }
    }

    init {
        methods.setMethodCallHandler(this); events.setStreamHandler(this)
        val filter = IntentFilter(permissionAction).apply { addAction(UsbManager.ACTION_USB_DEVICE_DETACHED) }
        if (Build.VERSION.SDK_INT >= 33) activity.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
        else { @Suppress("DEPRECATION") activity.registerReceiver(receiver, filter) }
    }

    override fun onListen(arguments: Any?, eventSink: EventChannel.EventSink) { sink = eventSink }
    override fun onCancel(arguments: Any?) { sink = null }
    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        if (closed) { result.error("usb_closed", "USB host disposed", null); return }
        when (call.method) {
            "devices" -> result.success(manager.deviceList.values.mapNotNull { device ->
                ports(device)?.let { p -> mapOf("device_id" to device.deviceId, "interface" to p.control.id,
                    "vendor_id" to device.vendorId, "product_id" to device.productId,
                    "name" to (device.productName ?: "ESP32-S3 CDC"), "permission" to manager.hasPermission(device)) }
            })
            "connect" -> {
                if (pending != null) { result.error("usb_busy", "A USB permission request is already open", null); return }
                if (connection != null) { result.error("usb_busy", "Disconnect the selected USB owner before selecting another interface", null); return }
                val owner = call.argument<Number>("owner")?.toLong()
                if (owner == null) { result.error("usb_owner", "USB request owner missing", null); return }
                val id = call.argument<Int>("device_id")
                val device = manager.deviceList.values.firstOrNull { it.deviceId == id }
                val p = device?.let { ports(it) }
                if (device == null || p == null || p.control.id != call.argument<Int>("interface")) {
                    result.error("usb_device", "Selected supported CDC interface is no longer attached", null); return
                }
                retire("USB owner replaced")
                if (manager.hasPermission(device)) open(device, owner, result)
                else {
                    pending = result; pendingDevice = device; pendingOwner = owner
                    val intent = Intent(permissionAction).setPackage(activity.packageName)
                    manager.requestPermission(device, PendingIntent.getBroadcast(activity, device.deviceId, intent,
                        PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
                }
            }
            "write" -> {
                val bytes = call.argument<ByteArray>("bytes")
                val session = call.argument<Number>("session")?.toLong()
                val c = connection; val endpoint = output
                if (bytes == null || bytes.isEmpty() || bytes.size > 56 || c == null || endpoint == null || session != generation.get()) {
                    result.error("usb_session", "USB write has no current owner or valid bounded frame", null); return
                }
                if (queuedWrites >= 8) { result.error("usb_busy", "USB write queue is full; nothing sent", null); return }
                queuedWrites++
                writes.execute {
                    var sent = 0
                    val deadline = System.nanoTime() + 1_500_000_000L
                    var error: String? = null
                    try {
                        while (sent < bytes.size && generation.get() == session && System.nanoTime() < deadline) {
                            val count = c.bulkTransfer(endpoint, bytes, sent, bytes.size - sent, 250)
                            if (count <= 0) { error = "USB bulk write failed after $sent bytes; outcome unknown, no replay"; break }
                            sent += count
                        }
                        if (sent != bytes.size) error = error ?: "USB write retired or expired; outcome unknown, no replay"
                    } catch (_: RuntimeException) {
                        error = "USB bulk write interrupted after $sent bytes; outcome unknown, no replay"
                    } finally {
                        main.post {
                            queuedWrites--
                            if (error == null && generation.get() == session && !closed) result.success(sent)
                            else {
                                result.error("usb_write", error ?: "USB owner retired after write; no replay", null)
                                if (generation.get() == session) retire(error ?: "USB write failed")
                            }
                        }
                    }
                }
            }
            "disconnect" -> {
                // A retired actor must never close a replacement's physical handle.
                val session = call.argument<Number>("session")?.toLong()
                val owner = call.argument<Number>("owner")?.toLong()
                if (session != null && session == generation.get() || owner != null && owner == currentOwner) retire("USB disconnected")
                if (owner != null && owner == pendingOwner) {
                    pending?.error("usb_cancelled", "USB permission request retired", null)
                    pending = null; pendingDevice = null; pendingOwner = null
                }
                result.success(null)
            }
            else -> result.notImplemented()
        }
    }

    private fun open(device: UsbDevice, owner: Long, result: MethodChannel.Result) {
        val p = ports(device) ?: run { result.error("usb_device", "CDC interface disappeared", null); return }
        val c = manager.openDevice(device) ?: run { result.error("usb_open", "Unable to open selected USB device", null); return }
        if (!c.claimInterface(p.control, false) || !c.claimInterface(p.data, false)) {
            c.close(); result.error("usb_claim", "CDC interfaces are in use", null); return
        }
        // Standard 115200 8N1, DTR only. The owned OTG firmware disables the
        // TinyUSB reboot-on-line-state hook BEFORE starting CDC. Never RTS/1200.
        val line = byteArrayOf(0x00, 0xc2.toByte(), 0x01, 0x00, 0, 0, 8)
        if (c.controlTransfer(0x21, 0x20, 0, p.control.id, line, line.size, 500) < 0 ||
            c.controlTransfer(0x21, 0x22, 1, p.control.id, null, 0, 500) < 0) {
            c.close(); result.error("usb_cdc", "CDC line setup failed", null); return
        }
        val session = generation.incrementAndGet()
        connection = c; selected = device; control = p.control; data = p.data; output = p.output
        currentOwner = owner
        result.success(mapOf("session" to session))
        reads.execute {
            val buffer = ByteArray(2048)
            try {
                while (!closed && generation.get() == session) {
                    val count = c.bulkTransfer(p.input, buffer, buffer.size, 100)
                    if (count > 0) {
                        if (queuedReads.incrementAndGet() > 8) {
                            queuedReads.decrementAndGet()
                            main.post { if (generation.get() == session) retire("USB receive queue overflow; no bytes replayed") }
                            break
                        }
                        val copy = buffer.copyOf(count)
                        main.post {
                            queuedReads.decrementAndGet()
                            if (!closed && generation.get() == session) sink?.success(mapOf("session" to session, "bytes" to copy))
                        }
                    }
                }
            } catch (_: RuntimeException) {
                // Closing a retired connection can interrupt its old worker.
                // Never retire a replacement handle or spin on that exception.
                main.post { if (!closed && generation.get() == session) retire("USB bulk read interrupted; connection retired without replay") }
            }
        }
    }

    private fun retire(reason: String) {
        val old = generation.getAndIncrement()
        val c = connection; connection = null; selected = null; output = null
        if (c != null) {
            control?.let { runCatching { c.releaseInterface(it) } }
            data?.let { runCatching { c.releaseInterface(it) } }
            runCatching { c.close() }
            sink?.success(mapOf("session" to old, "type" to "disconnected", "reason" to reason))
        }
        control = null; data = null
        currentOwner = null
    }
    fun close() {
        if (closed) return
        closed = true; retire("USB host disposed")
        pending?.error("usb_closed", "USB permission request disposed", null); pending = null; pendingDevice = null; pendingOwner = null
        activity.unregisterReceiver(receiver)
        methods.setMethodCallHandler(null); events.setStreamHandler(null); sink = null
        reads.shutdownNow()
        // Drain already bounded write jobs after retirement so each captured
        // MethodChannel.Result and queue counter settles exactly once.
        writes.shutdown()
    }
}
