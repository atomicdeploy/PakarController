package ir.digitalogic.pakar.pakarcontroller

import android.app.Activity
import android.app.ActivityManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInfo
import android.content.pm.PackageInstaller
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.Settings
import android.provider.OpenableColumns
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel
import java.io.File
import java.io.RandomAccessFile
import java.security.MessageDigest
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger
import java.util.zip.ZipFile

/** Adapted from PCController mobile UpgradeChannel.kt (actual bounded transfer/installer).
 * Self-package only, bounded chunks, same signer, newer version, OS consent.
 * No shell/root installation, third-party APKs or arbitrary paths. Self-removal
 * only opens Android's own consent screen after explicit local confirmation.
 */
class AppUpdateChannel(private val activity: Activity, messenger: BinaryMessenger) {
    companion object { var liveChannel: MethodChannel? = null }
    private val channel = MethodChannel(messenger, "ir.digitalogic.pakar/app-updates")
    private val io = Executors.newSingleThreadExecutor()
    private val waiting = AtomicInteger()
    private val root = File(activity.cacheDir, "peer-upgrades")
    @Volatile private var closed = false
    @Volatile private var foreground = false
    @Volatile private var transfer: Transfer? = null
    private var document: MethodChannel.Result? = null
    private val pickCode = 3041
    private val exportCode = 3042
    private var cachedExport: Pair<String, Map<String, Any>>? = null
    private val maximum = 128L * 1024 * 1024
    private val chunkSize = 16384
    private data class Transfer(val id: String, val file: File, val size: Long,
        val hash: String, var offset: Long = 0, var ready: Boolean = false,
        var last: Long = SystemClock.elapsedRealtime())
    init {
        liveChannel = channel
        // Cleanup now follows the explicit local policy through cleanup(); do
        // not sweep files unconditionally during activity construction.
        channel.setMethodCallHandler { call, result ->
            if (call.method == "notify") {
                try {
                    val code = call.argument<Number>("version_code")?.toLong() ?: 0L
                    val name = call.argument<String>("version_name") ?: ""
                    val hash = call.argument<String>("sha256") ?: ""
                    require(code > 0 && name.length <= 96 && hash.matches(Regex("[a-f0-9]{64}")))
                    val manager = activity.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                    val permitted = (Build.VERSION.SDK_INT < 33 || activity.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED) && manager.areNotificationsEnabled()
                    if (permitted) {
                        val channelId = "pakar_updates"
                        if (Build.VERSION.SDK_INT >= 26) manager.createNotificationChannel(NotificationChannel(channelId, "App updates", NotificationManager.IMPORTANCE_DEFAULT))
                        val intent = Intent(activity, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP)
                        val open = PendingIntent.getActivity(activity, 3043, intent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
                        @Suppress("DEPRECATION")
                        val builder = if (Build.VERSION.SDK_INT >= 26) Notification.Builder(activity, channelId) else Notification.Builder(activity)
                        val notice = builder
                            .setSmallIcon(android.R.drawable.stat_sys_download_done)
                            .setContentTitle("PakarController update available")
                            .setContentText("${name.ifEmpty { "New version" }} · build $code")
                            .setContentIntent(open).setAutoCancel(true).build()
                        manager.notify(3043, notice)
                    }
                    result.success(mapOf("posted" to permitted, "permission_required" to !permitted))
                } catch (_: Exception) { result.error("notification_rejected", "Update notification unavailable", null) }
            } else if (call.method == "notificationPermission") {
                try {
                    check(!closed && foreground) { "Return to the app first" }
                    if (Build.VERSION.SDK_INT >= 33 && activity.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                        activity.requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 3044)
                    } else {
                        activity.startActivity(Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, activity.packageName))
                    }
                    result.success(mapOf("requested" to true))
                } catch (_: Exception) { result.error("notification_rejected", "Notification settings unavailable", null) }
            } else if (call.method == "uninstall") {
                try {
                    check(!closed && foreground && !activity.isFinishing && document == null && waiting.get() == 0 && transfer == null) { "Finish current work before self-removal" }
                    require(call.argument<Boolean>("confirmed") == true) { "Explicit local confirmation required" }
                    activity.startActivity(Intent(Intent.ACTION_DELETE, Uri.parse("package:${activity.packageName}")))
                    result.success(mapOf("requested" to true, "os_consent_required" to true, "package" to activity.packageName))
                } catch (_: Exception) { result.error("uninstall_rejected", "Self-removal unavailable; nothing was removed", null) }
            } else if (call.method == "pick" || call.method == "export") {
                try {
                    check(!closed && document == null && waiting.get() == 0) { "Finish the current document operation" }
                    if (call.method == "pick") check(transfer == null) { "Discard the existing staged APK first" }
                    val intent = Intent(if (call.method == "pick") Intent.ACTION_OPEN_DOCUMENT else Intent.ACTION_CREATE_DOCUMENT).apply {
                        addCategory(Intent.CATEGORY_OPENABLE)
                        type = "application/vnd.android.package-archive"
                        if (call.method == "export") putExtra(Intent.EXTRA_TITLE, "PakarController.apk")
                    }
                    document = result
                    activity.startActivityForResult(intent, if (call.method == "pick") pickCode else exportCode)
                } catch (error: Exception) { document = null; result.error("document_failed", error.message, null) }
            } else if (waiting.incrementAndGet() > 8) {
                waiting.decrementAndGet(); result.error("busy", "Upgrade queue full", null)
            } else io.execute {
                try {
                    check(!closed) { "Upgrade channel closed" }
                    if (call.method != "cleanup") transfer?.let { if (SystemClock.elapsedRealtime() - it.last > 10 * 60 * 1000) abort() }
                    val args = call.arguments as? Map<*, *> ?: emptyMap<String, Any>()
                    val value = when (call.method) {
                        "info" -> exportInfo()
                        "peerInfo" -> peerInfo()
                        "read" -> read(args)
                        "begin" -> begin(args)
                        "write" -> write(args)
                        "finish" -> finish(args)
                        "abort" -> { requireTransfer(args); abort(); mapOf("aborted" to true) }
                        "status" -> status()
                        "cleanup" -> cleanup()
                        "install" -> install(args)
                        "permission" -> {
                            activity.runOnUiThread { if (!closed && Build.VERSION.SDK_INT >= 26) {
                                activity.startActivity(Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                    Uri.parse("package:${activity.packageName}")))
                            } }
                            status() + mapOf("requested" to true)
                        }
                        else -> error("Unknown upgrade method")
                    }
                    activity.runOnUiThread { if (!closed) result.success(value) }
                } catch (error: Exception) {
                    activity.runOnUiThread { if (!closed) result.error("upgrade_rejected", error.message ?: "Upgrade rejected", null) }
                } finally { waiting.decrementAndGet() }
            }
        }
    }
    fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean {
        if (requestCode != pickCode && requestCode != exportCode) return false
        val result = document ?: return true
        val uri = data?.data
        if (resultCode != Activity.RESULT_OK || uri == null) {
            document = null; result.success(null); return true
        }
        waiting.incrementAndGet()
        io.execute {
            try {
                check(!closed) { "Update channel closed" }
                val value = if (requestCode == exportCode) {
                    val file = installedApk()
                    activity.contentResolver.openOutputStream(uri, "w")?.use { output ->
                        file.inputStream().use { it.copyTo(output, 65536) }; output.flush()
                    } ?: error("Cannot create the selected export document")
                    mapOf("exported" to true, "bytes" to file.length(), "sha256" to digest(file))
                } else {
                    val size = activity.contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { cursor ->
                        if (cursor.moveToFirst() && !cursor.isNull(0)) cursor.getLong(0) else -1L
                    } ?: -1L
                    require(size in 1..maximum) { "Provider must report an APK length of 1 byte–128 MiB" }
                    begin(mapOf("bytes" to size, "sha256" to "0".repeat(64)), fromDocument = true)
                    val current = transfer ?: error("Staging unavailable")
                    try {
                        activity.contentResolver.openInputStream(uri)?.use { input ->
                            current.file.outputStream().use { output ->
                                val buffer = ByteArray(65536)
                                while (true) {
                                    check(!closed) { "APK selection retired" }
                                    val count = input.read(buffer); if (count < 0) break
                                    require(current.offset + count <= current.size) { "APK exceeds its declared size" }
                                    output.write(buffer, 0, count); current.offset += count
                                }
                            }
                        } ?: error("Cannot read selected APK")
                        // A local provider has no manifest hash; actual signer/package/version
                        // validation still applies and Android verifies the install again.
                        transfer = current.copy(hash = digest(current.file))
                        finish(mapOf("transfer_id" to current.id))
                    } catch (error: Exception) { abort(); throw error }
                }
                activity.runOnUiThread { document = null; if (!closed) result.success(value) }
            } catch (error: Exception) {
                activity.runOnUiThread { document = null; if (!closed) result.error("document_failed", error.message, null) }
            } finally { waiting.decrementAndGet() }
        }
        return true
    }
    @Suppress("DEPRECATION")
    private fun info(path: String? = null): PackageInfo {
        val flags = if (Build.VERSION.SDK_INT >= 28) PackageManager.GET_SIGNING_CERTIFICATES else PackageManager.GET_SIGNATURES
        return if (path == null) activity.packageManager.getPackageInfo(activity.packageName, flags)
        else activity.packageManager.getPackageArchiveInfo(path, flags) ?: error("Invalid APK")
    }
    @Suppress("DEPRECATION")
    private fun signers(value: PackageInfo): List<String> {
        val signatures = if (Build.VERSION.SDK_INT >= 28) value.signingInfo?.apkContentsSigners else value.signatures
        return (signatures ?: emptyArray()).map { hash(it.toByteArray()) }.sorted().also {
            require(it.isNotEmpty()) { "Package has no verified signer" }
        }
    }
    @Suppress("DEPRECATION")
    private fun version(value: PackageInfo): Long = if (Build.VERSION.SDK_INT >= 28) value.longVersionCode else value.versionCode.toLong()
    private fun hash(bytes: ByteArray): String = MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
    private fun digest(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input -> val buffer = ByteArray(65536); while (true) {
            val n = input.read(buffer); if (n < 0) break; digest.update(buffer, 0, n)
        } }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
    private fun installedApk(): File {
        require(activity.applicationInfo.splitSourceDirs.isNullOrEmpty()) { "Split installs cannot be exported as one APK" }
        return File(activity.applicationInfo.sourceDir).also { require(it.length() in 1..maximum) { "APK exceeds transfer bound" } }
    }
    private fun integer(args: Map<*, *>, key: String, fallback: Long = -1): Long {
        val value = args[key] ?: return fallback
        require(value is Int || value is Long) { "Expected an integer $key" }
        return (value as Number).toLong()
    }
    private fun archiveAbis(file: File): Set<String> {
        val abis = mutableSetOf<String>()
        ZipFile(file).use { zip ->
            var expanded = 0L; var count = 0
            for (entry in zip.entries().asSequence()) {
                require(++count <= 20000 && entry.size >= 0 && entry.size <= 512L * 1024 * 1024 - expanded) { "Invalid or oversized APK archive" }
                expanded += entry.size
                if (entry.name.startsWith("lib/") && entry.name.endsWith(".so")) abis.add(entry.name.split('/').getOrElse(1) { "" })
            }
        }
        return abis
    }
    private fun peerInfo(): Map<String, Any> {
        val current = activity.packageManager.getPackageInfo(activity.packageName, 0)
        val memory = ActivityManager.MemoryInfo()
        (activity.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(memory)
        return mapOf(
            "version" to (current.versionName ?: ""), "build" to version(current),
            "model" to Build.MODEL, "manufacturer" to Build.MANUFACTURER,
            "architecture" to (Build.SUPPORTED_ABIS.firstOrNull() ?: ""),
            "os_version" to Build.VERSION.RELEASE,
            "sdk" to Build.VERSION.SDK_INT, "memory_mb" to (memory.totalMem / (1024 * 1024)))
    }

    private fun exportInfo(): Map<String, Any> {
        val file = installedApk(); val key = "${file.path}:${file.length()}:${file.lastModified()}"
        cachedExport?.let { if (it.first == key) return it.second }
        val current = info()
        return mapOf("package" to activity.packageName, "version_code" to version(current),
            "version_name" to (current.versionName ?: ""), "bytes" to file.length(),
            "sha256" to digest(file), "signers" to signers(current), "chunk_bytes" to chunkSize,
            "abis" to archiveAbis(file).sorted(), "device_abis" to Build.SUPPORTED_ABIS.toList(),
            "minimum_sdk" to (if (Build.VERSION.SDK_INT >= 24) activity.applicationInfo.minSdkVersion else 1),
            "sdk" to Build.VERSION.SDK_INT).also { cachedExport = Pair(key, it) }
    }
    private fun read(args: Map<*, *>): Map<String, Any> {
        val file = installedApk(); val offset = integer(args, "offset")
        require(args["sha256"] == exportInfo()["sha256"]) { "Installed APK identity changed" }
        val requested = integer(args, "length", chunkSize.toLong())
        require(offset in 0..file.length() && requested in 1..chunkSize.toLong()) { "Invalid APK read range" }
        val length = requested.toInt()
        val bytes = ByteArray(minOf(length.toLong(), file.length() - offset).toInt())
        RandomAccessFile(file, "r").use { it.seek(offset); it.readFully(bytes) }
        return mapOf("offset" to offset, "bytes" to bytes, "eof" to (offset + bytes.size == file.length()))
    }
    private fun begin(args: Map<*, *>, fromDocument: Boolean = false): Map<String, Any> {
        require(document == null || fromDocument) { "Finish the open APK document picker first" }
        require(transfer == null) { "Finish or abort the existing transfer" }
        val size = integer(args, "bytes")
        val hash = args["sha256"] as? String ?: ""
        require(size in 1..maximum && hash.matches(Regex("[a-f0-9]{64}"))) { "Invalid APK manifest" }
        // Before 0% or file creation; installation needs an additional OS copy.
        require(activity.cacheDir.usableSpace >= size * 2 + 64 * 1024 * 1024) { "Insufficient space for APK and Android installation" }
        require(root.exists() || root.mkdirs()) { "Cannot create private staging" }
        val retained = root.listFiles()?.filter { it.name.matches(Regex("[a-f0-9-]{36}\\.apk")) }?.sumOf { it.length() } ?: 0
        require(retained + size <= maximum) { "Interrupted APK cache reaches its 128 MiB budget; clear this app's cache or wait for expiry" }
        val id = UUID.randomUUID().toString(); val file = File(root, "$id.apk")
        require(file.createNewFile()) { "Cannot create private transfer" }
        transfer = Transfer(id, file, size, hash)
        return status()
    }
    private fun requireTransfer(args: Map<*, *>): Transfer = (transfer ?: error("No staged APK")).also {
        require(args["transfer_id"] == it.id) { "Transfer identity changed" }; it.last = SystemClock.elapsedRealtime()
    }
    private fun write(args: Map<*, *>): Map<String, Any> {
        val current = requireTransfer(args)
        val offset = integer(args, "offset")
        val bytes = args["bytes"] as? ByteArray ?: error("Missing APK bytes")
        require(!current.ready && offset == current.offset && bytes.size in 1..chunkSize &&
            offset + bytes.size <= current.size) { "Invalid or replayed APK chunk; query status, do not retry blindly" }
        RandomAccessFile(current.file, "rw").use { it.seek(offset); it.write(bytes) }
        current.offset += bytes.size
        return status()
    }
    private fun finish(args: Map<*, *>): Map<String, Any> {
        val current = requireTransfer(args)
        require(current.offset == current.size && current.file.length() == current.size) { "Incomplete APK" }
        try {
            require(digest(current.file) == current.hash) { "APK digest mismatch" }
            val archive = info(current.file.path); val installed = info()
            require(archive.packageName == activity.packageName) { "Wrong application package" }
            require(signers(archive) == signers(installed)) { "APK signer differs from installed app" }
            require(version(archive) > version(installed)) { "APK is not newer than installed app" }
            if (Build.VERSION.SDK_INT >= 24) require((archive.applicationInfo ?: error("APK missing application manifest")).minSdkVersion <= Build.VERSION.SDK_INT) { "APK requires a newer Android version" }
            val abis = archiveAbis(current.file)
            require(abis.isEmpty() || Build.SUPPORTED_ABIS.any { it in abis }) { "APK does not support this CPU" }
            current.ready = true
            return status() + mapOf("package" to archive.packageName, "version_code" to version(archive), "version_name" to (archive.versionName ?: ""))
        } catch (error: Exception) { abort(); throw error }
    }
    private fun install(args: Map<*, *>): Map<String, Any> {
        check(foreground && !closed) { "Android installation requires this app in the foreground" }
        val current = requireTransfer(args); require(current.ready) { "APK has not been verified" }
        finish(args) // Recheck staged bytes/identity just before Android takes ownership.
        if (Build.VERSION.SDK_INT >= 26 && !activity.packageManager.canRequestPackageInstalls())
            return status() + mapOf("state" to "permission_required")
        val installer = activity.packageManager.packageInstaller
        val params = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL).apply {
            setAppPackageName(activity.packageName); setSize(current.size)
            if (Build.VERSION.SDK_INT >= 31) setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_REQUIRED)
        }
        val sessionId = installer.createSession(params)
        try {
            installer.openSession(sessionId).use { session ->
                session.openWrite("base.apk", 0, current.size).use { output ->
                    current.file.inputStream().use { it.copyTo(output, 65536) }; session.fsync(output)
                }
                val intent = Intent(activity, AppUpdateResultReceiver::class.java).putExtra("session_id", sessionId)
                val flags = PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= 31) PendingIntent.FLAG_MUTABLE else 0)
                val callback = PendingIntent.getBroadcast(activity, sessionId, intent, flags)
                val expected = org.json.JSONObject().put("session_id", sessionId)
                    .put("sha256", current.hash).put("version_code", version(info(current.file.path)))
                require(activity.getSharedPreferences("peer-upgrades", Context.MODE_PRIVATE).edit()
                    .putString("expected", expected.toString()).remove("receipt").commit()) { "Cannot persist installation identity" }
                check(foreground && !closed) { "Installation request retired before Android commit" }
                session.commit(callback.intentSender)
            }
        } catch (error: Exception) { installer.abandonSession(sessionId); throw error }
        abort() // Android now owns its one required installation copy.
        return mapOf("state" to "awaiting_android", "session_id" to sessionId)
    }
    private fun status(): Map<String, Any> {
        val current = transfer
        return if (current == null) {
            val raw = activity.getSharedPreferences("peer-upgrades", Context.MODE_PRIVATE).getString("receipt", "") ?: ""
            val receipt = try { org.json.JSONObject(raw) } catch (_: Exception) { null }
            if (receipt != null && receipt.optInt("status", -99) == PackageInstaller.STATUS_SUCCESS) {
                val installed = exportInfo()
                receipt.put("running_verified", receipt.optString("sha256") == installed["sha256"] &&
                    receipt.optLong("version_code", -1) == installed["version_code"])
            }
            mapOf("state" to "idle", "install_status" to (receipt?.toString() ?: raw))
        }
        else mapOf("state" to if (current.ready) "verified" else "receiving", "transfer_id" to current.id,
            "bytes" to current.size, "received" to current.offset, "sha256" to current.hash,
            "install_permission" to (Build.VERSION.SDK_INT < 26 || activity.packageManager.canRequestPackageInstalls()))
    }
    private fun abort() { transfer?.file?.delete(); transfer = null }
    private fun cleanup(): Map<String, Any> {
        check(document == null) { "Finish the document operation before cleanup" }
        val canonical = root.canonicalFile
        require(canonical.parentFile == activity.cacheDir.canonicalFile && canonical.name == "peer-upgrades") { "Private staging owner mismatch" }
        var removed = 0; var bytes = 0L; var failed = 0
        val now = System.currentTimeMillis()
        // Bounded, old, regular files owned by this module only. Never the live
        // transfer, installed package, preferences, Android install copy or a symlink.
        for (file in (root.listFiles() ?: emptyArray()).take(128)) {
            if (file.name.matches(Regex("[a-f0-9-]{36}\\.apk")) && file.isFile &&
                file.canonicalFile.parentFile == canonical && file.canonicalFile == File(canonical, file.name).absoluteFile &&
                file != transfer?.file && now - file.lastModified() > 30 * 60 * 1000L) {
                val size = file.length()
                if (size < 0 || size > maximum - bytes) continue
                if (file.delete()) { removed++; bytes += size } else failed++
            }
        }
        return mapOf("removed_files" to removed, "removed_bytes" to bytes, "failed_files" to failed, "active_preserved" to (transfer != null))
    }
    fun setForeground(value: Boolean) { foreground = value }
    fun close() {
        closed = true
        document?.error("closed", "APK picker closed; no action will be replayed", null); document = null
        if (liveChannel === channel) liveChannel = null
        channel.setMethodCallHandler(null); io.execute { abort() }; io.shutdown()
    }
}

/** The platform verifies and asks the human; a committed session is not success. */
class AppUpdateResultReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE)
        val receipt = org.json.JSONObject().put("session_id", intent.getIntExtra("session_id", -1))
            .put("status", status).put("message", intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE) ?: "")
        val preferences = context.getSharedPreferences("peer-upgrades", Context.MODE_PRIVATE)
        val expected = try { org.json.JSONObject(preferences.getString("expected", "") ?: "") } catch (_: Exception) { null }
        if (expected != null && expected.optInt("session_id", -2) == intent.getIntExtra("session_id", -1)) {
            receipt.put("sha256", expected.optString("sha256")).put("version_code", expected.optLong("version_code"))
        }
        preferences.edit().putString("receipt", receipt.toString()).apply()
        Handler(Looper.getMainLooper()).post {
            AppUpdateChannel.liveChannel?.invokeMethod("installStatus", mapOf("session_id" to intent.getIntExtra("session_id", -1), "status" to status,
                "message" to (intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE) ?: "")))
        }
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            @Suppress("DEPRECATION") val confirmation = intent.getParcelableExtra<Intent>(Intent.EXTRA_INTENT)
            try { confirmation?.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)?.let { context.startActivity(it) } }
            catch (_: Exception) { /* Receipt remains pending, never claim installed. */ }
        }
    }
}
