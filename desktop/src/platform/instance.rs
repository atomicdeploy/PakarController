//! Adapted from Pealayer's windows.rs GUI ownership and interop.rs named pipe.
//! The owner mutex closes the startup race; secondary launches forward typed
//! requests to the primary and exit, even while the primary window is hidden.
use serde::{Deserialize, Serialize};
use std::{
    sync::{
        Arc, Mutex,
        mpsc::{self, Receiver, Sender},
    },
    time::Duration,
};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(tag = "command", rename_all = "snake_case", deny_unknown_fields)]
pub enum Command {
    DeviceSettings,
    DeviceSettingsPage {
        page: String,
    },
    Show,
    Minimize,
    Connect,
    Preferences,
    PreferencesPage {
        page: String,
    },
    CellBackgrounds {
        value: String,
    },
    Refresh,
    Fullscreen,
    About,
    Workspaces,
    Panel {
        name: String,
    },
    FindCell {
        query: String,
    },
    CellManager,
    Screenshot {
        path: String,
    },
    Quit,
    Status {
        #[serde(default)]
        pixels: bool,
    },
    NotificationTest,
    UseWifi {
        endpoint: Option<String>,
    },
    UseUsb {
        port: Option<String>,
    },
    Disconnect,
    LedEffect {
        value: String,
    },
    LedOutput {
        value: String,
    },
    PixelArm,
    PixelShow {
        index: u16,
    },
    LedMode {
        value: String,
    },
    LedCell {
        id: u8,
        state: String,
        effect: String,
    },
    LedCells {
        ids: String,
        state: String,
        effect: String,
    },
    UpdateEsp {
        path: String,
        confirmed: bool,
    },
}
fn parse_command(bytes: &[u8]) -> Result<Command, String> {
    let value: serde_json::Value = serde_json::from_slice(bytes).map_err(|e| e.to_string())?;
    let object = value.as_object().ok_or("Expected a command object")?;
    let allowed = match object.get("command").and_then(|v| v.as_str()) {
        Some("status") => &["command", "pixels"][..],
        Some("panel") => &["command", "name"][..],
        Some("find_cell") => &["command", "query"][..],
        Some("screenshot") => &["command", "path"][..],
        Some("device_settings_page") => &["command", "page"][..],
        Some("preferences_page") => &["command", "page"][..],
        Some("cell_backgrounds") => &["command", "value"][..],
        Some("use_wifi") => &["command", "endpoint"][..],
        Some("use_usb") => &["command", "port"][..],
        Some("led_effect") | Some("led_mode") | Some("led_output") => &["command", "value"][..],
        Some("led_cell") => &["command", "id", "state", "effect"][..],
        Some("led_cells") => &["command", "ids", "state", "effect"][..],
        Some("pixel_show") => &["command", "index"][..],
        Some("update_esp") => &["command", "path", "confirmed"][..],
        _ => &["command"][..],
    };
    if object.keys().any(|key| !allowed.contains(&key.as_str())) {
        return Err("Unknown command field".into());
    }
    let command: Command = serde_json::from_value(value).map_err(|e| e.to_string())?;
    match &command {
        Command::CellBackgrounds { value } if !matches!(value.as_str(), "on" | "off") => {
            return Err("Cell backgrounds require on or off".into());
        }
        Command::UpdateEsp { path, confirmed }
            if !confirmed
                || !std::path::Path::new(path).is_absolute()
                || path.len() > 2048
                || path.chars().any(char::is_control) =>
        {
            return Err("An absolute firmware path and explicit confirmation are required".into());
        }
        Command::UseWifi {
            endpoint: Some(value),
        } => {
            let url = reqwest::Url::parse(value).map_err(|_| "Invalid Wi-Fi endpoint")?;
            if value.len() > 512
                || !matches!(url.scheme(), "http" | "https")
                || url.host_str().is_none()
                || !url.username().is_empty()
                || url.password().is_some()
                || url.query().is_some()
                || url.fragment().is_some()
                || !matches!(url.path(), "" | "/")
            {
                return Err(
                    "Expected http(s)://hostname[:port] without credentials or a path".into(),
                );
            }
        }
        Command::UseUsb { port: Some(value) }
            if value.trim().is_empty()
                || value.len() > 512
                || value.chars().any(char::is_control) =>
        {
            return Err("Select a valid OS serial port/path".into());
        }
        Command::LedEffect { value } if !matches!(value.as_str(), "none" | "rainbow" | "fire") => {
            return Err("Unknown LED effect".into());
        }
        Command::LedOutput { value }
            if !matches!(value.as_str(), "on" | "off" | "static" | "effects" | "gpio-static" | "rmt-dma") =>
        {
            return Err("LED output requires on, off, static, effects, gpio-static or rmt-dma".into());
        }
        Command::PixelShow { index } if !(1..=480).contains(index) => {
            return Err("Physical pixel requires 1..480".into());
        }
        Command::LedMode { value } if !matches!(value.as_str(), "cell" | "snake") => {
            return Err("Unknown LED mode".into());
        }
        Command::LedCells { ids, state, effect } => {
            crate::cell_batch::Request::new(&crate::ui::cell_search::parse(ids)?, state, effect)?;
        }
        Command::LedCell { id, state, effect }
            if !(1..=80).contains(id)
                || !matches!(state.as_str(), "white" | "red" | "off")
                || !matches!(effect.as_str(), "immediate" | "fade" | "pulse") =>
        {
            return Err("Invalid LED cell operation".into());
        }
        _ => {}
    }
    if let Command::Screenshot { path } = &command {
        crate::ui::screenshot::validate_path(path)?;
    }
    if let Command::FindCell { query } = &command {
        crate::ui::cell_search::parse(query)?;
    }
    if let Command::DeviceSettingsPage { page } = &command {
        if crate::ui::device_settings::page_index(page).is_none() {
            return Err("Unknown device settings page".into());
        }
    }
    if let Command::PreferencesPage { page } = &command {
        if crate::ui::preferences::page_index(page).is_none() {
            return Err("Unknown preferences page".into());
        }
    }
    Ok(command)
}
pub struct Control {
    pub rx: Receiver<Command>,
    pub snapshot: Arc<Mutex<serde_json::Value>>,
    pub demand: Arc<crate::usb_demand::Watchers>,
}
#[cfg(windows)]
pub struct GuiOwnershipGuard(windows::Win32::Foundation::HANDLE);
#[cfg(windows)]
impl Drop for GuiOwnershipGuard {
    fn drop(&mut self) {
        unsafe {
            let _ = windows::Win32::Foundation::CloseHandle(self.0);
        }
    }
}
#[cfg(not(windows))]
pub struct GuiOwnershipGuard;
pub enum GuiOwnership {
    Primary(GuiOwnershipGuard),
    Existing,
}
fn identity_hash(identity: &str) -> u64 {
    identity
        .trim()
        .to_lowercase()
        .as_bytes()
        .iter()
        .fold(0xcbf29ce484222325_u64, |hash, byte| {
            (hash ^ u64::from(*byte)).wrapping_mul(0x100000001b3)
        })
}
#[cfg(windows)]
pub fn current_session_id() -> Result<u32, String> {
    use windows::Win32::System::{
        RemoteDesktop::ProcessIdToSessionId, Threading::GetCurrentProcessId,
    };
    let mut session = 0;
    unsafe { ProcessIdToSessionId(GetCurrentProcessId(), &mut session) }
        .map_err(|e| e.to_string())?;
    Ok(session)
}
#[cfg(windows)]
fn pipe_name() -> String {
    format!(
        r"\\.\pipe\PakarController.Control.{:016x}.{}",
        identity_hash("PakarController"),
        current_session_id().unwrap_or_default()
    )
}
#[cfg(windows)]
pub fn acquire_gui_ownership() -> Result<GuiOwnership, String> {
    use windows::{
        Win32::{
            Foundation::{CloseHandle, ERROR_ALREADY_EXISTS, GetLastError},
            System::Threading::CreateMutexW,
        },
        core::PCWSTR,
    };
    let name = format!(
        "Local\\PakarController.GuiOwner.{:016x}",
        identity_hash("PakarController")
    );
    let wide: Vec<u16> = name.encode_utf16().chain(Some(0)).collect();
    let handle =
        unsafe { CreateMutexW(None, false, PCWSTR(wide.as_ptr())) }.map_err(|e| e.to_string())?;
    if unsafe { GetLastError() } == ERROR_ALREADY_EXISTS {
        unsafe {
            let _ = CloseHandle(handle);
        }
        Ok(GuiOwnership::Existing)
    } else {
        Ok(GuiOwnership::Primary(GuiOwnershipGuard(handle)))
    }
}
#[cfg(not(windows))]
pub fn acquire_gui_ownership() -> Result<GuiOwnership, String> {
    Ok(GuiOwnership::Primary(GuiOwnershipGuard))
}
#[cfg(windows)]
pub fn send(command: &Command, timeout: Duration) -> Result<serde_json::Value, String> {
    use windows::{
        Win32::{Foundation::GetLastError, System::Pipes::CallNamedPipeW},
        core::PCWSTR,
    };
    let name: Vec<u16> = pipe_name().encode_utf16().chain(Some(0)).collect();
    let request = serde_json::to_vec(command).map_err(|e| e.to_string())?;
    let mut buffer = vec![0u8; 65536];
    let mut read = 0;
    let ok = unsafe {
        CallNamedPipeW(
            PCWSTR(name.as_ptr()),
            Some(request.as_ptr().cast()),
            request.len() as u32,
            Some(buffer.as_mut_ptr().cast()),
            buffer.len() as u32,
            &mut read,
            timeout.as_millis().clamp(1, u128::from(u32::MAX)) as u32,
        )
    };
    if !ok.as_bool() {
        return Err(format!("PakarController IPC unavailable: {:?}", unsafe {
            GetLastError()
        }));
    }
    serde_json::from_slice(&buffer[..read as usize]).map_err(|e| e.to_string())
}
#[cfg(not(windows))]
pub fn send(_: &Command, _: Duration) -> Result<serde_json::Value, String> {
    Err("Native instance IPC is currently available on Windows".into())
}
pub fn start(ctx: eframe::egui::Context, enable_listener: bool) -> Control {
    let (tx, rx) = mpsc::channel();
    let snapshot = Arc::new(Mutex::new(
        serde_json::json!({"process_id":std::process::id(),"application":"PakarController","ready":true}),
    ));
    let demand = Arc::new(crate::usb_demand::Watchers::default());
    #[cfg(windows)]
    if enable_listener {
        listener(tx, ctx, snapshot.clone(), demand.clone());
    }
    #[cfg(not(windows))]
    let _ = (tx, ctx, enable_listener);
    Control {
        rx,
        snapshot,
        demand,
    }
}
#[cfg(windows)]
fn listener(
    tx: Sender<Command>,
    ctx: eframe::egui::Context,
    snapshot: Arc<Mutex<serde_json::Value>>,
    demand: Arc<crate::usb_demand::Watchers>,
) {
    use windows::{
        Win32::{
            Foundation::{CloseHandle, ERROR_PIPE_CONNECTED, GetLastError},
            Storage::FileSystem::{FlushFileBuffers, PIPE_ACCESS_DUPLEX, ReadFile, WriteFile},
            System::Pipes::{
                ConnectNamedPipe, CreateNamedPipeW, DisconnectNamedPipe,
                GetNamedPipeClientProcessId, PIPE_READMODE_MESSAGE, PIPE_REJECT_REMOTE_CLIENTS,
                PIPE_TYPE_MESSAGE, PIPE_WAIT,
            },
            System::RemoteDesktop::ProcessIdToSessionId,
        },
        core::PCWSTR,
    };
    let name: Vec<u16> = pipe_name().encode_utf16().chain(Some(0)).collect();
    let expected_session = current_session_id().ok();
    std::thread::Builder::new().name("pakar-native-ipc".into()).spawn(move || loop {
        let handle=unsafe {CreateNamedPipeW(PCWSTR(name.as_ptr()),PIPE_ACCESS_DUPLEX,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,1000,None)};
        if handle.is_invalid(){break;}
        let connected=unsafe {ConnectNamedPipe(handle,None)}.is_ok() || unsafe {GetLastError()}==ERROR_PIPE_CONNECTED;
        if connected {
            let mut pid=0;let mut session=0;
            let same_session=unsafe {GetNamedPipeClientProcessId(handle,&mut pid)}.is_ok() && unsafe {ProcessIdToSessionId(pid,&mut session)}.is_ok() && expected_session==Some(session);
            let mut bytes=vec![0u8;65536];let mut read=0;
            if same_session && unsafe {ReadFile(handle,Some(&mut bytes),Some(&mut read),None)}.is_ok() {
                let result=parse_command(&bytes[..read as usize]);
                let response=match result {
                    Ok(Command::Status{pixels})=>{demand.touch(crate::usb_demand::Demand{flags:crate::usb_demand::STATUS|if pixels{crate::usb_demand::PIXELS}else{0},table:true});ctx.request_repaint();snapshot.lock().map(|s|s.clone()).unwrap_or_default()},
                    Ok(command)=>{
                        let valid=!matches!(&command,Command::Panel{name} if crate::ui::layout::Panel::from_name(name).is_none());
                        if valid && tx.send(command).is_ok(){ctx.request_repaint();serde_json::json!({"accepted":true,"process_id":std::process::id()})}
                        else {serde_json::json!({"accepted":false,"error":"Unknown panel or closed application"})}
                    }
                    Err(error)=>serde_json::json!({"accepted":false,"error":error.to_string()}),
                };
                let reply=serde_json::to_vec(&response).unwrap_or_default();let mut written=0;
                unsafe {let _=WriteFile(handle,Some(&reply),Some(&mut written),None);let _=FlushFileBuffers(handle);}
            }
            unsafe {let _=DisconnectNamedPipe(handle);}
        }
        unsafe {let _=CloseHandle(handle);}
    }).expect("native IPC listener");
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn connection_and_led_ipc_are_explicit_typed_and_bounded() {
        assert!(matches!(
            parse_command(br#"{"command":"status"}"#),
            Ok(Command::Status { pixels: false })
        ));
        assert!(matches!(
            parse_command(br#"{"command":"status","pixels":true}"#),
            Ok(Command::Status { pixels: true })
        ));
        assert!(parse_command(br#"{"command":"status","pixels":"true"}"#).is_err());
        for bytes in [
            br#"{"command":"disconnect"}"#.as_slice(),
            br#"{"command":"use_usb","port":"COM22"}"#,
            br#"{"command":"use_wifi","endpoint":"http://ledboard.local"}"#,
            br#"{"command":"led_effect","value":"rainbow"}"#,
            br#"{"command":"led_output","value":"on"}"#,
            br#"{"command":"led_output","value":"off"}"#,
            br#"{"command":"led_output","value":"static"}"#,
            br#"{"command":"led_output","value":"effects"}"#,
            br#"{"command":"pixel_arm"}"#,
            br#"{"command":"pixel_show","index":1}"#,
            br#"{"command":"pixel_show","index":480}"#,
            br#"{"command":"led_cell","id":80,"state":"red","effect":"fade"}"#,
            br#"{"command":"led_cells","ids":"1,8-12,80","state":"red","effect":"fade"}"#,
        ] {
            assert!(parse_command(bytes).is_ok());
        }
        for bytes in [
            br#"{"command":"use_wifi","endpoint":"http://user:secret@host"}"#.as_slice(),
            br#"{"command":"use_wifi","endpoint":"http://ledboard.local/api/status"}"#,
            br#"{"command":"use_usb","port":""}"#,
            br#"{"command":"led_effect","value":"unknown"}"#,
            br#"{"command":"led_output","value":"toggle"}"#,
            br#"{"command":"pixel_show","index":0}"#,
            br#"{"command":"pixel_show","index":481}"#,
            br#"{"command":"led_output","value":true}"#,
            br#"{"command":"led_mode","value":"idle"}"#,
            br#"{"command":"led_cell","id":0,"state":"red","effect":"fade"}"#,
            br#"{"command":"led_cell","id":1,"state":"purple","effect":"fade"}"#,
            br#"{"command":"led_cells","ids":"1,81","state":"red","effect":"fade"}"#,
            br#"{"command":"led_cells","ids":"","state":"red","effect":"fade"}"#,
            br#"{"command":"update_esp","path":"relative.bin","confirmed":true}"#,
            br#"{"command":"disconnect","endpoint":"other"}"#,
        ] {
            assert!(parse_command(bytes).is_err());
        }
    }
    #[test]
    fn launch_requests_use_only_known_typed_commands() {
        assert!(parse_command(br#"{"command":"cell_backgrounds","value":"on"}"#).is_ok());
        assert!(parse_command(br#"{"command":"cell_backgrounds","value":"off"}"#).is_ok());
        for bytes in [br#"{"command":"cell_backgrounds","value":"toggle"}"#.as_slice(),
            br#"{"command":"cell_backgrounds","value":true}"#,
            br#"{"command":"cell_backgrounds"}"#,
            br#"{"command":"cell_backgrounds","value":"on","board":true}"#] {
            assert!(parse_command(bytes).is_err());
        }
        assert!(serde_json::from_str::<Command>(r#"{"command":"preferences"}"#).is_ok());
        assert!(parse_command(br#"{"command":"minimize"}"#).is_ok());
        assert!(parse_command(br#"{"command":"minimize","inject":"x"}"#).is_err());
        assert!(parse_command(br#"{"command":"show","inject":"x"}"#).is_err());
        assert!(serde_json::from_str::<Command>(r#"{"command":"firmware_erase"}"#).is_err());
        assert!(parse_command(br#"{"command":"find_cell","query":"1, 20-28"}"#).is_ok());
        assert!(parse_command(br#"{"command":"find_cell","query":"81"}"#).is_err());
        assert!(parse_command(br#"{"command":"find_cell","query":"1","inject":"x"}"#).is_err());
        assert!(parse_command(br#"{"command":"notification_test"}"#).is_ok());
        assert!(parse_command(br#"{"command":"screenshot","path":"relative.png"}"#).is_err());
        let capture = Command::Screenshot {
            path: std::env::temp_dir()
                .join("pakar-capture.png")
                .to_string_lossy()
                .into(),
        };
        assert!(parse_command(&serde_json::to_vec(&capture).unwrap()).is_ok());
        assert!(parse_command(br#"{"command":"notification_test","message":"injected"}"#).is_err());
        assert!(parse_command(br#"{"command":"preferences_page","page":"notifications"}"#).is_ok());
        assert!(parse_command(br#"{"command":"preferences_page","page":"unknown"}"#).is_err());
        for action in crate::platform::windows_quick_actions::WINDOWS_QUICK_ACTIONS {
            let command = crate::ui::shell::launch_command(&[action.arguments.into()]);
            assert!(parse_command(&serde_json::to_vec(&command).unwrap()).is_ok());
        }
    }
    #[cfg(windows)]
    #[test]
    fn owner_mutex_does_not_admit_two_primary_instances() {
        // Skip if a real application owns the production namespace.
        let first = acquire_gui_ownership().unwrap();
        if let GuiOwnership::Primary(guard) = first {
            assert!(matches!(
                acquire_gui_ownership().unwrap(),
                GuiOwnership::Existing
            ));
            drop(guard);
        }
    }
}
