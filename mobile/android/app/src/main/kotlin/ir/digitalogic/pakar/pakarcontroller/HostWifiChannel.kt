package ir.digitalogic.pakar.pakarcontroller

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.net.wifi.WifiNetworkSuggestion
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel

/** Explicit host-side consent only. Never changes ESP settings or verifies a peer.
 * Reuses the donor DiscoveryChannel's main-loop/callback/close ownership pattern.
 */
class HostWifiChannel(private val activity: Activity, messenger: BinaryMessenger) {
    private val channel = MethodChannel(messenger, "ir.digitalogic.pakar/host-wifi")
    private val cm = activity.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private val main = Handler(Looper.getMainLooper())
    private var callback: ConnectivityManager.NetworkCallback? = null
    private var bound: Network? = null
    private var pending: MethodChannel.Result? = null
    private var permissionCall: MethodCall? = null
    private var saving = false
    private val permissionCode = 3114
    private val saveCode = 3115
    init { channel.setMethodCallHandler { call, result ->
        try {
            when (call.method) {
                "settings" -> { activity.startActivity(Intent(Settings.ACTION_WIFI_SETTINGS)); result.success(null) }
                "release", "cancel" -> { release(); complete("cancelled"); permissionCall = null; result.success(null) }
                "join", "save" -> {
                    check(pending == null && !saving) { "Finish the current Wi-Fi request first" }
                    val ssid = call.argument<String>("ssid") ?: ""
                    val password = call.argument<String>("password") ?: ""
                    val open = call.argument<Boolean>("open") == true
                    require(ssid.toByteArray(Charsets.UTF_8).size in 1..32 && ssid.none { it.code < 32 || it.code == 127 }) { "Invalid Wi-Fi name" }
                    require(open || password.length in 8..63 && password.all { it.code in 32..126 }) { "Use an 8–63 character WPA2 password" }
                    if (call.method == "save") {
                        check(Build.VERSION.SDK_INT >= 30) { "Saving networks requires Android 11. Use Wi-Fi settings instead." }
                        val suggestion = WifiNetworkSuggestion.Builder().setSsid(ssid).apply {
                            if (!open) setWpa2Passphrase(password)
                        }.build()
                        val intent = Intent(Settings.ACTION_WIFI_ADD_NETWORKS).putParcelableArrayListExtra(
                            Settings.EXTRA_WIFI_NETWORK_LIST, arrayListOf(suggestion))
                        pending = result; saving = true
                        activity.startActivityForResult(intent, saveCode)
                    } else {
                        check(Build.VERSION.SDK_INT >= 29) { "Local joins require Android 10. Use Wi-Fi settings instead." }
                        pending = result
                        val permission = if (Build.VERSION.SDK_INT >= 33) Manifest.permission.NEARBY_WIFI_DEVICES else Manifest.permission.ACCESS_FINE_LOCATION
                        if (activity.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                            permissionCall = call
                            activity.requestPermissions(arrayOf(permission), permissionCode)
                        } else join(call)
                    }
                }
                else -> result.notImplemented()
            }
        } catch (_: Exception) {
            if (pending === result) { pending = null; permissionCall = null; saving = false }
            result.error("wifi_unavailable", "Wi-Fi request unavailable. Check Wi-Fi and permissions, or use system Wi-Fi settings.", null)
        }
    } }
    private fun join(call: MethodCall) {
        release()
        val spec = WifiNetworkSpecifier.Builder().setSsid(call.argument<String>("ssid")!!).apply {
            if (call.argument<Boolean>("open") != true) setWpa2Passphrase(call.argument<String>("password")!!)
        }.build()
        val request = NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).setNetworkSpecifier(spec).build()
        val cb = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) { main.post {
                if (callback !== this) return@post
                try {
                    if (cm.bindProcessToNetwork(network)) { bound = network; complete("available") }
                    else { fail("Android could not route this app over Wi-Fi"); release() }
                } catch (_: Exception) { fail("Android denied Wi-Fi routing"); release() }
            } }
            override fun onUnavailable() { main.post {
                if (callback === this) { fail("Wi-Fi not available or request declined. Check the network name and password."); release() }
            } }
            override fun onLost(network: Network) { main.post {
                if (callback === this) { release(); complete("cancelled") }
            } }
        }
        callback = cb
        try { cm.requestNetwork(request, cb, 45000) }
        catch (_: Exception) { release(); fail("Wi-Fi permission or location service is unavailable. Use system Wi-Fi settings.") }
    }
    fun onRequestPermissionsResult(code: Int) {
        if (code != permissionCode) return
        val call = permissionCall ?: return
        permissionCall = null
        val permission = if (Build.VERSION.SDK_INT >= 33) Manifest.permission.NEARBY_WIFI_DEVICES else Manifest.permission.ACCESS_FINE_LOCATION
        if (activity.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) {
            try { join(call) } catch (_: Exception) { release(); fail("Wi-Fi join unavailable") }
        } else fail("Wi-Fi permission declined. You can still join through system Wi-Fi settings.")
    }
    fun onActivityResult(code: Int, result: Int, data: Intent?): Boolean {
        if (code != saveCode) return false
        saving = false
        if (result != Activity.RESULT_OK) complete("cancelled")
        else {
            val states = data?.getIntegerArrayListExtra(Settings.EXTRA_WIFI_NETWORK_RESULT_LIST)
            if (states?.size == 1 && states[0] in listOf(Settings.ADD_WIFI_RESULT_SUCCESS, Settings.ADD_WIFI_RESULT_ALREADY_EXISTS)) complete("saved")
            else fail("Android did not save the network. Use system Wi-Fi settings.")
        }
        return true
    }
    private fun complete(value: String) { val result = pending; pending = null; result?.success(value) }
    private fun fail(message: String) { val result = pending; pending = null; result?.error("wifi_failed", message, null) }
    private fun release() {
        // Release only our process binding; never disconnect the system's Wi-Fi.
        try { if (bound != null && cm.boundNetworkForProcess == bound) cm.bindProcessToNetwork(null) }
        catch (_: Exception) { /* Android owns cleanup if the network is already gone. */ }
        bound = null
        val old = callback; callback = null
        if (old != null) try { cm.unregisterNetworkCallback(old) } catch (_: Exception) {}
    }
    fun close() { release(); permissionCall = null; complete("cancelled"); channel.setMethodCallHandler(null) }
}
