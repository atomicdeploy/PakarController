package ir.digitalogic.pakar.pakarcontroller

import android.Manifest
import android.app.Activity
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodChannel
import org.json.JSONObject
import java.util.ArrayDeque
import java.util.UUID

/** Native GATT only. The Dart owner handles bounded application framing/ACKs.
 * GATT write completion is NEVER an application command acknowledgement.
 * No hidden HTTP, WiFi, bonding, adapter-enable, background service or replay.
 */
@Suppress("DEPRECATION", "MissingPermission")
class BleControllerChannel(private val activity: Activity, messenger: BinaryMessenger) {
    companion object {
        private val SERVICE = UUID.fromString("a4b68d10-5c3f-4c5a-9f12-50616b617243")
        private val RX = UUID.fromString("a4b68d11-5c3f-4c5a-9f12-50616b617243")
        private val TX = UUID.fromString("a4b68d12-5c3f-4c5a-9f12-50616b617243")
        private val INFO = UUID.fromString("a4b68d13-5c3f-4c5a-9f12-50616b617243")
        private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val PERMISSION_CODE = 3080
    }
    private val channel = MethodChannel(messenger, "ir.digitalogic.pakar/ble")
    private val events = EventChannel(messenger, "ir.digitalogic.pakar/ble-events")
    private val handler = Handler(Looper.getMainLooper())
    private var sink: EventChannel.EventSink? = null
    private val adapter get() = (activity.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
    private var foreground = false
    private var closed = false
    private var epoch = 0
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private var ready = false
    private var mtu = 23
    private var phase = "idle"
    private var info = ""
    private var pendingConnect: MethodChannel.Result? = null
    private var pendingPermission: MethodChannel.Result? = null
    private var scanResult: MethodChannel.Result? = null
    private var scanner: BluetoothLeScanner? = null
    private var scanning: ScanCallback? = null
    private val devices = linkedMapOf<String, Map<String, Any>>()
    private data class Write(val bytes: ByteArray, val result: MethodChannel.Result, val epoch: Int)
    private val writes = ArrayDeque<Write>()
    private var writing: Write? = null
    private var writeTimer: Runnable? = null
    private var connectTimer: Runnable? = null
    private var scanTimer: Runnable? = null
    init {
        events.setStreamHandler(object : EventChannel.StreamHandler {
            override fun onListen(arguments: Any?, next: EventChannel.EventSink) { sink = next }
            override fun onCancel(arguments: Any?) { sink = null; disconnect("BLE event consumer retired") }
        })
        channel.setMethodCallHandler { call, result ->
            try {
                check(!closed) { "Bluetooth controller closed" }
                when (call.method) {
                    "status" -> {
                        val denied = missing()
                        result.success(mapOf("supported" to (adapter != null && activity.packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)), "enabled" to (if (denied.isEmpty()) adapter?.isEnabled else null), "missing_permissions" to denied, "ready" to ready, "session" to epoch, "mtu" to mtu))
                    }
                    "permission" -> {
                        check(foreground && pendingPermission == null) { "Permission request unavailable" }
                        val missing = missing()
                        if (missing.isEmpty()) result.success(mapOf("granted" to true))
                        else { pendingPermission = result; activity.requestPermissions(missing.toTypedArray(), PERMISSION_CODE) }
                    }
                    "enable" -> {
                        requireReadyAdapter(false)
                        activity.startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
                        result.success(mapOf("requested" to true))
                    }
                    "scan" -> scan(result)
                    "connect" -> connect(call.argument<String>("address") ?: "", result)
                    "disconnect" -> {
                        val expected = call.argument<Int>("session")
                        if (expected == null || expected == epoch) disconnect("Explicit Bluetooth disconnect")
                        result.success(null)
                    }
                    "write" -> {
                        // A paused activity still drains an admitted request and
                        // protocol ACKs. Dart admits no new background mutation.
                        check(ready && call.argument<Int>("session") == epoch) { "Bluetooth session is not ready" }
                        val bytes = call.argument<ByteArray>("value") ?: error("Missing BLE value")
                        require(bytes.size in 7..minOf(mtu - 3, 512) && writes.size < 16) { "BLE value or queue exceeds bound" }
                        val write = Write(bytes.copyOf(), result, epoch)
                        if (bytes[0].toInt() == 1) writes.addFirst(write) else writes.addLast(write)
                        nextWrite()
                    }
                    else -> result.notImplemented()
                }
            } catch (error: Exception) {
                if (pendingConnect === result) {pendingConnect = null;disconnect("Bluetooth dial rejected")}
                if (scanResult === result) {scanResult = null;finishScan("Bluetooth scan rejected")}
                result.error("ble_unavailable", error.message ?: "Bluetooth operation unavailable", null)
            }
        }
    }
    private fun missing(): List<String> {
        val needed = if (Build.VERSION.SDK_INT >= 31) listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
            else listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        return needed.filter { activity.checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
    }
    fun onRequestPermissionsResult(code: Int): Boolean {
        if (code != PERMISSION_CODE) return false
        pendingPermission?.success(mapOf("granted" to missing().isEmpty(), "missing_permissions" to missing()))
        pendingPermission = null; return true
    }
    private fun requireReadyAdapter(enabled: Boolean = true) {
        check(foreground) { "Bluetooth is foreground-only" }
        check(missing().isEmpty()) { "Nearby Devices permission is required (location on Android 11 or earlier)" }
        check(adapter != null) { "Bluetooth LE is unavailable on this device" }
        if (enabled) check(adapter?.isEnabled == true) { "Bluetooth is off; use Enable Bluetooth" }
    }
    private fun scan(result: MethodChannel.Result) {
        requireReadyAdapter(); check(scanning == null) { "Bluetooth scan already active" }
        devices.clear(); scanResult = result
        val callback = object : ScanCallback() {
            override fun onScanResult(type: Int, value: ScanResult) { handler.post {
                if (scanning !== this || devices.size >= 64) return@post
                val address = value.device.address
                devices[address] = mapOf("address" to address, "name" to (value.scanRecord?.deviceName ?: "PakarController"), "rssi" to value.rssi)
            } }
            override fun onScanFailed(errorCode: Int) { handler.post { if (scanning === this) finishScan("BLE scan failed ($errorCode)") } }
        }
        scanning = callback; scanner = adapter?.bluetoothLeScanner ?: error("BLE scanner unavailable")
        scanner!!.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
        scanTimer = Runnable { finishScan(null) }.also { handler.postDelayed(it, 6000) }
    }
    private fun finishScan(error: String?) {
        scanTimer?.let(handler::removeCallbacks);scanTimer = null
        try { scanning?.let { scanner?.stopScan(it) } } catch (_: Exception) {}
        scanning = null; scanner = null
        val result = scanResult;scanResult = null
        if (error == null) result?.success(devices.values.toList()) else result?.error("ble_scan", error, null)
    }
    private fun connect(address: String, result: MethodChannel.Result) {
        requireReadyAdapter(); require(BluetoothAdapter.checkBluetoothAddress(address)) { "Select an advertised BLE device" }
        finishScan(null); disconnect("Bluetooth connection replaced")
        pendingConnect = result; phase = "connecting"; mtu = 23
        val current = epoch
        val callback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(owner: BluetoothGatt, status: Int, state: Int) { handler.post {
                if (gatt !== owner || current != epoch) { owner.close(); return@post }
                if (status != BluetoothGatt.GATT_SUCCESS || state == BluetoothProfile.STATE_DISCONNECTED) { disconnect("Bluetooth link lost ($status)"); return@post }
                if (state == BluetoothProfile.STATE_CONNECTED) {
                    phase = "mtu"
                    if (!owner.requestMtu(517)) discover(owner)
                }
            } }
            override fun onMtuChanged(owner: BluetoothGatt, value: Int, status: Int) { handler.post {
                if (gatt !== owner || current != epoch || phase != "mtu") return@post
                if (status == BluetoothGatt.GATT_SUCCESS) mtu = value.coerceIn(23, 517)
                discover(owner)
            } }
            override fun onServicesDiscovered(owner: BluetoothGatt, status: Int) { handler.post {
                if (gatt !== owner || current != epoch || phase != "services") return@post
                val service = owner.getService(SERVICE)
                rx = service?.getCharacteristic(RX);tx = service?.getCharacteristic(TX)
                val identity = service?.getCharacteristic(INFO)
                if (status != BluetoothGatt.GATT_SUCCESS || rx == null || tx == null || identity == null ||
                    (rx!!.properties and BluetoothGattCharacteristic.PROPERTY_WRITE) == 0 ||
                    (tx!!.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0) { disconnect("Pakar BLE service/characteristics missing"); return@post }
                phase = "info"
                if (!owner.readCharacteristic(identity)) disconnect("Cannot read Pakar identity")
            } }
            override fun onCharacteristicRead(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) = identityRead(owner, characteristic, characteristic.value ?: byteArrayOf(), status, current)
            override fun onCharacteristicRead(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, status: Int) = identityRead(owner, characteristic, value.copyOf(), status, current)
            override fun onDescriptorWrite(owner: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) { handler.post {
                if (gatt !== owner || current != epoch || phase != "cccd" || descriptor.uuid != CCCD) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { disconnect("Pakar notifications were not acknowledged"); return@post }
                ready = true;phase = "ready";connectTimer?.let(handler::removeCallbacks);connectTimer = null
                val result = pendingConnect;pendingConnect = null
                result?.success(mapOf("session" to epoch, "mtu" to mtu, "info" to info))
            } }
            override fun onCharacteristicChanged(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic) = notification(owner, characteristic, characteristic.value?.copyOf() ?: byteArrayOf(), current)
            override fun onCharacteristicChanged(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) = notification(owner, characteristic, value.copyOf(), current)
            override fun onCharacteristicWrite(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) { handler.post {
                if (gatt !== owner || current != epoch || characteristic.uuid != RX) return@post
                val write = writing ?: return@post
                writing = null;writeTimer?.let(handler::removeCallbacks);writeTimer = null
                if (status != BluetoothGatt.GATT_SUCCESS) { write.result.error("ble_write", "GATT write failed; no command replay", null);disconnect("BLE write failed");return@post }
                write.result.success(null);nextWrite()
            } }
        }
        gatt = adapter!!.getRemoteDevice(address).connectGatt(activity, false, callback, BluetoothDevice.TRANSPORT_LE)
        connectTimer = Runnable { if (current == epoch && !ready) disconnect("Bluetooth connection/MTU/notification deadline") }.also { handler.postDelayed(it, 15000) }
    }
    private fun discover(owner: BluetoothGatt) { phase = "services"; if (!owner.discoverServices()) disconnect("Cannot discover BLE service") }
    private fun identityRead(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, status: Int, current: Int) { handler.post {
        if (gatt !== owner || current != epoch || phase != "info" || characteristic.uuid != INFO) return@post
        try {
            require(status == BluetoothGatt.GATT_SUCCESS && value.size in 2..512) { "BLE identity unavailable" }
            info = value.toString(Charsets.UTF_8);val identity = JSONObject(info)
            require(identity.optString("product") == "PakarController" &&
                identity.optString("board_id").matches(Regex("[!-~]{1,80}")) && identity.optString("boot_id").matches(Regex("[!-~]{1,80}"))) { "Not a compatible Pakar board" }
            check(identity.optBoolean("auth_required", true) == false) { "This board requires authentication; protected BLE control is unavailable" }
            val notify = tx ?: error("TX missing")
            check(owner.setCharacteristicNotification(notify, true)) { "Cannot enable BLE notifications" }
            val descriptor = notify.getDescriptor(CCCD) ?: error("Notification descriptor missing")
            phase = "cccd"
            if (Build.VERSION.SDK_INT >= 33) check(owner.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS)
            else { descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; check(owner.writeDescriptor(descriptor)) }
        } catch (error: Exception) { disconnect(error.message ?: "BLE identity rejected") }
    } }
    private fun notification(owner: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, current: Int) { handler.post {
        if (gatt === owner && current == epoch && ready && characteristic.uuid == TX && value.size in 7..minOf(mtu - 3, 512)) sink?.success(mapOf("type" to "data", "session" to epoch, "value" to value))
    } }
    private fun nextWrite() {
        if (writing != null || writes.isEmpty()) return
        val write = writes.removeFirst()
        if (!ready || write.epoch != epoch) { write.result.error("ble_stale", "Bluetooth owner retired", null);nextWrite();return }
        val owner = gatt
        val characteristic = rx
        if (owner == null || characteristic == null) { write.result.error("ble_stale", "Bluetooth owner unavailable", null);disconnect("Bluetooth owner unavailable");return }
        writing = write
        try {
            val accepted = if (Build.VERSION.SDK_INT >= 33) owner.writeCharacteristic(characteristic, write.bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
                else { characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;characteristic.value = write.bytes;owner.writeCharacteristic(characteristic) }
            check(accepted) { "GATT write was not admitted" }
            writeTimer = Runnable { if (writing === write) disconnect("GATT write deadline; no replay") }.also { handler.postDelayed(it, 4000) }
        } catch (_: Exception) { disconnect("GATT write unavailable; no replay") }
    }
    private fun disconnect(reason: String) {
        val retired = epoch;epoch++;ready = false;phase = "idle"
        connectTimer?.let(handler::removeCallbacks);connectTimer = null
        writeTimer?.let(handler::removeCallbacks);writeTimer = null
        pendingConnect?.error("ble_connection", reason, null);pendingConnect = null
        writing?.result?.error("ble_stale", reason, null);writing = null
        while (writes.isNotEmpty()) writes.removeFirst().result.error("ble_stale", reason, null)
        val old = gatt;gatt = null;rx = null;tx = null
        try { old?.disconnect() } catch (_: Exception) {} finally { old?.close() }
        sink?.success(mapOf("type" to "disconnected", "session" to retired, "reason" to reason))
    }
    fun setForeground(value: Boolean) { foreground = value;if (!value) finishScan("Scan paused") }
    fun close() {closed = true;setForeground(false);disconnect("Bluetooth activity destroyed");pendingPermission?.error("closed", "Bluetooth permission request retired", null);pendingPermission = null;channel.setMethodCallHandler(null);events.setStreamHandler(null);sink = null}
}
