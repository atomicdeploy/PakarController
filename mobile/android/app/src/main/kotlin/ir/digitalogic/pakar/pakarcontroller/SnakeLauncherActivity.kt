package ir.digitalogic.pakar.pakarcontroller

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Toast

/** External, removable game. ADB reverse can expose its loopback host to Android.
 * Opening this activity cannot connect a board or start a round. */
class SnakeLauncherActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (intent.action == Intent.ACTION_VIEW &&
            intent.dataString == "pakarcontroller://mini-app/snake") {
            try {
                startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("http://127.0.0.1:18490/snake")))
            } catch (_: ActivityNotFoundException) {
                Toast.makeText(this, "Install a browser to open the Snake mini-app", Toast.LENGTH_LONG).show()
            }
        }
        finish()
    }
}
