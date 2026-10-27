//! Pealayer src/main.rs + src/diagnostics.rs console-free GUI / fallible output.
//! Explicit CLI/TUI launches may join the caller's existing terminal, never
//! create a console. GUI launches and automatic updater helpers never attach.
use std::io::Write;

macro_rules! cli_println {
    () => { crate::cli_console::output(format_args!(""), false, true) };
    ($($argument:tt)*) => { crate::cli_console::output(format_args!($($argument)*), false, true) };
}
macro_rules! cli_eprintln {
    ($($argument:tt)*) => { crate::cli_console::output(format_args!($($argument)*), true, true) };
}
macro_rules! cli_print {
    ($($argument:tt)*) => { crate::cli_console::output(format_args!($($argument)*), false, false) };
}

pub fn output(arguments: std::fmt::Arguments<'_>, error: bool, newline: bool) {
    // Imported write_cli_line policy: a missing/closed inherited pipe must not
    // turn a successful command or orderly GUI shutdown into a panic.
    if error {
        let _ = write_output(&mut std::io::stderr(), arguments, newline);
    } else {
        let _ = write_output(&mut std::io::stdout(), arguments, newline);
    }
}
fn write_output(
    writer: &mut impl Write,
    arguments: std::fmt::Arguments<'_>,
    newline: bool,
) -> std::io::Result<()> {
    if newline {
        writeln!(writer, "{arguments}")
    } else {
        write!(writer, "{arguments}")
    }
}

fn terminal_requested(arguments: &[String]) -> bool {
    matches!(
        arguments.get(1).map(String::as_str),
        Some(
            "-h" | "--help"
                | "-V"
                | "--version"
                | "--status"
                | "--command"
                | "--tui"
                | "--app-status"
                | "--quit"
                | "--wifi"
                | "--usb"
                | "--disconnect"
                | "--led-effect"
                | "--led-mode"
                | "--led-cell"
                | "--led-cells"
                | "--led-output"
                | "--cell-backgrounds"
                | "--update-esp"
                | "--pixel-arm"
                | "--pixel-show"
                | "--client-enroll"
                | "--client-management-config"
                | "--client-update-init"
        )
    )
}

pub fn prepare(arguments: &[String]) {
    if !terminal_requested(arguments) {
        return;
    }
    #[cfg(windows)]
    unsafe {
        use windows::Win32::{
            Storage::FileSystem::{FILE_TYPE_DISK, FILE_TYPE_PIPE, GetFileType},
            System::Console::{
                ATTACH_PARENT_PROCESS, AttachConsole, GetStdHandle, STD_ERROR_HANDLE,
                STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, SetStdHandle,
            },
        };
        // AttachConsole can replace standard handles. Preserve every inherited
        // file/pipe, including enrollment stdin and IPC/updater redirected JSON.
        // These are borrowed handles: never close them or allocate a console.
        let redirected = [STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE].map(|kind| {
            (
                kind,
                GetStdHandle(kind).ok().filter(|handle| {
                    !handle.is_invalid()
                        && matches!(GetFileType(*handle), FILE_TYPE_DISK | FILE_TYPE_PIPE)
                }),
            )
        });
        if AttachConsole(ATTACH_PARENT_PROCESS).is_ok() {
            for (kind, handle) in redirected {
                if let Some(handle) = handle {
                    let _ = SetStdHandle(kind, handle);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn only_explicit_text_actions_can_attach_to_an_existing_terminal() {
        for option in [
            "--help",
            "--version",
            "--app-status",
            "--command",
            "--tui",
            "--client-enroll",
            "--led-cells",
            "--cell-backgrounds",
        ] {
            assert!(terminal_requested(&["app.exe".into(), option.into()]));
        }
        for option in [
            "--preferences",
            "--connect-dialog",
            "--apply-client-update",
            "--screenshot",
            "--minimize",
        ] {
            assert!(!terminal_requested(&["app.exe".into(), option.into()]));
        }
        assert!(!terminal_requested(&["app.exe".into()]));
        assert!(!terminal_requested(&[]));
    }
    #[test]
    fn inherited_output_and_partial_tui_lines_are_preserved() {
        let mut bytes = Vec::new();
        write_output(&mut bytes, format_args!("{}", "PakarController"), false).unwrap();
        write_output(&mut bytes, format_args!(" {}", 42), true).unwrap();
        assert_eq!(bytes, b"PakarController 42\n");
    }
    #[test]
    fn disconnected_pipe_returns_an_error_without_panicking() {
        struct ClosedPipe;
        impl Write for ClosedPipe {
            fn write(&mut self, _: &[u8]) -> std::io::Result<usize> {
                Err(std::io::ErrorKind::BrokenPipe.into())
            }
            fn flush(&mut self) -> std::io::Result<()> {
                Ok(())
            }
        }
        assert_eq!(
            write_output(&mut ClosedPipe, format_args!("progress"), true)
                .unwrap_err()
                .kind(),
            std::io::ErrorKind::BrokenPipe
        );
    }
    #[test]
    fn native_identity_has_no_vendor_prefix() {
        assert_eq!(crate::application_identity::APP_NAME, "PakarController");
        assert_eq!(
            crate::application_identity::WINDOWS_APP_ID,
            "PakarController"
        );
    }
}
