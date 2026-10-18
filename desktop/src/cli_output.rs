//! Actual Pealayer src/cli.rs PrintHelp/PrintVersion/format_help_message
//! branches adapted to this controller. These terminate before preferences,
//! networking, GUI ownership or windows are initialized. Media/update/remote
//! command parsing is not silently certified by this local-output import.
pub const DEVICE_COMMANDS: &str = "status | info | telemetry | firmware status | firmware fetch HTTPS_URL SHA256 [all|esp|avr] | firmware apply JOB BOOT SHA256 | firmware cancel JOB BOOT SHA256 | device name | device rename NAME[.local] | auth info | auth change USERNAME (PAKAR_NEW_PASSWORD) | avr contracts | avr settings | avr set PARAMETER VALUE | avr panel | avr command OPCODE [HEX] | mode <...> | cell <id> <state> [effect] | brightness <n> | led-output on|off|static|effects | effect <name> | display <n> | display stop | display segments [--speed-ms n] [--duration-ms n] [--repeat once|loop|interval] [--interval-ms n] [--scroll] -- TEXT | audio <cue> | snake <direction> | reset | quit";

pub fn format_help_message() -> String {
    let mut output=format!(
        "PakarController {} - Board control workspace\n\n\
USAGE:\n  PakarController [APPLICATION_OPTION]\n  PakarController --status\n  PakarController --command COMMAND [ARGUMENTS...]\n  PakarController --tui\n\n\
APPLICATION OPTIONS:\n  -h, --help         Print help and exit without starting an app\n  -V, --version      Print software identity and exit\n  --app-status       Read the existing app IPC state; no new app\n  --quit             Gracefully close the existing app\n  --minimize         Minimize the existing app (launch without flags to restore)\n  --preferences      Open application Preferences\n  --device-settings  Open acknowledged device settings\n  --connect-dialog   Open connection controls\n  --screenshot PATH  Request an existing app framebuffer capture\n\n\
DEVICE COMMANDS (only advertised capabilities are available):\n  {}\n\n\
ENDPOINT/AUTH:\n  PAKAR_ENDPOINT, PAKAR_USER, PAKAR_PASSWORD configure device access.\n  Keep credentials out of command arguments and logs.\n  REST uses /api/... without numbered namespaces; WebSocket uses /ws.\n",
        format!("{} (build {})",env!("CARGO_PKG_VERSION"),crate::client_management::build()),
        format!("{DEVICE_COMMANDS}\n\n  Atomic selection: --command cells IDS white|red|off [immediate|fade|pulse]\n  Existing app (selected USB or Wi-Fi): --led-cells IDS STATE [EFFECT]\n  Atomic batches require cell-batch capability; no per-cell fallback.\n  Existing app (selected USB or Wi-Fi only): --led-output on|off|static|effects\n  Rainbow test: --led-effect rainbow (keeps the current output mode)\n  Manual physical pixel diagnostic (selected Wi-Fi only): --pixel-arm, then --pixel-show 1..480\n  Pixel diagnostics send one complete frame per explicit action; no automatic stepping/replay.")
    );
    output.push_str("\nAPP MANAGEMENT:\n  Preferences → App updates configures app updates and enrollment separately from board firmware.\n  --preferences-page updates opens that page in the existing application.\n  --client-update-init initializes the current canonical managed package and existing profile only.\n  Initialization belongs to the approved installer/deployment workflow, not arbitrary target paths.\n");
    output
}

pub fn parse_cli_output(arguments: &[String]) -> Option<String> {
    match arguments.get(1).map(String::as_str) {
        Some("-h" | "--help") => Some(format_help_message()),
        Some("-V" | "--version") => Some(format!("PakarController {}", env!("CARGO_PKG_VERSION"))),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn actual_donor_short_and_long_help_version_flags() {
        for option in ["-h", "--help"] {
            let output = parse_cli_output(&["PakarController.exe".into(), option.into()]).unwrap();
            assert!(output.contains("PakarController") && output.contains(DEVICE_COMMANDS));
        }
        for option in ["-V", "--version"] {
            assert_eq!(
                parse_cli_output(&["PakarController.exe".into(), option.into()]).unwrap(),
                format!("PakarController {}", env!("CARGO_PKG_VERSION"))
            );
        }
    }
    #[test]
    fn device_payload_is_not_interpreted_as_a_local_option() {
        assert!(
            parse_cli_output(&[
                "PakarController.exe".into(),
                "--command".into(),
                "display".into(),
                "--help".into()
            ])
            .is_none()
        );
        assert!(parse_cli_output(&["PakarController.exe".into()]).is_none());
        assert!(parse_cli_output(&[]).is_none());
    }
}
