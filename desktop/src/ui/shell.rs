//! Pealayer application shell adapted for a controller rather than media playback.
//! Menu -> typed action -> app dispatcher is shared with native launch/IPC/tray.
use crate::{
    ui::{
        dialog, icons, menu,
        layout::{self, Panel},
    },
    *,
};
use eframe::egui;
use super::dropdown::DropdownUiExt;
use platform::instance::Command;

pub fn launch_command(args: &[String]) -> Command {
    let option = |flag: &str| args.iter().position(|v| v == flag)
        .and_then(|at| args.get(at + 1)).filter(|v| !v.starts_with("--")).cloned();
    if args.iter().any(|v| v == "--wifi") {
        Command::UseWifi { endpoint: option("--endpoint") }
    } else if args.iter().any(|v| v == "--usb") {
        Command::UseUsb { port: option("--port") }
    } else if args.iter().any(|v| v == "--disconnect") {
        Command::Disconnect
    } else if args.iter().any(|v| v == "--update-esp") {
        Command::UpdateEsp { path: option("--update-esp").unwrap_or_default(),
            confirmed: args.iter().any(|v| v == "--confirm") }
    } else if args.iter().any(|v| v == "--led-effect") {
        Command::LedEffect { value: option("--led-effect").unwrap_or_default() }
    } else if args.iter().any(|v| v == "--led-output") {
        Command::LedOutput { value: option("--led-output").unwrap_or_default() }
    } else if args.iter().any(|v| v == "--pixel-arm") {
        Command::PixelArm
    } else if args.iter().any(|v| v == "--pixel-show") {
        Command::PixelShow { index: option("--pixel-show").and_then(|v| v.parse().ok()).unwrap_or(0) }
    } else if args.iter().any(|v| v == "--led-mode") {
        Command::LedMode { value: option("--led-mode").unwrap_or_default() }
    } else if let Some(at) = args.iter().position(|v| v == "--led-cells") {
        Command::LedCells {
            ids:args.get(at+1).filter(|v|!v.starts_with("--")).cloned().unwrap_or_default(),
            state:args.get(at+2).filter(|v|!v.starts_with("--")).cloned().unwrap_or_default(),
            effect:args.get(at+3).filter(|v|!v.starts_with("--")).cloned().unwrap_or_else(||"immediate".into()),
        }
    } else if let Some(at) = args.iter().position(|v| v == "--led-cell") {
        Command::LedCell {
            id: args.get(at + 1).and_then(|v| v.parse().ok()).unwrap_or(0),
            state: args.get(at + 2).filter(|v| !v.starts_with("--")).cloned().unwrap_or_default(),
            effect: args.get(at + 3).filter(|v| !v.starts_with("--")).cloned().unwrap_or_else(|| "immediate".into()),
        }
    } else if let Some(at) = args.iter().position(|v| v == "--screenshot") {
        Command::Screenshot {
            path: args.get(at + 1).cloned().unwrap_or_default(),
        }
    } else if args.iter().any(|v| v == "--manage-cells") {
        Command::CellManager
    } else if args.iter().any(|v| v == "--notification-test") {
        Command::NotificationTest
    } else if let Some(at) = args.iter().position(|v| v == "--preferences-page") {
        Command::PreferencesPage {
            page: args.get(at + 1).cloned().unwrap_or_default(),
        }
    } else if args.iter().any(|v| v == "--refresh") {
        Command::Refresh
    } else if args.iter().any(|v| v == "--fullscreen") {
        Command::Fullscreen
    } else if args.iter().any(|v| v == "--minimize") {
        Command::Minimize
    } else if let Some(at) = args.iter().position(|v| v == "--settings-page") {
        Command::DeviceSettingsPage {
            page: args.get(at + 1).cloned().unwrap_or_default(),
        }
    } else if args.iter().any(|v| v == "--device-settings") {
        Command::DeviceSettings
    } else if args.iter().any(|v| v == "--connect-dialog") {
        Command::Connect
    } else if args.iter().any(|v| v == "--quit") {
        Command::Quit
    } else if args.iter().any(|v| v == "--app-status") {
        Command::Status
    } else if args.iter().any(|v| v == "--preferences") {
        Command::Preferences
    } else if args.iter().any(|v| v == "--about") {
        Command::About
    } else if args.iter().any(|v| v == "--workspaces") {
        Command::Workspaces
    } else if let Some(at) = args.iter().position(|v| v == "--find-cell") {
        Command::FindCell {
            query: args.get(at + 1).cloned().unwrap_or_default(),
        }
    } else if let Some(at) = args.iter().position(|v| v == "--panel") {
        Command::Panel {
            name: args.get(at + 1).cloned().unwrap_or_else(|| "Table".into()),
        }
    } else {
        Command::Show
    }
}

pub(crate) fn led_effect_code(value: &str) -> Option<u8> {
    match value { "none" => Some(0), "rainbow" => Some(1), "fire" => Some(2), _ => None }
}
pub(crate) fn led_output_request(value: &str) -> Option<(Vec<(String, String)>, Vec<u8>)> {
    match value {
        "on" | "off" => Some((vec![("enabled".into(), (value == "on").to_string())], vec![u8::from(value == "on")])),
        "static" | "effects" => Some((vec![("mode".into(), value.into())], vec![2, u8::from(value == "effects")])),
        _ => None,
    }
}
pub(crate) fn led_mode_code(value: &str) -> Option<u8> {
    match value { "cell" => Some(1), "snake" => Some(3), _ => None }
}
pub(crate) fn led_cell_payload(id: u8, state: &str, effect: &str) -> Option<Vec<u8>> {
    if !(1..=80).contains(&id) { return None; }
    let state = match state { "white" => 0, "red" => 1, "off" => 2, _ => return None };
    let effect = match effect { "immediate" => 0, "fade" => 1, "pulse" => 2, _ => return None };
    Some(vec![id, state, effect])
}
fn connection_change_blocked(preferences_dirty: bool, device_dirty: bool, pending: bool,
    maintenance: bool, identity_pending: bool) -> bool {
    preferences_dirty || device_dirty || pending || maintenance || identity_pending
}

#[cfg(test)]
mod lifecycle_launch_tests {
    use super::*;
    fn parsed(args: &[&str]) -> serde_json::Value {
        serde_json::to_value(launch_command(&args.iter().map(|value| value.to_string()).collect::<Vec<_>>())).unwrap()
    }
    #[test]
    fn explicit_lifecycle_launches_retain_optional_configured_endpoints() {
        assert_eq!(parsed(&["--wifi"]), serde_json::json!({"command":"use_wifi","endpoint":null}));
        assert_eq!(parsed(&["--wifi", "--endpoint", "http://ledboard.local"]),
            serde_json::json!({"command":"use_wifi","endpoint":"http://ledboard.local"}));
        assert_eq!(parsed(&["--usb"]), serde_json::json!({"command":"use_usb","port":null}));
        assert_eq!(parsed(&["--usb", "--port", "COM22"]), serde_json::json!({"command":"use_usb","port":"COM22"}));
        assert_eq!(parsed(&["--disconnect"]), serde_json::json!({"command":"disconnect"}));
        assert_eq!(parsed(&["--led-output", "on"]), serde_json::json!({"command":"led_output","value":"on"}));
        assert_eq!(parsed(&["--led-output", "off"]), serde_json::json!({"command":"led_output","value":"off"}));
        for mode in ["static", "effects"] {
            assert_eq!(parsed(&["--led-output", mode]), serde_json::json!({"command":"led_output","value":mode}));
            let (form, payload) = led_output_request(mode).unwrap();
            assert_eq!(form, vec![("mode".into(), mode.into())]);
            assert_eq!(payload, vec![2, u8::from(mode == "effects")]);
        }
        assert!(led_output_request("animated").is_none());
        assert_eq!(parsed(&["--pixel-arm"]), serde_json::json!({"command":"pixel_arm"}));
        assert_eq!(parsed(&["--pixel-show", "480"]), serde_json::json!({"command":"pixel_show","index":480}));
        assert_eq!(parsed(&["--pixel-show", "bad"])["index"], 0);
        assert_eq!(parsed(&["--wifi", "--endpoint", "--app-status"])["endpoint"], serde_json::Value::Null);
    }
    #[test]
    fn esp_update_launch_requires_a_separate_explicit_confirmation() {
        assert_eq!(parsed(&["--update-esp", "C:\\firmware\\pakar.bin"]),
            serde_json::json!({"command":"update_esp","path":"C:\\firmware\\pakar.bin","confirmed":false}));
        assert_eq!(parsed(&["--update-esp", "C:\\firmware\\pakar.bin", "--confirm"]),
            serde_json::json!({"command":"update_esp","path":"C:\\firmware\\pakar.bin","confirmed":true}));
        assert_eq!(parsed(&["--update-esp", "--confirm"])["path"], "");
    }
    #[test]
    fn led_launches_use_the_exact_existing_api_and_wire_values() {
        assert_eq!(parsed(&["--led-cells","1,8-12,80","red","fade"]),serde_json::json!({"command":"led_cells","ids":"1,8-12,80","state":"red","effect":"fade"}));
        for (name, code) in [("none",0), ("rainbow",1), ("fire",2)] {
            assert_eq!(parsed(&["--led-effect", name]), serde_json::json!({"command":"led_effect","value":name}));
            assert_eq!(led_effect_code(name), Some(code));
        }
        for (name, code) in [("cell",1), ("snake",3)] {
            assert_eq!(parsed(&["--led-mode", name]), serde_json::json!({"command":"led_mode","value":name}));
            assert_eq!(led_mode_code(name), Some(code));
        }
        assert_eq!(parsed(&["--led-cell", "80", "white"]), serde_json::json!({"command":"led_cell","id":80,"state":"white","effect":"immediate"}));
        assert_eq!(parsed(&["--led-cell", "2", "off", "pulse"]), serde_json::json!({"command":"led_cell","id":2,"state":"off","effect":"pulse"}));
        for (state, code) in [("white",0), ("red",1), ("off",2)] {
            for (effect, transition) in [("immediate",0), ("fade",1), ("pulse",2)] {
                assert_eq!(led_cell_payload(1, state, effect), Some(vec![1, code, transition]));
            }
        }
        for args in [&["--led-cell", "256", "white"][..], &["--led-cell", "garbage", "white"][..], &["--led-cell"][..]] {
            assert_eq!(parsed(args)["id"], 0);
        }
        assert!(led_effect_code("").is_none());
        assert!(led_effect_code("confetti").is_none());
        assert!(led_mode_code("first-wins").is_none()); // No AVR/game shortcut in this bounded interface.
        assert!(led_cell_payload(0, "white", "immediate").is_none());
        assert!(led_cell_payload(81, "white", "immediate").is_none());
        assert!(led_cell_payload(1, "unknown", "immediate").is_none());
        assert!(led_cell_payload(1, "white", "unknown").is_none());
    }
    #[test]
    fn each_dirty_or_pending_owner_independently_blocks_connection_changes() {
        assert!(!connection_change_blocked(false, false, false, false, false));
        for bit in 0..5 {
            let mut guards = [false;5];
            guards[bit] = true;
            assert!(connection_change_blocked(guards[0], guards[1], guards[2], guards[3], guards[4]));
        }
    }
}
fn badge(ui: &mut egui::Ui, label: &str, active: bool) {
    let color = ui::palette::color(
        config::live().color_palette,
        ui.visuals().dark_mode,
        if active { "green" } else { "subtle" },
    );
    ui.label(
        egui::RichText::new(format!("●  {label}"))
            .color(color)
            .size(11.0),
    );
}
fn field(ui: &mut egui::Ui, icon: &str, label: &str, value: &mut String, password: bool) {
    dialog::setting_row(ui, icon, label, None, |ui| {
        ui.add_sized(
            [ui.available_width().min(340.0), 28.0],
            dialog::singleline_text_edit(value).password(password),
        );
    });
}

fn finish_table_area(ui: &mut egui::Ui, area: egui::Rect) {
    // Absolute ui.put calls move the cursor to the last cell.
    // Restore the complete staggered-grid extent before placing the footer.
    ui.advance_cursor_after_rect(area);
    ui.add_space(8.0);
}

#[cfg(test)]
mod table_geometry_tests {
    use super::*;
    #[test]
    fn footer_stays_below_every_half_staggered_row_after_absolute_widgets() {
        for pitch in [30.0, 60.0, 94.0] {
            let ctx = egui::Context::default();
            let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                let height = pitch * 0.72;
                let (area, _) = ui.allocate_exact_size(
                    egui::vec2(pitch * 20.0, height * 4.5),
                    egui::Sense::hover(),
                );
                let last_cell = crate::ui::cell_table::cell_rect(area.min,pitch,height,80);
                ui.put(last_cell, egui::Label::new("80"));
                finish_table_area(ui, area);
                let footer = ui.label(crate::ui::i18n::text("Cell selected")).rect;
                assert!(
                    footer.top() >= area.bottom() + 8.0,
                    "Footer overlaps staggered grid at pitch {pitch}: {area:?} {footer:?}"
                );
            });
            output.textures_delta.clear();
        }
    }
}
impl PakarControllerApp {
    pub fn receive_configuration(&mut self, path: &str, value: serde_json::Value) {
        if let Err(error) = self.device_settings.observe(path, value.clone()) {
            self.device_settings.message = error;
            return;
        }
        if path == "/api/device/name" {
            let dirty = self.identity_saved.as_ref().is_some_and(|(host,label)|
                self.device_name.as_ref()!=Some(host) || self.board_display_name.as_ref()!=Some(label));
            if dirty { return; } // Periodic readback must not erase a name draft.
            if let Some(name) = value["hostname"]
                .as_str()
                .filter(|name| crate::identity::normalize_name(name).is_ok())
            {
                self.device_name = Some(name.into());
            }
            self.board_display_name = value["display_name"].as_str().filter(|v|crate::identity::valid_display_name(v)).map(str::to_owned);
            self.identity_saved = self.device_name.clone().zip(self.board_display_name.clone());
        }
    }
    pub fn open_preferences(&mut self) {
        if self.preferences.is_none() {
            self.preferences = Some(ui::preferences::PreferencesDraft::new(self.config.clone()));
        }
    }
    pub fn dispatch(&mut self, command: Command, ctx: &egui::Context) {
        match command {
            Command::UseWifi { endpoint } => {
                if self.connection_change_guard() { return; }
                if let Some(endpoint) = endpoint {
                    let endpoint = endpoint.trim();
                    if reqwest::Url::parse(endpoint).ok().is_none_or(|url|
                        endpoint.len() > 512 || !matches!(url.scheme(), "http" | "https") || url.host_str().is_none()
                            || !url.username().is_empty() || url.password().is_some() || url.query().is_some()
                            || url.fragment().is_some() || !matches!(url.path(), "" | "/")) {
                        self.message = "Use http(s)://hostname[:port] without credentials or a path".into();
                        return;
                    }
                    self.base = endpoint.to_owned();
                }
                self.set_connection(Transport::WiFi, true);
                ctx.request_repaint();
                return; // Process-local lifecycle must never focus the desktop.
            }
            Command::UseUsb { port } => {
                if self.connection_change_guard() { return; }
                if let Some(port) = port {
                    if port.trim().is_empty() || port.len() > 512 || port.chars().any(char::is_control) {
                        self.message = "Use a nonempty USB serial port or hardware path".into();
                        return;
                    }
                    self.serial_port = port.trim().to_owned();
                }
                self.set_connection(Transport::Serial, true);
                ctx.request_repaint();
                return;
            }
            Command::Disconnect => {
                self.set_connection(self.transport, false);
                ctx.request_repaint();
                return;
            }
            Command::UpdateEsp { path, confirmed } => {
                if !confirmed {
                    self.message = "ESP firmware update requires explicit --confirm".into();
                } else if let Err(error) = self.update_esp_from_file(&path) {
                    self.message = error;
                }
                ctx.request_repaint();
                return; // The guarded existing maintenance owner handles OTA.
            }
            Command::LedEffect { value } => {
                if let Some(code) = led_effect_code(&value) {
                    self.action("/api/effect", vec![("value".into(), value)], OP_PATTERN, vec![code]);
                } else { self.message = "Unknown LED effect; use none, rainbow or fire".into(); }
                ctx.request_repaint();
                return;
            }
            Command::LedOutput { value } => {
                if let Some((form, payload)) = led_output_request(&value) {
                    let known = self.live_status().is_some_and(|s| match value.as_str() {
                        "static" | "effects" => led_output_mode(s).is_some(),
                        _ => s.led_output.get("enabled").and_then(|v| v.as_bool()).is_some(),
                    });
                    if known { self.action("/api/led/output", form, OP_LED_OUTPUT, payload); }
                    else { self.message = "LED output state is unknown for the current board; refresh before changing it".into(); }
                } else { self.message = "LED output requires on, off, static or effects".into(); }
                ctx.request_repaint();
                return;
            }
            Command::PixelArm => { self.arm_pixel_diagnostic(); ctx.request_repaint(); return; }
            Command::PixelShow { index } => { self.show_pixel_diagnostic(index); ctx.request_repaint(); return; }
            Command::LedMode { value } => {
                if let Some(code) = led_mode_code(&value) {
                    self.action("/api/mode", vec![("value".into(), value)], OP_SET_MODE, vec![code]);
                } else { self.message = "Unknown LED mode; use cell or snake".into(); }
                ctx.request_repaint();
                return;
            }
            Command::LedCell { id, state, effect } => {
                if let Some(payload) = led_cell_payload(id, &state, &effect) {
                    self.action("/api/cell", vec![("id".into(), id.to_string()),
                        ("state".into(), state), ("effect".into(), effect)], OP_CELL_ACTION, payload);
                } else { self.message = "Invalid LED cell, state or transition".into(); }
                ctx.request_repaint();
                return;
            }
            Command::LedCells { ids,state,effect } => {
                match super::cell_search::parse(&ids).and_then(|ids|crate::cell_batch::Request::new(&ids,&state,&effect)) {
                    Ok(batch)=>{if let Err(error)=self.dispatch_cell_batch(batch){self.message=error;}},
                    Err(error)=>self.message=error,
                }
                ctx.request_repaint();return;
            }
            Command::Screenshot { path } => {
                if let Err(error) = self.capture.request(path) {
                    self.message = error;
                }
                ctx.request_repaint();
                return; // A capture request never changes focus, panels or visibility.
            }
            Command::DeviceSettings => {
                self.device_settings.open = true;
                self.configuration_loaded = false;
            }
            Command::DeviceSettingsPage { page } => {
                if let Some(index) = ui::device_settings::page_index(&page) {
                    self.device_settings.open = true;
                    self.device_settings.page = index;
                    self.device_settings.search.clear();
                    self.configuration_loaded = false;
                } else {
                    self.message = "Unknown device settings page".into();
                }
            }
            Command::Quit => {
                self.exiting = true;
                self.save_workspace();
                ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                return;
            }
            Command::Minimize => {
                ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(true));
                ctx.request_repaint();
                return; // Do not immediately restore through the shared tail.
            }
            Command::Preferences => self.open_preferences(),
            Command::PreferencesPage { page } => {
                if let Some(index) = ui::preferences::page_index(&page) {
                    self.open_preferences();
                    if let Some(draft) = &mut self.preferences {
                        draft.select_page(index);
                    }
                } else {
                    self.message = "Unknown preferences page".into();
                }
            }
            Command::Refresh => self.refresh(),
            Command::Fullscreen => {
                let fullscreen = ctx.input(|i| i.viewport().fullscreen.unwrap_or(false));
                ctx.send_viewport_cmd(egui::ViewportCommand::Fullscreen(!fullscreen));
            }
            Command::NotificationTest => self.notification_test(),
            Command::Connect => self.show_connection = true,
            Command::About => self.show_about = true,
            Command::Workspaces => self.show_workspaces = true,
            Command::CellManager => {
                if !self.has_table() { return; }
                self.table.manager_open = true;
                self.reveal_panels.push(Panel::Table);
            }
            Command::FindCell { query } => {
                if !self.has_table() { return; }
                if ui::cell_search::parse(&query).is_ok() {
                    self.cell_search = query;
                    self.apply_cell_search_selection();
                    self.reveal_panels.push(Panel::Table);
                    self.focus_cell_search = true;
                }
            }
            Command::Panel { name } => {
                if let Some(panel) = Panel::from_name(&name) {
                    if panel.available(self.peer_context()) { self.reveal_panels.push(panel); }
                }
            }
            Command::Status => return, // Read-only diagnostics must never raise/focus the window.
            Command::Show => {}
        }
        if !self.screenshot_harness {
            ctx.send_viewport_cmd(egui::ViewportCommand::Visible(true));
            ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(false));
            ctx.send_viewport_cmd(egui::ViewportCommand::Focus);
        }
        ctx.request_repaint();
    }
    pub fn notify(&self, title: &str, message: &str, severity: messaging::Severity) {
        self.notify_id("pakar.command", title, message, severity);
    }
    pub fn notify_id(&self, id: &str, title: &str, message: &str, severity: messaging::Severity) {
        let request = messaging::ToastRequest {
            id: Some(id.into()),
            title: title.into(),
            message: message.chars().take(2048).collect(),
            severity,
            timeout_ms: 5000,
        };
        if messaging::publish(request.clone(), "desktop").is_ok() {
            self.notifications.borrow_mut().enqueue(request);
        }
    }
    pub fn notification_test(&self) {
        self.notify_id("pakar.notification-test", "Notification test", "This is an in-app toast and a native Windows notification request. OS visibility is reported separately in Preferences and --app-status.", messaging::Severity::Info);
    }
    pub fn record_event(&mut self, kind: &str, message: &str) {
        self.event_log.record(kind, message);
    }
    pub fn save_workspace(&mut self) {
        self.config.connection_method = self.transport;
        self.config.connection_enabled = self.connection_enabled;
        layout::remember_connected(&mut self.config, &self.dock, self.active_context.is_some());
        if config::AppConfig::load().ok().as_ref() != Some(&self.config) {
            if let Err(error) = self.config.save() {
                self.message = error;
            }
        }
    }
    pub(crate) fn connection_change_guard(&mut self) -> bool {
        let blocked = connection_change_blocked(
            self.preferences.as_ref().is_some_and(|draft| draft.is_dirty()),
            self.device_settings.dirty(), self.device_settings.pending.is_some(),
            self.device_settings.maintenance_pending.is_some(), self.identity_pending);
        if blocked {
            self.message = "Save or discard settings drafts and wait for pending operations before changing connection".into();
        }
        blocked
    }
    pub(crate) fn live_status(&self) -> Option<&DeviceStatus> {
        match self.command_route() {
            Some(Transport::WiFi) => self.status.as_ref(),
            Some(Transport::Serial) => self.serial_status.as_ref(),
            _ => None,
        }
    }
    pub(super) fn refresh(&mut self) {
        self.configuration_loaded = false;
        self.last_poll = Instant::now() - Duration::from_secs(2);
        self.last_mapping_poll = Instant::now() - Duration::from_secs(30);
    }
    pub(crate) fn peer_context(&self) -> Option<&crate::capabilities::Manifest> {
        self.live_status()?.capabilities.as_ref().filter(|c| c.known())
    }
    pub(crate) fn has_feature(&self, feature: &str) -> bool {
        self.peer_context().is_some_and(|c| c.has(feature))
    }
    pub(crate) fn has_table(&self) -> bool {
        self.peer_context().is_some_and(|c| c.table())
    }
    pub(crate) fn mapping_status(&self) -> String {
        if !self.has_table() { return "This connected board has not acknowledged table mapping".into(); }
        if self.command_route()==Some(Transport::Serial) && self.live_status().and_then(peer_http::Pin::from_status)==self.mapping_peer && !self.mappings.is_empty(){
            return "Confirmed mapping from this board’s immutable USB snapshot · Wi-Fi is not used".into();
        }
        if self.command_route() != Some(Transport::WiFi) {
            return "Mapping readback requires the selected Wi-Fi connection; USB never falls back to Wi-Fi".into();
        }
        let peer = self.status.as_ref().and_then(peer_http::Pin::from_status);
        let confirmed = peer.is_some() && peer == self.mapping_peer && !self.mappings.is_empty();
        match (confirmed, self.mapping_request_pending.is_some(), self.mapping_error.as_deref()) {
            (true, _, Some(error)) => format!("Using confirmed mapping for this board; refresh failed and will retry: {error}"),
            (false, _, Some(error)) => format!("Mapping not received yet; retrying: {error}"),
            (true, true, _) => "Confirmed mapping for this board · refreshing…".into(),
            (true, false, _) => "Confirmed mapping for this board".into(),
            (false, true, _) => "Reading this board’s mapping…".into(),
            _ => "Waiting to read this board’s mapping; not evidence of an unsupported capability".into(),
        }
    }
    fn synchronize_context(&mut self, ctx: &egui::Context) {
        let available: Vec<_> = Panel::ALL.into_iter().filter(|p| p.available(self.peer_context())).collect();
        let key = self.peer_context().map(|c| format!("{:?}:{}:{}:{}", self.command_route(),
            if self.command_route() == Some(Transport::Serial) { &self.serial_port } else { &self.base }, c.board_kind, c.name));
        if key != self.active_context {
            if let Some(previous) = self.active_context.take() {
                layout::remember_connected(&mut self.config, &self.dock, true);
                // Runtime-only per-peer workspace cache. No discovery-driven SSD writes.
                if self.context_layouts.len() >= 16 { self.context_layouts.clear(); }
                self.context_layouts.insert(previous, self.dock.clone());
            }
            self.dock = key.as_ref().and_then(|key| self.context_layouts.get(key)).cloned()
                .unwrap_or_else(|| if key.is_some() { layout::restore_connected(&mut self.config) }
                    else { egui_dock::DockState::new(vec![Panel::Events]) });
            self.active_context = key;
            self.device_name = self.peer_context().and_then(|c|crate::identity::normalize_name(&c.hostname).ok());
            self.board_display_name = self.peer_context().filter(|c|crate::identity::valid_display_name(&c.name)).map(|c|c.name.clone());
            self.identity_saved = self.device_name.clone().zip(self.board_display_name.clone());
            self.configuration_loaded = false;
            self.context_panels = available.clone();
            self.table.stop_pending_edits();
            self.table.selection.ids.clear(); self.cell_search.clear();
            self.table.pixel_sample = None;
            // A title/capability refresh can change the workspace key without
            // changing board/boot. Do not erase its confirmed mapping merely
            // because the next optional HTTP refresh has not completed yet.
            let peer = self.live_status().and_then(peer_http::Pin::from_status);
            if peer.is_none() || peer != self.mapping_peer {
                self.mappings.clear(); self.mapping_peer = None;
                if !self.mapping_request_pending.as_ref().is_some_and(|pending| Some(&pending.peer) == peer.as_ref()) {
                    self.mapping_request_pending = None; self.mapping_error = None;
                }
                self.last_mapping_poll = Instant::now() - Duration::from_secs(16);
                let _ = self.ws_tx.send(WsCommand::Mapping(Vec::new()));
            }
            let _ = self.ws_tx.send(WsCommand::Stream(false));
            let _ = self.ws_tx.send(WsCommand::Sample(false));
            self.streaming = false; self.sampling = false;
            self.show_mapping = false; self.table.manager_open = false;
        }
        // Prune even on first restore; saved panels are preferences, never evidence.
        let removed: Vec<_> = self.dock.iter_all_tabs().filter(|(_,p)| !available.contains(p)).map(|(_,p)| *p).collect();
        for panel in removed { while let Some(path) = self.dock.find_tab(&panel) { self.dock.remove_tab(path); } }
        if available != self.context_panels {
            for panel in &available {
                if !self.context_panels.contains(panel) && self.dock.find_tab(panel).is_none() {
                    self.dock.push_to_focused_leaf(*panel);
                }
            }
            self.context_panels = available;
        }
        if !self.has_table() {
            self.table.stop_pending_edits();
            self.table.manager_open = false; self.table.editing_order = false;
            self.show_mapping = false; self.confirm_reset = false;
        }
        if !self.has_table() || !self.has_feature("streaming") || self.command_route() != Some(Transport::WiFi) {
            if self.streaming || self.sampling {
                let _ = self.ws_tx.send(WsCommand::Stream(false));
                let _ = self.ws_tx.send(WsCommand::Sample(false));
                self.streaming = false; self.sampling = false;
            }
        }
        let title = self.live_status().map(|s| s.capabilities.as_ref().map(|c| c.title())
            .unwrap_or_else(|| "PakarController · Discovering capabilities".into()))
            .unwrap_or_else(|| "PakarController · No board connected".into());
        let app_name = self.peer_context().and_then(|p|p.application()).map(|a|a.name.clone()).unwrap_or_else(||"PakarController".into());
        if let Some(icon) = crate::branding::sync_peer(ctx, self.peer_context()) {
            if let Some(tray) = &self.tray {
                if let Err(error) = tray.set_application(&app_name, &icon) { self.record_event("Application artwork", &error); }
            }
        }
        ctx.send_viewport_cmd(egui::ViewportCommand::Title(title));
    }
    // Pealayer src/app.rs App::logic: command/event/snapshot work is not paint work.
    pub fn native_logic(&mut self, ctx: &egui::Context) {
        ui::window_geometry::reconcile(ctx);
        while let Ok(command) = self.control.rx.try_recv() {
            self.dispatch(command, ctx);
        }
        if let Some(action) = self.tray.as_mut().and_then(|tray| tray.poll()) {
            match action {
                platform::windows_tray::TrayAction::Show => self.dispatch(Command::Show, ctx),
                platform::windows_tray::TrayAction::Preferences => {
                    self.dispatch(Command::Preferences, ctx)
                }
                platform::windows_tray::TrayAction::DeviceSettings => {
                    self.dispatch(Command::DeviceSettings, ctx)
                }
                platform::windows_tray::TrayAction::Workspaces => {
                    self.dispatch(Command::Workspaces, ctx)
                }
                platform::windows_tray::TrayAction::Refresh => self.refresh(),
                platform::windows_tray::TrayAction::Exit => self.dispatch(Command::Quit, ctx),
            }
        }
        if !self.exiting
            && self.config.close_to_tray
            && self.tray.is_some()
            && ctx.input(|i| i.viewport().close_requested())
        {
            ctx.send_viewport_cmd(egui::ViewportCommand::CancelClose);
            ctx.send_viewport_cmd(egui::ViewportCommand::Visible(false));
            self.save_workspace();
        }
        self.poll();
        self.bridge_tick();
        self.synchronize_context(ctx);
        self.process_cell_queue(ctx);
        self.deliver_notifications();
        #[cfg(windows)]
        if let Some(desktop) = &mut self.windows_desktop {
            let errors = desktop.sync(
                ctx.theme() == egui::Theme::Dark,
                self.config.color_palette,
                self.device_settings.pending.is_some()
                    || self.device_settings.maintenance_pending.is_some(),
            );
            for error in errors {
                self.record_event("Windows desktop", &error);
            }
        }
        ctx.request_repaint_after(Duration::from_millis(250));
        let snapshot = self.live_status().cloned();
        let fullscreen = ctx.input(|i| i.viewport().fullscreen.unwrap_or(false));
        if let Ok(mut value) = self.control.snapshot.lock() {
            *value = serde_json::json!({"application":self.peer_context().and_then(|p|p.application()).map(|a|a.name.as_str()).unwrap_or("PakarController"),"client_application":"PakarController","app_title":self.peer_context().map(|p|p.title()),"version":env!("CARGO_PKG_VERSION"),"configured_endpoint":self.base,"process_id":std::process::id(),"ready":true,"single_instance":self.config.single_instance,"panels":self.dock.iter_all_tabs().map(|(_,p)|p.name()).collect::<Vec<_>>(),"preferences_open":self.preferences.is_some(),"mapping_open":self.show_mapping,"workspaces_open":self.show_workspaces,"about_open":self.show_about,"device_online":snapshot.is_some(),"websocket_live":self.websocket_live,"serial_live":self.serial_live,"cell_search":ui::cell_search::format_selection(&self.table.selection.ids),"cell_search_draft":self.cell_search,"selected_cells":self.table.selection.cells(),"highlighted_cells":self.table.selection.cells(),"search_matches":self.table.selection.cells(),"selected_cell":self.selected_cell});
            value["device_settings"] = serde_json::json!({"open": self.device_settings.open, "page": self.device_settings.page, "network_loaded": self.device_settings.network.is_some(), "cloud_loaded": self.device_settings.cloud.is_some(), "write_pending": self.device_settings.pending.is_some(), "maintenance_pending": self.device_settings.maintenance_pending.is_some()});
            value["connection"] = serde_json::json!({
                "method": self.transport.label(), "enabled": self.connection_enabled,
                "releasing": self.connection_gate.releasing().map(|method| method.label()),
                "online": self.command_route() == Some(self.transport),
            });
            value["serial_port"] = serde_json::json!(self.serial_port);
            value["pixel_diagnostic"] = serde_json::json!({"wire_pixel":self.pixel_diagnostic.index.clamp(1,480),
                "raw_index":self.pixel_diagnostic.index.clamp(1,480)-1,"pending":self.pixel_diagnostic.pending.is_some(),
                "armed":snapshot.as_ref().is_some_and(|s| pixel_diagnostic::ready(s)
                    && peer_http::Pin::from_status(s) == self.pixel_diagnostic.armed),"message":self.pixel_diagnostic.message});
            value["device_status"] = serde_json::json!(snapshot);
            value["bridge"] = self.bridge.snapshot();
            value["message"] = serde_json::json!(self.message);
            value["device_settings"]["message"] = serde_json::json!(self.device_settings.message);
            value["peer_capabilities"] = serde_json::json!(self.peer_context());
            value["cell_management"] = serde_json::json!({"open":self.table.manager_open,"selection":self.table.selection.ids,"additive_selection":self.config.additive_cell_selection,"animations":self.config.animate_cells,"indicators":self.config.show_cell_indicators});
            value["screenshot"] = self.capture.status();
            value["minimized"] = serde_json::json!(ctx.input(|i| i.viewport().minimized));
            value["preferences_dirty"] = serde_json::json!(self.preferences.as_ref().is_some_and(|d| d.is_dirty()));
            value["diagnostics"] = serde_json::json!({"events":self.event_log.len(),"active":self.event_log.active_count(),"repeated_occurrences":self.event_log.repeated_count(),"status_request_pending":self.status_request_pending,"status_retry_ms":self.status_retry.delay().as_millis()});
            value["diagnostics"]["recent"] = serde_json::json!(self.event_log.iter().rev().take(12)
                .map(|row| serde_json::json!({"kind":row.kind,"message":row.message,"active":row.active,"occurrences":row.occurrences})).collect::<Vec<_>>());
            value["diagnostics"]["active_incidents"] = serde_json::json!(self.event_log.iter().rev().filter(|row| row.active).take(12)
                .map(|row| serde_json::json!({"kind":row.kind,"message":row.message,"occurrences":row.occurrences})).collect::<Vec<_>>());
            value["mapping"] = serde_json::json!({"count":self.mappings.len(),"pending":self.mapping_request_pending.is_some(),"status":self.mapping_status(),"error":self.mapping_error});
            value["usb_commands_pending"] = serde_json::json!(self.serial_operations_pending);
            value["usb_command"] = serde_json::json!(self.serial_timing);
            value["notifications"] = self.notification_snapshot();
            value["tray_available"] = serde_json::json!(self.tray.is_some());
            value["tray_shell"] = self.tray.as_ref().map(|tray|tray.shell_snapshot()).unwrap_or(serde_json::Value::Null);
            value["fullscreen"] = serde_json::json!(fullscreen);
        }
    }
    pub fn draw_shell(&mut self, ui: &mut egui::Ui) {
        ui.with_layout(super::i18n::vertical_layout(super::i18n::rtl()), |ui| {
            self.draw_shell_contents(ui);
        });
    }
    fn draw_shell_contents(&mut self, ui: &mut egui::Ui) {
        let ctx = ui.ctx().clone();
        let snapshot = self.live_status().cloned();
        if ctx.input_mut(|i| {
            i.consume_key(
                egui::Modifiers::CTRL | egui::Modifiers::SHIFT,
                egui::Key::Comma,
            )
        }) {
            self.device_settings.open = true;
            self.configuration_loaded = false;
        } else if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::Comma)) {
            self.open_preferences();
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::K)) {
            self.show_connection = true;
        }
        if self.has_table() && ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::M)) {
            self.show_mapping = true;
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::F5)) {
            self.refresh();
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::F11)) {
            self.dispatch(Command::Fullscreen, &ctx);
        }
        if self.preferences.is_none()
            && !self.device_settings.open
            && !self.show_connection
            && !self.confirm_reset
            && !self.table.manager_open
            && !self.show_mapping
            && !self.show_workspaces
            && !self.show_about
        {
            if self.has_table() && ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::F)) {
                self.reveal_panels.push(Panel::Table);
                self.focus_cell_search = true;
            }
            let previous = ctx.input_mut(|i| i.consume_key(egui::Modifiers::SHIFT, egui::Key::F3));
            let next = ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::F3));
            if self.has_table() && (previous || next) {
                if let Some(id) = self.table.selection.navigate(self.selected_cell, previous) {
                    self.selected_cell = id;
                    self.reveal_panels.push(Panel::Table);
                }
            }
        }
        egui::Panel::top("pakar-menu").show(ui, |ui| {
            egui::MenuBar::new().ui(ui, |ui| {
                menu::top_menu_button(ui, crate::ui::i18n::text("File"), |ui| {
                    if ui
                        .add(egui::Button::new(format!("{}  {}", icons::CIRCUITRY, crate::ui::i18n::text("Device settings…")))
                            .shortcut_text("Ctrl+Shift+,"))
                        .clicked()
                    {
                        self.device_settings.open = true;
                        self.configuration_loaded = false;
                        ui.close();
                    }
                    if ui
                        .add(egui::Button::new(format!("{}  {}", icons::PLUG, crate::ui::i18n::text("Connect…")))
                            .shortcut_text("Ctrl+K"))
                        .clicked()
                    {
                        self.show_connection = true;
                        ui.close();
                    }
                    if self.has_table() && ui
                        .add(egui::Button::new(format!("{}  {}", icons::GRID_FOUR, crate::ui::i18n::text("Cell mapping…")))
                            .shortcut_text("Ctrl+M"))
                        .clicked() && self.has_table()
                    {
                        self.show_mapping = true;
                        ui.close();
                    }
                    ui.separator();
                    if ui
                        .add(egui::Button::new(format!("{}  {}", icons::GEAR, crate::ui::i18n::text("Preferences…")))
                            .shortcut_text("Ctrl+,"))
                        .clicked()
                    {
                        self.open_preferences();
                        ui.close();
                    }
                    ui.separator();
                    if ui.button(format!("{}  {}", icons::POWER, crate::ui::i18n::text("Quit"))).clicked() {
                        self.dispatch(Command::Quit, &ctx);
                    }
                });
                menu::top_menu_button(ui, crate::ui::i18n::text("Workspace"), |ui| {
                    if ui
                        .add(egui::Button::new(format!("{}  {}", icons::APP_WINDOW, crate::ui::i18n::text("Toggle fullscreen")))
                            .shortcut_text("F11"))
                        .clicked()
                    {
                        self.dispatch(Command::Fullscreen, &ctx);
                        ui.close();
                    }
                    ui.separator();
                    icons::submenu(ui, format!("{}  {}", icons::TABS, crate::ui::i18n::text("Panels")), |ui| {
                        layout::draw_workspace_tab_menu(self, ui)
                    });
                    ui.separator();
                    if ui
                        .button(format!("{}  {}", icons::TABS, crate::ui::i18n::text("Manage workspaces…")))
                        .clicked()
                    {
                        self.show_workspaces = true;
                        ui.close();
                    }
                    if ui
                        .button(format!(
                            "{}  Reset workspace layout",
                            icons::ARROW_COUNTER_CLOCKWISE
                        ))
                        .clicked()
                    {
                        self.dock = layout::create_initial_layout();
                        ui.close();
                    }
                });
                menu::top_menu_button(ui, crate::ui::i18n::text("Device"), |ui| {
                    if ui
                        .add(egui::Button::new(format!("{}  {}", icons::ARROW_CLOCKWISE, crate::ui::i18n::text("Refresh")))
                            .shortcut_text("F5"))
                        .clicked()
                    {
                        self.refresh();
                        ui.close();
                    }
                    if self.has_feature("network") && ui
                        .button(format!("{}  {}", icons::GLOBE, crate::ui::i18n::text("Network & cloud")))
                        .clicked()
                    {
                        self.reveal_panels.push(Panel::Network);
                        ui.close();
                    }
                    if self.has_table() && ui
                        .add_enabled(
                            snapshot.is_some(),
                            egui::Button::new(format!(
                                "{}  Reset game…",
                                icons::ARROW_COUNTER_CLOCKWISE
                            )),
                        )
                        .clicked()
                    {
                        self.confirm_reset = true;
                        ui.close();
                    }
                });
                menu::top_menu_button(ui, crate::ui::i18n::text("Help"), |ui| {
                    if ui.button(format!("{}  {}", icons::DOWNLOAD_SIMPLE, crate::ui::i18n::text("Save workspace screenshot…"))).clicked() {
                        ui.close();
                        if let Some(path) = rfd::FileDialog::new().add_filter("PNG image", &["png"]).set_file_name("PakarController.png").save_file() {
                            if let Err(error)=self.capture.request(path.to_string_lossy().into()) {self.message=error;}
                        }
                    }
                    if ui
                        .button(format!("{}  About {}…", icons::INFO,self.peer_context().and_then(|p|p.application()).map(|a|a.name.as_str()).unwrap_or("PakarController")))
                        .clicked()
                    {
                        self.show_about = true;
                        ui.close();
                    }
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui
                        .button(icons::GEAR)
                        .on_hover_text(crate::ui::i18n::text("Preferences (Ctrl+,)"))
                        .clicked()
                    {
                        self.open_preferences();
                    }
                    if ui
                        .button(icons::PLUG)
                        .on_hover_text(crate::ui::i18n::text("Connect (Ctrl+K)"))
                        .clicked()
                    {
                        self.show_connection = true;
                    }
                    ui.label(egui::RichText::new(self.peer_context().map(|c| c.title()).unwrap_or_else(|| "PakarController".into())).strong());
                    crate::branding::peer_logo(ui, 18.0, self.peer_context().and_then(|p|p.application()));
                });
            });
        });
        let s = snapshot.clone().unwrap_or_default();
        let status_actions = ui::status_bar::draw(
            ui,
            ui::status_bar::StatusBar {
                status_rgb: s.status_rgb.as_ref(),
                peer_known: snapshot.is_some(),
                avr_available: ["display", "text", "buzzer", "inputs", "outputs", "settings"].iter().any(|v| self.has_feature(v)),
                display_available: self.has_feature("display"),
                network_available: self.transport == Transport::WiFi && self.has_feature("network"),
                telemetry_available: self.has_feature("telemetry"),
                message: &self.message,
                hostname: &s.hostname,
                station: if self.transport != Transport::WiFi { "" }
                    else if s.uplink == "ethernet" && s.uplink_ipv4_ready == Some(true) { "Ethernet" }
                    else { &s.station_ssid },
                avr_online: s.avr_online,
                tm1637_ready: s.tm1637_ack,
                websocket_live: self.transport == Transport::WiFi && self.websocket_live,
                serial_live: self.transport == Transport::Serial && self.serial_live,
            },
            &mut self.config.status_items,
        );
        for action in status_actions {
            match action {
                ui::status_bar::Action::Save => self.save_workspace(),
                ui::status_bar::Action::Refresh => self.refresh(),
                ui::status_bar::Action::Connect => self.show_connection = true,
                ui::status_bar::Action::Hardware => self.reveal_panels.push(Panel::Hardware),
                ui::status_bar::Action::Network => self.reveal_panels.push(Panel::Network),
                ui::status_bar::Action::Preferences => self.open_preferences(),
            }
        }
        let mut connection_actions = Vec::new();
        egui::Panel::top("pakar-workspace-toolbar")
            .frame(egui::Frame::new().inner_margin(egui::Margin::symmetric(12, 8)))
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.label(
                        egui::RichText::new(icons::GRID_FOUR)
                            .size(19.0)
                            .color(ui.visuals().selection.bg_fill),
                    );
                    ui.strong(self.peer_context().map(|c| c.name.as_str()).filter(|s| !s.is_empty()).unwrap_or("Controller workspace"));
                    ui.separator();
                    badge(
                        ui,
                        if snapshot.is_some() {
                            "Device connected"
                        } else {
                            "Device offline"
                        },
                        snapshot.is_some(),
                    );
                    ui::connection_controls::draw(ui, ui::connection_controls::State {
                        method: self.transport,
                        enabled: self.connection_enabled,
                        live: self.command_route().is_some(),
                        endpoint: &self.base,
                        port: &self.serial_port,
                        identity_pending: self.identity_pending,
                    }, || serialport::available_ports()
                            .map(|ports| ports.into_iter().map(|port| port.port_name).collect())
                            .map_err(|error| error.to_string()), &mut connection_actions);
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui.button(format!("{}  {}", icons::TABS, crate::ui::i18n::text("Workspaces"))).clicked() {
                            self.show_workspaces = true;
                        }
                        ui.menu_button(format!("{}  {}", icons::TABS, crate::ui::i18n::text("Panels")), |ui| {
                            layout::draw_workspace_tab_menu(self, ui)
                        });
                        if self.has_table() && ui
                            .button(format!("{}  {}", icons::GRID_FOUR, crate::ui::i18n::text("Mapping")))
                            .clicked() && self.has_table()
                        {
                            self.show_mapping = true;
                        }
                        if ui
                            .button(icons::ARROW_CLOCKWISE)
                            .on_hover_text(crate::ui::i18n::text("Refresh"))
                            .clicked()
                        {
                            self.refresh();
                        }
                    });
                });
            });
        for action in connection_actions {
            use ui::connection_controls::Action;
            match action {
                Action::Method(method) if !self.identity_pending => self.set_connection(method, false),
                Action::Lifecycle(enabled) if !self.identity_pending => self.set_connection(self.transport, enabled),
                Action::Port(port) if !self.identity_pending => {
                    if self.connection_change_guard() { continue; }
                    self.set_connection(self.transport, false);
                    if self.connection_enabled { continue; } // A refused release cannot change its endpoint.
                    self.serial_port = port;
                    self.config.serial_port = self.serial_port.clone();
                    self.save_workspace();
                }
                Action::Configure => self.show_connection = true,
                _ => {}
            }
        }
        self.open_panels.clear();
        let layout_changed = !self.close_panels.is_empty() || !self.reveal_panels.is_empty();
        for panel in std::mem::take(&mut self.close_panels) {
            if let Some(path) = self.dock.find_tab(&panel) {
                self.dock.remove_tab(path);
            }
        }
        for panel in std::mem::take(&mut self.reveal_panels) {
            if panel.available(self.peer_context()) { layout::reveal(&mut self.dock, panel); }
        }
        if layout_changed {
            self.save_workspace();
        }
        // Workspace restore/menu actions occur after discovery synchronization.
        // Apply the same gate before any tab header is painted in this frame.
        let unavailable: Vec<_> = self.dock.iter_all_tabs().filter(|(_,p)| !p.available(self.peer_context())).map(|(_,p)| *p).collect();
        for panel in unavailable { while let Some(path) = self.dock.find_tab(&panel) { self.dock.remove_tab(path); } }
        egui::CentralPanel::default()
            .frame(egui::Frame::new().inner_margin(0))
            .show(ui, |ui| {
                if self.dock.iter_all_tabs().count() == 0 {
                    ui.centered_and_justified(|ui| {
                        if ui
                            .button(format!("{}  {}", icons::TABS, crate::ui::i18n::text("Restore workspace")))
                            .clicked()
                        {
                            self.dock = egui_dock::DockState::new(Panel::ALL.into_iter().filter(|p| p.available(self.peer_context())).collect());
                        }
                    });
                } else {
                    self.open_panels = self.dock.iter_all_tabs().map(|(_, p)| *p).collect();
                    let tab_rects_id = egui::Id::new("pakar-workspace-tab-button-rects");
                    ctx.data_mut(|data| data.insert_temp(tab_rects_id, Vec::<egui::Rect>::new()));
                    let mut dock =
                        std::mem::replace(&mut self.dock, egui_dock::DockState::new(vec![]));
                    let mut style = egui_dock::Style::from_egui(ui.style().as_ref());
                    let tab_bar_height = style.tab_bar.height;
                    style.buttons.collapse_tabs_color = egui::Color32::TRANSPARENT;
                    style.buttons.collapse_tabs_active_color = egui::Color32::TRANSPARENT;
                    egui_dock::DockArea::new(&mut dock)
                        .style(style)
                        .show_leaf_collapse_buttons(true)
                        .show_inside(ui, &mut layout::PakarTabViewer { app: self });
                    self.dock = dock;
                    layout::paint_dock_disclosure_icons(ui, &self.dock, tab_bar_height);
                    // Adapted from Pealayer app.rs: empty header popup excludes real tab hit rects.
                    let tab_rects = ctx.data_mut(|data| {
                        data.get_temp::<Vec<egui::Rect>>(tab_rects_id)
                            .unwrap_or_default()
                    });
                    let pointer = ctx.pointer_hover_pos();
                    let in_header = pointer.is_some_and(|pos| {
                        self.dock.iter_leaves().any(|(_, leaf)| {
                            let header = egui::Rect::from_min_size(
                                leaf.rect.min,
                                egui::vec2(leaf.rect.width(), tab_bar_height),
                            );
                            header.contains(pos) && !tab_rects.iter().any(|rect| rect.contains(pos))
                        })
                    });
                    let open = in_header
                        && ctx.input(|i| i.pointer.button_clicked(egui::PointerButton::Secondary));
                    let anchor = ui.interact(
                        ui.max_rect(),
                        egui::Id::new("pakar-empty-header-anchor"),
                        egui::Sense::hover(),
                    );
                    egui::Popup::menu(&anchor)
                        .id(egui::Id::new("pakar-empty-header-popup"))
                        .at_pointer_fixed()
                        .open_memory(open.then_some(egui::SetOpenCommand::Bool(true)))
                        .show(|ui| {
                            ui::cell_table::compact_menu(ui);
                            ui.strong(crate::ui::i18n::text("Panels"));
                            ui.separator();
                            layout::draw_workspace_tab_menu(self, ui);
                        });
                    self.open_panels.clear();
                }
            });
        ui::preferences::draw(self, &ctx);
        ui::device_settings::draw(self, &ctx);
        self.draw_dialogs(&ctx);
        self.draw_cell_manager(&ctx);
        if self.sync_cell_selection_search() { ctx.request_repaint(); }
        ui::toasts::draw(&ctx);
    }
    pub fn draw_panel(&mut self, ui: &mut egui::Ui, panel: Panel) {
        if !panel.available(self.peer_context()) { return; }
        dialog::scroll_column(
            ui,
            ("panel-body", panel),
            Some(ui.available_height()),
            |ui| match panel {
                Panel::Table => self.draw_table(ui),
                Panel::EffectControls => self.draw_effects(ui),
                Panel::FrontPanel => self.draw_front_panel(ui),
                Panel::Hardware => self.draw_hardware(ui),
                Panel::Network => self.draw_network(ui),
                Panel::Streaming => self.draw_streaming(ui),
                Panel::Events => self.draw_events(ui),
            },
        );
    }
    fn draw_table(&mut self, ui: &mut egui::Ui) {
        if !self.has_table() { return; }
        self.sync_cell_selection_search();
        let snapshot = self.live_status().cloned();
        ui.horizontal(|ui| {
            ui.heading(crate::ui::i18n::text("Pakar table"));
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if let Some(s) = &snapshot {
                    ui.label(
                        egui::RichText::new(format!("{} · {} pixels", s.mode, s.pixels)).weak(),
                    );
                }
            });
        });
        ui.add_space(8.0);
        if self.has_feature("streaming") && self.command_route()==Some(Transport::WiFi) {
            egui::CollapsingHeader::new("Physical pixel diagnostic")
                .id_salt("physical-pixel-diagnostic").default_open(true)
                .show(ui, |ui| self.draw_pixel_diagnostic(ui));
            ui.add_space(6.0);
        }
        if self.mappings.is_empty() || self.mapping_error.is_some() {
            ui.weak(self.mapping_status());
        }
        ui.horizontal_wrapped(|ui| {
            ui.label(icons::MAGNIFYING_GLASS);
            let search = ui.add_sized(
                [ui.available_width().min(270.0), 28.0],
                dialog::singleline_text_edit(&mut self.cell_search)
                    .id(egui::Id::new("table-cell-search"))
                    .hint_text(crate::ui::i18n::text("Find cell: 12, 3–8  ·  Ctrl+F"))
                    .char_limit(256),
            ).on_hover_text(crate::ui::i18n::text("Exact logical cell IDs, lists and ranges. Search does not change LEDs. Enter/F3: next; Shift+F3: previous; Escape: clear."));
            if self.focus_cell_search {
                search.request_focus();
                self.focus_cell_search = false;
            }
            if search.has_focus() && ui.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::Escape)) {
                self.cell_search.clear();
                self.apply_cell_search_selection();
            }
            if search.changed() { self.apply_cell_search_selection(); }
            if search.lost_focus() && ui::cell_search::parse(&self.cell_search).is_ok() {
                self.cell_search = ui::cell_search::format_selection(&self.table.selection.ids);
            }
            let previous = ui.add_enabled(!self.table.selection.ids.is_empty(), egui::Button::new(icons::CARET_LEFT))
                .on_hover_text(crate::ui::i18n::text("Previous selected cell (Shift+F3)")).clicked();
            let next = ui.add_enabled(!self.table.selection.ids.is_empty(), egui::Button::new(icons::CARET_RIGHT))
                .on_hover_text(crate::ui::i18n::text("Next selected cell (Enter / F3)")).clicked()
                || (search.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter)));
            if (previous || next) && let Some(id) = self.table.selection.navigate(self.selected_cell, previous) {
                self.selected_cell = id;
                if search.lost_focus() { search.request_focus(); }
            }
            if ui.add_enabled(!self.cell_search.is_empty(), egui::Button::new(icons::X))
                .on_hover_text(crate::ui::i18n::text("Clear cell selection")).clicked() {
                self.cell_search.clear();
                self.apply_cell_search_selection();
            }
        });
        let search_result = ui::cell_search::parse(&self.cell_search);
        if let Err(error) = &search_result {
            ui.colored_label(ui.visuals().error_fg_color, error);
        }
        if !self.table.selection.ids.is_empty() {
            let cells = self.table.selection.cells();
            let current = cells.iter().position(|id| *id == self.selected_cell);
            ui.label(
                egui::RichText::new(match current {
                    Some(index) => format!(
                        "Selection {} of {} · logical cell {}",
                        index + 1,
                        cells.len(),
                        self.selected_cell
                    ),
                    None => format!("{} selected cells", cells.len()),
                })
                .small()
                .color(ui.visuals().warn_fg_color),
            );
        }
        self.cell_selection_toolbar(ui);
        ui.add_space(8.0);
        let width = ui.available_width().max(200.0);
        let visible_height = (ui.clip_rect().bottom() - ui.cursor().min.y - 38.0).max(180.0);
        let (pitch,height)=super::cell_table::grid_metrics(width,visible_height,
            self.config.cell_shape==super::cell_pixels::Shape::Rounded);
        if self.table.pixel_sample.as_ref().is_none_or(|sample|!sample.fresh()) {
            ui.weak("Showing acknowledged cell states · individual pixel RGB is unavailable.");
        }
        egui::ScrollArea::horizontal().id_salt("table-usable-cell-width").auto_shrink([false,true]).show(ui,|ui|{
            let grid_width=width.max(pitch*20.0);
            let (area,_)=ui.allocate_exact_size(egui::vec2(grid_width,height*4.5),egui::Sense::hover());
            let grid_origin=area.min+egui::vec2((grid_width-pitch*20.0)*0.5,0.0);
            self.draw_cell_grid(ui,area,grid_origin,pitch,height);
            finish_table_area(ui,area);
        });
        ui.horizontal(|ui| {
            ui.label(
                egui::RichText::new(format!(
                    "Cursor: cell {} · {} selected",
                    self.selected_cell,
                    self.table.selection.ids.len()
                ))
                .weak(),
            );
            if snapshot.is_none() {
                if ui.button(format!("{}  {}", icons::PLUG, crate::ui::i18n::text("Connect…"))).clicked() {
                    self.show_connection = true;
                }
            }
        });
    }
    fn draw_pixel_diagnostic(&mut self, ui: &mut egui::Ui) {
        if self.transport != Transport::WiFi {
            ui.weak("Individual-pixel diagnostics currently require the selected Wi-Fi raw-frame path. USB controls remain offline; no network fallback.");
            return;
        }
        let Some(status) = self.live_status().cloned().filter(pixel_diagnostic::supported) else {
            ui.weak("Connect to an acknowledged 480-pixel streaming board first."); return;
        };
        let Some(peer) = peer_http::Pin::from_status(&status) else { return; };
        let busy = self.pixel_diagnostic.pending.is_some();
        ui.weak("Manual only: one white wire pixel, all others black. No automatic stepping. Confirm effects mode first.");
        ui.horizontal_wrapped(|ui| {
            if self.streaming || self.sampling {
                if ui.button("Stop active streams").clicked() {
                    self.streaming = false; self.sampling = false;
                    let _ = self.ws_tx.send(WsCommand::Stream(false));
                    let _ = self.ws_tx.send(WsCommand::Sample(false));
                }
            }
            let can_arm = !busy && self.websocket_live && !self.streaming && !self.sampling && pixel_diagnostic::can_arm(&status);
            if ui.add_enabled(can_arm, egui::Button::new("Confirm individual-pixel mode")).clicked() {
                self.arm_pixel_diagnostic();
            }
            if !pixel_diagnostic::can_arm(&status) { ui.weak("Requires enabled output, Cell mode and no global effect."); }
        });
        self.pixel_diagnostic.index = self.pixel_diagnostic.index.clamp(1, 480);
        let ready = !busy && self.websocket_live && !self.streaming && !self.sampling
            && self.pixel_diagnostic.armed.as_ref() == Some(&peer) && pixel_diagnostic::ready(&status);
        let mut show = None;
        ui.horizontal_wrapped(|ui| {
            ui.label("Wire pixel (1–480)");
            ui.add_enabled(!busy, egui::DragValue::new(&mut self.pixel_diagnostic.index).range(1..=480));
            if ui.add_enabled(ready && self.pixel_diagnostic.index > 1, egui::Button::new("Previous")).clicked() {
                show = Some(self.pixel_diagnostic.index - 1);
            }
            if ui.add_enabled(ready, egui::Button::new("Show pixel")).clicked() { show = Some(self.pixel_diagnostic.index); }
            if ui.add_enabled(ready && self.pixel_diagnostic.index < 480, egui::Button::new("Next")).clicked() {
                show = Some(self.pixel_diagnostic.index + 1);
            }
            let raw = self.pixel_diagnostic.index - 1;
            let group = (raw / 6 + 1) as u8;
            let mapped = self.mappings.iter().find(|m| m.physical_cell == group)
                .map(|m| format!(" · reported logical cell {}", m.logical_id)).unwrap_or_else(|| " · logical mapping unavailable".into());
            ui.weak(format!("raw {raw} · wire group {group} · sector {}{mapped}", raw % 6 + 1));
        });
        if let Some(index) = show {
            self.show_pixel_diagnostic(index);
        }
        if !self.pixel_diagnostic.message.is_empty() { ui.label(&self.pixel_diagnostic.message); }
    }
    fn arm_pixel_diagnostic(&mut self) {
        let status = self.live_status().cloned();
        if self.command_route() != Some(Transport::WiFi) || !self.websocket_live || self.streaming || self.sampling
            || self.pixel_diagnostic.pending.is_some() || !status.as_ref().is_some_and(pixel_diagnostic::can_arm) {
            self.pixel_diagnostic.message = "Use the enabled Wi-Fi Cell board, stop global effects/streams, then confirm diagnostics.".into(); return;
        }
        let peer = peer_http::Pin::from_status(status.as_ref().unwrap()).unwrap();
        self.pixel_diagnostic.armed = None;
        let id = self.pixel_diagnostic.begin();
        self.pixel_diagnostic.message = "Confirming effects mode via WebSocket ACK and same-board readback…".into();
        let request = pixel_diagnostic::Request { id, index: 0, peer, endpoint: self.endpoint(), at: Instant::now() };
        if self.ws_tx.send(WsCommand::ArmPixel(request)).is_err() {
            self.pixel_diagnostic.finish(id, "WebSocket worker unavailable; no frame sent".into());
        }
    }
    fn show_pixel_diagnostic(&mut self, index: u16) {
        let status = self.live_status().cloned();
        let peer = status.as_ref().and_then(peer_http::Pin::from_status);
        if !(1..=480).contains(&index) || self.command_route() != Some(Transport::WiFi) || !self.websocket_live
            || self.streaming || self.sampling || self.pixel_diagnostic.pending.is_some()
            || peer.is_none() || self.pixel_diagnostic.armed != peer || !status.as_ref().is_some_and(pixel_diagnostic::ready) {
            self.pixel_diagnostic.message = "Confirm individual-pixel effects mode on the current Wi-Fi board before showing a pixel.".into(); return;
        }
        self.pixel_diagnostic.index = index;
        let id = self.pixel_diagnostic.begin();
        self.pixel_diagnostic.message = format!("Sending physical wire pixel {index} once…");
        let request = pixel_diagnostic::Request { id, index: index - 1, peer: peer.unwrap(), endpoint: self.endpoint(), at: Instant::now() };
        if self.ws_tx.send(WsCommand::Pixel(request)).is_err() {
            self.pixel_diagnostic.finish(id, "WebSocket worker unavailable; frame not sent".into());
        }
    }
    fn set_cell(&mut self, id: u8, state: &str, value: u8) {
        let effect = match self.cell_effect.as_str() {
            "fade" => 1,
            "pulse" => 2,
            _ => 0,
        };
        self.action(
            "/api/cell",
            vec![
                ("id".into(), id.to_string()),
                ("state".into(), state.into()),
                ("effect".into(), self.cell_effect.clone()),
            ],
            OP_CELL_ACTION,
            vec![id, value, effect],
        );
    }
    fn draw_effects(&mut self, ui: &mut egui::Ui) {
        let snapshot = self.live_status().cloned();
        let modes: Vec<_> = self.peer_context().map(|c| c.modes().filter(|mode|mode.id!="snake").cloned().collect()).unwrap_or_default();
        if !modes.is_empty() { dialog::section(ui, icons::PLAY, "Board modes", |ui| {
            ui.add_enabled_ui(snapshot.is_some(), |ui| {
                for mode in &modes {
                    let (label, name, value) = (mode.label.as_str(), mode.id.as_str(), mode.code);
                    ui.weak(if mode.kind == "presentation" { "Table presentation" } else { "Game" });
                    let selected = snapshot.as_ref().is_some_and(|s| s.mode == name);
                    if ui
                        .add_sized(
                            [ui.available_width(), 30.0],
                            egui::Button::new(label).selected(selected),
                        )
                        .clicked()
                    {
                        self.action(
                            "/api/mode",
                            vec![("value".into(), name.into())],
                            OP_SET_MODE,
                            vec![value],
                        );
                    }
                }
                if ui
                    .button(format!("{}  {}", icons::ARROW_COUNTER_CLOCKWISE, crate::ui::i18n::text("Reset game…")))
                    .clicked()
                {
                    self.confirm_reset = true;
                }
            });
        }); }
        ui.add_space(10.0);
        if self.has_table() {
        dialog::section(
            ui,
            icons::HAND_TAP,
            &format!("Cell {}", self.selected_cell),
            |ui| {
                if let Some(mapping) = self
                    .mappings
                    .iter()
                    .find(|m| m.logical_id == self.selected_cell)
                {
                    ui.label(
                        egui::RichText::new(format!(
                            "{} · physical {}",
                            mapping.name, mapping.physical_cell
                        ))
                        .weak(),
                    );
                }
                ui.add_enabled_ui(snapshot.is_some(), |ui| {
                    ui.horizontal(|ui| {
                        for (label, name, value) in
                            [("White", "white", 0), ("Red", "red", 1), ("Off", "off", 2)]
                        {
                            if ui.button(label).clicked() {
                                self.set_cell(self.selected_cell, name, value);
                            }
                        }
                    });
                    super::dropdown::ComboBox::from_label("Transition")
                        .selected_text(&self.cell_effect)
                        .show_ui(ui, |ui| {
                            for name in ["immediate", "fade", "pulse"] {
                                ui.dropdown_value(&mut self.cell_effect, name.into(), name);
                            }
                        });
                });
                if ui
                    .button(format!("{}  {}", icons::PENCIL_SIMPLE, crate::ui::i18n::text("Edit identity…")))
                    .clicked()
                {
                    self.select_mapping(self.selected_cell);
                    self.show_mapping = true;
                }
            },
        );
        }
        ui.add_space(10.0);
        self.draw_led_settings(ui);
        if modes.iter().any(|m| m.id == "snake") && snapshot.as_ref().is_some_and(|s| s.mode == "snake") {
            ui.add_space(10.0);
            dialog::section(ui, icons::ARROW_RIGHT, "Snake direction", |ui| {
                ui.horizontal(|ui| {
                    for (icon, direction) in [
                        (icons::ARROW_UP, "up"),
                        (icons::CARET_LEFT, "left"),
                        (icons::ARROW_DOWN, "down"),
                        (icons::ARROW_RIGHT, "right"),
                    ] {
                        if ui.button(icon).clicked() {
                            self.post(
                                "/api/snake/direction",
                                vec![("value".into(), direction.into())],
                            );
                        }
                    }
                });
            });
        }
    }
    pub(super) fn draw_led_settings(&mut self, ui: &mut egui::Ui) {
        let snapshot = self.live_status().cloned();
        if self.has_feature("leds") {
        dialog::section(ui, icons::SPARKLE, "Strip effect", |ui| {
            if let Some(mut enabled) = snapshot.as_ref()
                .and_then(|s| s.led_output.get("enabled")).and_then(|v| v.as_bool()) {
                ui.horizontal(|ui| {
                    if ui.checkbox(&mut enabled, "LED output service").changed() {
                        self.action("/api/led/output", vec![("enabled".into(), enabled.to_string())],
                            OP_LED_OUTPUT, vec![u8::from(enabled)]);
                    }
                    if ui.small_button("Off").on_hover_text("Send black again, even when output is already off.").clicked() {
                        self.action("/api/led/output", vec![("enabled".into(), "false".into())], OP_LED_OUTPUT, vec![0]);
                    }
                });
            } else {
                ui.label(egui::RichText::new("LED output service: Unknown").weak());
            }
            if let Some(observed) = snapshot.as_ref().and_then(led_output_mode) {
                let mut mode = observed.to_owned();
                super::dropdown::ComboBox::from_label("Output mode")
                    .selected_text(if mode == "static" { "Static grouped" } else { "Animated effects" })
                    .show_ui(ui, |ui| {
                        ui.dropdown_value(&mut mode, "static".into(), "Static grouped");
                        ui.dropdown_value(&mut mode, "effects".into(), "Animated effects");
                    });
                if mode != observed {
                    if let Some((form, payload)) = led_output_request(&mode) {
                        self.action("/api/led/output", form, OP_LED_OUTPUT, payload);
                    }
                }
                ui.weak(if observed == "static" { "Static grouped cells; six pixels share one color. Rainbow test still animates." }
                    else { "Animated effects are explicitly enabled." });
            } else { ui.weak("Output mode: Unknown"); }
            let observed_effect=snapshot.as_ref().map(|s|s.effect.as_str()).filter(|effect|!effect.is_empty());
            ui.label(format!("Current effect: {}",observed_effect.unwrap_or("Unknown")));
            let test_ready = snapshot.as_ref().is_some_and(|s|
                s.led_output.get("enabled").and_then(|v| v.as_bool()) == Some(true));
            if ui.add_enabled(test_ready, egui::Button::new(format!("{}  Rainbow test", icons::SPARKLE)).selected(observed_effect==Some("rainbow")))
                .on_hover_text("Rainbow animates in both modes, keeping each cell's six pixels together in static mode. Does not change output mode or enable the service.")
                .clicked() {
                self.action("/api/effect", vec![("value".into(), "rainbow".into())], OP_PATTERN, vec![1]);
            }
            ui.add_enabled_ui(snapshot.is_some(), |ui| {
                super::dropdown::ComboBox::from_id_salt("strip-effect")
                    .width(ui.available_width() - 10.0)
                    .selected_text(&self.pattern)
                    .show_ui(ui, |ui| {
                        for name in ["none", "rainbow", "fire"] {
                            ui.dropdown_value(&mut self.pattern, name.into(), name);
                        }
                    });
                if dialog::primary_action_button(ui, icons::PLAY, "Apply").clicked() {
                    let value = match self.pattern.as_str() {
                        "rainbow" => 1,
                        "fire" => 2,
                        _ => 0,
                    };
                    self.action(
                        "/api/effect",
                        vec![("value".into(), self.pattern.clone())],
                        OP_PATTERN,
                        vec![value],
                    );
                }
                if ui
                    .add(egui::Slider::new(&mut self.brightness, 1..=255).text("Brightness"))
                    .changed()
                {
                    self.action(
                        "/api/brightness",
                        vec![("value".into(), self.brightness.to_string())],
                        OP_BRIGHTNESS,
                        vec![self.brightness],
                    );
                }
            });
        });
        }
    }
    pub(super) fn draw_front_panel(&mut self, ui: &mut egui::Ui) {
        let snapshot = self.live_status().cloned();
        if self.has_feature("lcd") {
            if let Some(lcd) = snapshot.as_ref().and_then(|s| s.lcd.as_ref()) {
                if let Some(lines) = lcd.confirmed_lines() {
                    dialog::section(ui, icons::TEXT_ALIGN_LEFT, "LCD display", |ui| {
                        ui.label(egui::RichText::new(format!("I²C 0x{:02X} · 16×2", lcd.address.unwrap())).weak());
                        egui::Frame::group(ui.style()).show(ui, |ui| {
                            for line in lines { ui.monospace(line); }
                        });
                        if lcd.pending { ui.spinner(); }
                    });
                }
            }
        }
        if self.has_feature("display") {
        dialog::section(ui, icons::GAUGE, "Live display", |ui| {
            if let Some(s) = &snapshot {
                if s.tm1637_segments.len() == 4 {
                    ui::seven_segment::draw(
                        ui,
                        &s.tm1637_segments,
                        s.tm1637_brightness,
                        s.tm1637_ack,
                    );
                } else {
                    ui.label(crate::ui::i18n::text("Segment readback unavailable"));
                }
                badge(
                    ui,
                    if s.tm1637_ack {
                        "TM1637 acknowledged"
                    } else {
                        "No display acknowledgment"
                    },
                    s.tm1637_ack,
                );
            } else {
                ui.label(egui::RichText::new(crate::ui::i18n::text("Display state unavailable")).weak());
            }
            ui.horizontal(|ui| {
                ui.add(egui::DragValue::new(&mut self.display_value).range(-999..=9999));
                if ui.button(crate::ui::i18n::text("Show number")).clicked() {
                    self.action(
                        "/api/display",
                        vec![("value".into(), self.display_value.to_string())],
                        OP_DISPLAY,
                        self.display_value.to_le_bytes().to_vec(),
                    );
                }
            });
        });
        }
        ui.add_space(10.0);
        if self.has_feature("text") {
        dialog::section(ui, icons::TEXT_ALIGN_LEFT, "Text", |ui| {
            ui.add_sized(
                [ui.available_width(), 28.0],
                dialog::singleline_text_edit(&mut self.display_text)
                    .char_limit(40)
                    .hint_text(crate::ui::i18n::text("Front-panel text")),
            );
            ui.horizontal_wrapped(|ui| {
                ui.label(crate::ui::i18n::text("Step"));
                ui.add(
                    egui::DragValue::new(&mut self.config.display.speed_ms)
                        .range(80..=5000)
                        .suffix(" ms"),
                );
                ui.checkbox(&mut self.config.display.force_scroll, crate::ui::i18n::text("Scroll short text"));
            });
            ui.horizontal_wrapped(|ui| {
                ui.label(crate::ui::i18n::text("Repeat"));
                super::dropdown::ComboBox::from_id_salt("display-repeat")
                    .selected_text(self.config.display.repeat.label())
                    .show_ui(ui, |ui| {
                        for value in [
                            crate::display::Repeat::Once,
                            crate::display::Repeat::Loop,
                            crate::display::Repeat::Interval,
                        ] {
                            ui.dropdown_value(
                                &mut self.config.display.repeat,
                                value,
                                value.label(),
                            );
                        }
                    });
                if self.config.display.repeat == crate::display::Repeat::Interval {
                    ui.label(crate::ui::i18n::text("Pause"));
                    ui.add(
                        egui::DragValue::new(&mut self.config.display.interval_seconds)
                            .range(1..=255)
                            .suffix(" s"),
                    );
                }
                if self.display_text.len() <= 4
                    && !self.config.display.force_scroll
                    && self.config.display.repeat != crate::display::Repeat::Loop
                {
                    ui.label(crate::ui::i18n::text("Hold"));
                    ui.add(
                        egui::DragValue::new(&mut self.config.display.hold_ms)
                            .range(0..=60000)
                            .suffix(" ms"),
                    );
                }
            });
            let payload = self.config.display.payload(&self.display_text);
            if let Err(error) = &payload {
                ui.colored_label(ui.visuals().error_fg_color, error);
            }
            ui.horizontal(|ui| {
                if ui
                    .add_enabled(
                        snapshot.is_some() && payload.is_ok() && !self.display_text.is_empty(),
                        egui::Button::new(format!("{}  {}", icons::PLAY, crate::ui::i18n::text("Present"))),
                    )
                    .clicked()
                {
                    self.action(
                        "/api/display",
                        self.config.display.form(&self.display_text),
                        OP_DISPLAY,
                        payload.clone().unwrap(),
                    );
                    self.save_workspace();
                }
                if snapshot.as_ref().is_some_and(|s| s.tm1637_host_active)
                    && ui.button(crate::ui::i18n::text("Stop text")).clicked()
                {
                    self.action(
                        "/api/display",
                        vec![("release".into(), "true".into())],
                        OP_DISPLAY,
                        crate::display::RELEASE.to_vec(),
                    );
                }
            });
        });
        }
        ui.add_space(10.0);
        if self.has_feature("buzzer") {
        dialog::section(ui, icons::MUSIC_NOTE, "Melodies", |ui| {
            ui.add_enabled_ui(
                snapshot
                    .as_ref()
                    .is_some_and(|s| s.avr_online && !s.audio_cues.is_empty()),
                |ui| {
                    ui.horizontal_wrapped(|ui| {
                        for (cue, name) in audio::CUES {
                            if !snapshot
                                .as_ref()
                                .is_some_and(|s| s.audio_cues.iter().any(|v| v == cue))
                            {
                                continue;
                            }
                            if ui.button(format!("{}  {name}", icons::PLAY)).clicked() {
                                self.action(
                                    "/api/audio",
                                    vec![("cue".into(), cue.into())],
                                    OP_AUDIO,
                                    vec![audio::id(cue).expect("catalog ID")],
                                );
                            }
                        }
                    });
                },
            );
            if icons::disclosure_header(ui, "custom-tone", "Custom tone", false) {
                ui.horizontal(|ui| {
                    ui.add(
                        egui::DragValue::new(&mut self.buzzer_frequency)
                            .range(20..=12000)
                            .suffix(" Hz"),
                    );
                    ui.add(
                        egui::DragValue::new(&mut self.buzzer_duration)
                            .range(1..=5000)
                            .suffix(" ms"),
                    );
                });
                if ui
                    .add_enabled(
                        snapshot.is_some(),
                        egui::Button::new(format!("{}  {}", icons::PLAY, crate::ui::i18n::text("Play tone"))),
                    )
                    .clicked()
                {
                    let mut p = self.buzzer_frequency.to_le_bytes().to_vec();
                    p.extend(self.buzzer_duration.to_le_bytes());
                    self.action(
                        "/api/buzzer",
                        vec![
                            ("frequency".into(), self.buzzer_frequency.to_string()),
                            ("duration_ms".into(), self.buzzer_duration.to_string()),
                        ],
                        OP_BUZZER,
                        p,
                    );
                }
            }
        }); }
    }
    pub(super) fn draw_hardware(&mut self, ui: &mut egui::Ui) {
        let Some(s) = self.live_status().cloned() else {
            ui.label(crate::ui::i18n::text("Hardware state unavailable"));
            return;
        };
        let avr_acknowledged = ["display", "text", "buzzer", "inputs", "outputs", "settings"]
            .iter().any(|feature| self.has_feature(feature));
        dialog::section(ui, icons::CPU, "Controllers", |ui| {
            if avr_acknowledged { badge(ui, "AVR link", s.avr_online); }
            if self.has_feature("display") { badge(ui, "TM1637", s.tm1637_ack); }
            egui::Grid::new("controller-health")
                .num_columns(2)
                .spacing([16.0, 8.0])
                .show(ui, |ui| {
                    let mut values = vec![
                        ("Firmware", s.version.clone()),
                        ("Host", s.hostname.clone()),
                    ];
                    if avr_acknowledged { values.extend([
                        ("Link", s.link_mode.clone()),
                        (
                            "UART errors",
                            format!("ESP {} / AVR {}", s.esp_uart_errors, s.avr_uart_errors),
                        ),
                    ]); }
                    if self.has_feature("isp") { values.push(("Program latch", if s.programming_latch { "Active" } else { "Released" }.into())); }
                    if self.command_route() == Some(Transport::Serial) { values.push(("USB errors", s.usb_serial_errors.to_string())); }
                    for (name, value) in values {
                        ui.label(egui::RichText::new(name).weak());
                        ui.label(value);
                        ui.end_row();
                    }
                });
        });
        ui.add_space(10.0);
        if self.has_feature("inputs") { dialog::section(ui, icons::HAND_TAP, "Inputs", |ui| {
            ui.horizontal_wrapped(|ui| {
                for bit in 0..9 {
                    let active = if bit == 8 {
                        s.input_9
                    } else {
                        s.active_inputs & (1 << bit) != 0
                    };
                    badge(ui, &format!("{}", bit + 1), active);
                }
            });
        }); }
        ui.add_space(10.0);
        if self.has_feature("outputs") { dialog::section(ui, icons::LIGHTBULB, "Shift-register outputs", |ui| {
            ui.add_enabled_ui(s.avr_online, |ui| {
                ui.horizontal_wrapped(|ui| {
                    for bit in 0..8 {
                        let mut on = s.active_outputs & (1 << bit) != 0;
                        if ui.checkbox(&mut on, (bit + 1).to_string()).changed() {
                            let mask = if on {
                                s.active_outputs | (1 << bit)
                            } else {
                                s.active_outputs & !(1 << bit)
                            };
                            self.action(
                                "/api/outputs",
                                vec![("mask".into(), mask.to_string())],
                                OP_OUTPUT_MASK,
                                vec![mask],
                            );
                        }
                    }
                });
            });
        }); }
    }
    pub(super) fn draw_device_identity(&mut self, ui: &mut egui::Ui) {
        let writable = self.command_route() == Some(Transport::Serial) || self.http_writable();
        dialog::section(ui, icons::GLOBE, "Board identity", |ui| {
            if let Some(app) = self.peer_context().and_then(|p|p.application()) {
                ui.label(egui::RichText::new(&app.name).strong());
                ui.label(egui::RichText::new(format!("{} · {}",app.title,app.id)).small().weak());
                ui.label(egui::RichText::new(crate::ui::i18n::text("Application branding is reported by the running firmware; renaming this board does not change its software identity.")).small().weak());
            }
            if self.command_route()==Some(Transport::Serial) && self.peer_context().is_none_or(|c|c.hostname.is_empty()) {
                ui.label(crate::ui::i18n::text("Reading this USB board’s identity…")); return;
            }
            if let Some(name) = &mut self.device_name {
                if let Some(label) = &mut self.board_display_name { field(ui, icons::CIRCUITRY, "Board display name", label, false); }
                field(ui, icons::GLOBE, "Hostname", name, false);
                ui.label(egui::RichText::new(crate::ui::i18n::text("The display name is separate from the DNS hostname. Saved across OTA. Only a hostname change restarts Wi-Fi/AP; credentials stay unchanged.")).small().weak());
                let label = self.board_display_name.clone();
                if ui.add_enabled_ui(writable && !self.identity_pending, |ui| dialog::primary_action_button(ui, icons::CHECK, "Save board identity")).inner.clicked() {
                    match crate::identity::normalize_name(name).and_then(|n| if label.as_ref().is_some_and(|s| !crate::identity::valid_display_name(s)) { Err("Board name must be 1..63 UTF-8 bytes without controls or edge spaces".into()) } else { Ok(n) }) {
                        Ok(name) => {
                            self.identity_pending = true;
                            if self.command_route() == Some(Transport::Serial) {
                                if let Some(admission)=self.serial_admission().filter(|a|a.peer.is_some()){
                                    let _=self.serial_tx.send(crate::SerialCommand::Identity(self.serial_port.clone(),admission,name,label.unwrap_or_default()));
                                }else{self.identity_pending=false;self.message="Read this USB board’s verified identity before saving; nothing queued".into();}
                            } else {
                                let _ = self.http_tx.send(crate::HttpCommand::NameChange(self.endpoint(), name, label));
                            }
                        }
                        Err(error) => self.notify(
                            "Invalid device name",
                            &error,
                            crate::messaging::Severity::Warning,
                        ),
                    }
                }
                if let Some((host,label))=&self.identity_saved {
                    if self.device_name.as_ref()!=Some(host) || self.board_display_name.as_ref()!=Some(label) {
                        ui.label(egui::RichText::new(crate::ui::i18n::text("Unsaved board identity changes")).small().weak());
                        if dialog::action_button(ui,icons::X,"Discard name edits").clicked() {
                            self.device_name=Some(host.clone());self.board_display_name=Some(label.clone());
                        }
                    }
                }
            } else {
                ui.label(if self.live_status().is_none() {
                    "Device offline. Connect to view its name."
                } else {
                    "Device name unavailable."
                });
            }
        });
    }
    pub(super) fn draw_device_access(&mut self, ui: &mut egui::Ui) {
        let writable = self.http_writable();
        dialog::section(ui, icons::LOCK, "Device access", |ui| {
            if let Some(info) = &self.access_info {
                if info.get("ready").and_then(|v| v.as_bool()) != Some(true) {
                    ui.colored_label(
                        ui.visuals().error_fg_color,
                        "Device access storage is unavailable. Physical USB recovery is required.",
                    );
                    return;
                }
                ui.add_enabled_ui(writable, |ui| { ui.checkbox(&mut self.access_enabled, "Require authentication"); });
                if self.access_enabled && info.get("default_credentials").and_then(|v| v.as_bool()) == Some(true) {
                    ui.colored_label(
                        ui.visuals().warn_fg_color,
                        "Default login: admin / pakarcontroller. Change it here.",
                    );
                }
                if self.access_enabled { field(
                    ui,
                    icons::LOCK,
                    "New username",
                    &mut self.access_user,
                    false,
                );
                field(
                    ui,
                    icons::LOCK,
                    "New password",
                    &mut self.access_password,
                    true,
                );
                field(
                    ui,
                    icons::LOCK,
                    "Confirm password",
                    &mut self.access_confirm,
                    true,
                ); }
                ui.weak(if self.access_enabled { "Saved on the device across OTA. Existing clients reconnect after a change." } else { "Authentication is disabled. No username or password is required." });
                if ui.add_enabled_ui(writable, |ui| dialog::primary_action_button(ui, icons::CHECK, "Save device access"))
                    .inner.clicked()
                {
                    match if self.access_enabled { crate::access::validate(
                        &self.access_user,
                        &self.access_password,
                        &self.access_confirm,
                    ) } else { Ok(()) } {
                        Ok(()) => {
                            let next = crate::Endpoint {
                                base: self.base.clone(),
                                user: if self.access_enabled { self.access_user.clone() } else { String::new() },
                                password: if self.access_enabled { self.access_password.clone() } else { String::new() },
                            };
                            let _ = self
                                .http_tx
                                .send(crate::HttpCommand::AccessChange(self.endpoint(), next, self.access_enabled));
                        }
                        Err(error) => self.notify(
                            "Cannot save credentials",
                            &error,
                            crate::messaging::Severity::Warning,
                        ),
                    }
                }
            } else {
                ui.label(crate::ui::i18n::text("Credential settings have not been advertised by the device. Connect to the updated firmware to configure them."));
            }
        });
    }
    fn draw_network(&mut self, ui: &mut egui::Ui) {
        dialog::section(ui, icons::GEAR, "Device configuration", |ui| {
            ui.label(crate::ui::i18n::text("Manage the controller's saved settings, network services and front panel."));
            if dialog::primary_action_button(ui, icons::GEAR, "Device settings…").clicked() {
                self.device_settings.open = true;
                self.configuration_loaded = false;
            }
        });
        if let Some(status) = self.live_status().cloned() {
            ui.add_space(12.0);
            dialog::section(ui, icons::BROADCAST, "Live services", |ui| {
                ui.horizontal_wrapped(|ui| {
                    badge(ui, "Station", status.station_connected);
                    badge(ui, "AP", status.ap_started);
                    badge(ui, "NTP", status.ntp_synced);
                    badge(ui, "MQTT", status.mqtt_connected);
                    badge(ui, "Cloud WS", status.cloud_websocket_connected);
                });
                ui.hyperlink_to(&status.canonical_url, &status.canonical_url);
                if !status.cloud_error.is_empty() {
                    ui.colored_label(ui.visuals().error_fg_color, status.cloud_error);
                }
            });
        }
    }
    fn draw_streaming(&mut self, ui: &mut egui::Ui) {
        ui.horizontal_wrapped(|ui| {
            ui.label(crate::ui::i18n::text("Target rate"));
            if ui
                .add(
                    egui::DragValue::new(&mut self.config.stream_fps)
                        .range(0..=streaming::MAX_FPS)
                        .suffix(" FPS"),
                )
                .changed()
            {
                let _ = self.ws_tx.send(WsCommand::Rate(self.config.stream_fps));
            }
            ui.weak("0 = uncapped; shared by rainbow and screen sampler");
        });
        if let Some((fps, at)) = self
            .stream_sent_fps
            .filter(|(_, at)| at.elapsed() < Duration::from_secs(3))
        {
            let _ = at;
            ui.label(format!(
                "Sent: {fps:.1} frames/s • complete WebSocket writes, not measured LED refresh"
            ));
        } else {
            ui.weak("Sent rate unavailable. Connect before starting a stream.");
        }
        ui.horizontal_wrapped(|ui| {
            badge(ui, "WebSocket", self.websocket_live);
            if ui
                .add_enabled(
                    self.websocket_live,
                    egui::Button::new(format!(
                        "{}  {}",
                        if self.streaming {
                            icons::STOP_CIRCLE
                        } else {
                            icons::PLAY
                        },
                        if self.streaming {
                            "Stop rainbow stream"
                        } else {
                            "Stream rainbow"
                        }
                    )),
                )
                .clicked()
            {
                self.streaming = !self.streaming;
                if self.streaming {
                    self.sampling = false;
                }
                let _ = self.ws_tx.send(WsCommand::Stream(self.streaming));
            }
            if ui
                .add_enabled(
                    self.websocket_live
                        && (self.sampling || streaming::valid_mapping(&self.mappings)),
                    egui::Button::new(format!(
                        "{}  {}",
                        icons::MONITOR_PLAY,
                        if self.sampling {
                            "Stop screen sampler"
                        } else {
                            "Sample primary screen"
                        }
                    )),
                )
                .on_hover_text(crate::ui::i18n::text("Requires a complete validated cell map from this board"))
                .clicked()
            {
                self.sampling = !self.sampling;
                if self.sampling {
                    self.streaming = false;
                }
                let _ = self.ws_tx.send(WsCommand::Sample(self.sampling));
            }
        });
    }
    fn draw_events(&mut self, ui: &mut egui::Ui) {
        ui.horizontal(|ui| {
            ui.label(egui::RichText::new(format!("{} events", self.event_log.len())).weak());
            if ui
                .button(icons::TRASH)
                .on_hover_text(crate::ui::i18n::text("Clear events"))
                .clicked()
            {
                self.event_log.clear();
            }
        });
        for event in self.event_log.iter().rev() {
            let kind = &event.kind;
            ui.horizontal_wrapped(|ui| {
                ui.label(egui::RichText::new(kind).color(if kind == "Error" {
                    ui.visuals().error_fg_color
                } else {
                    ui.visuals().weak_text_color()
                }));
                ui.label(&event.message);
                if event.occurrences > 1 {
                    ui.label(egui::RichText::new(format!("×{}", event.occurrences)).weak())
                        .on_hover_text(format!("{} occurrences; last seen {:.0}s ago; {}",
                            event.occurrences, event.last_seen.elapsed().as_secs_f64(),
                            if event.active { "unresolved diagnostic" } else { "closed diagnostic" }));
                }
            });
        }
    }
    pub(super) fn select_mapping(&mut self, id: u8) {
        self.mapping_logical = id;
        if let Some(m) = self.mappings.iter().find(|m| m.logical_id == id) {
            self.mapping_physical = m.physical_cell;
            self.mapping_name = m.name.clone();
        }
    }
    fn draw_dialogs(&mut self, ctx: &egui::Context) {
        if self.confirm_reset {
            let response = egui::Modal::new(egui::Id::new("reset-game")).show(ctx, |ui| {
                ui.set_width(360.0);
                ui.heading(crate::ui::i18n::text("Reset the current game?"));
                ui.label(crate::ui::i18n::text("The winner and current game state will be cleared."));
                ui.add_space(12.0);
                dialog::action_row(ui, false, |ui| {
                    if dialog::primary_action_button(ui, icons::ARROW_COUNTER_CLOCKWISE, "Reset")
                        .clicked()
                    {
                        self.action("/api/game/reset", vec![], OP_RESET, vec![]);
                        self.confirm_reset = false;
                    }
                    if dialog::action_button(ui, icons::X, "Cancel").clicked() {
                        self.confirm_reset = false;
                    }
                });
            });
            if response.should_close() {
                self.confirm_reset = false;
            }
        }
        if self.show_connection {
            let mut method = self.transport;
            let mut draft = ui::connection_controls::Draft {
                endpoint: self.base.clone(), user: self.user.clone(),
                password: self.password.clone(), port: self.serial_port.clone(),
            };
            let mut lifecycle = None;
            let response = egui::Modal::new(egui::Id::new("connection-dialog")).show(ctx, |ui| {
                ui.set_width(480.0);
                ui.heading(format!("{}  {}", icons::PLUG, crate::ui::i18n::text("Connect to controller")));
                ui.add_space(12.0);
                ui::connection_controls::method_picker(ui, &mut method, self.identity_pending);
                ui.add_space(10.0);
                ui::connection_controls::details(ui, method,
                    !self.identity_pending && !self.connection_enabled,
                    crate::access::requirement(&self.access_requirement, &draft.endpoint), &mut draft,
                    || serialport::available_ports()
                        .map(|ports| ports.into_iter().map(|port| port.port_name).collect())
                        .map_err(|error| error.to_string()));
                if self.identity_pending {
                    ui.weak("Identity write in progress; connection changes are locked.");
                } else if self.connection_enabled {
                    ui.weak("Disconnect before editing the endpoint or credentials.");
                }
                ui.add_space(12.0);
                dialog::action_row(ui, false, |ui| {
                    let endpoint_present = match method {
                        Transport::WiFi => !draft.endpoint.trim().is_empty(),
                        Transport::Serial => !draft.port.trim().is_empty(),
                    };
                    if ui.add_enabled_ui(!self.identity_pending && (self.connection_enabled || endpoint_present), |ui|
                        dialog::primary_action_button(ui, icons::PLUG,
                            if self.connection_enabled { "Disconnect" } else { "Connect" })
                    ).inner.clicked() {
                        lifecycle = Some(!self.connection_enabled);
                        self.show_connection = false;
                    }
                    if dialog::action_button(ui, icons::X, "Close").clicked() {
                        self.show_connection = false;
                    }
                });
            });
            let connection_draft_changed = draft.endpoint != self.base || draft.user != self.user
                || draft.password != self.password || draft.port != self.serial_port;
            let connection_change_requested = method != self.transport || connection_draft_changed || lifecycle.is_some();
            if connection_change_requested && !self.connection_change_guard() {
                if method != self.transport {
                    self.set_connection(method, false);
                    // Switching the method is a deliberate disconnect, never
                    // a same-frame implicit connect through another worker.
                    lifecycle = None;
                }
                if method == self.transport && !self.connection_enabled && connection_draft_changed {
                    self.base = draft.endpoint;
                    self.user = draft.user;
                    self.password = draft.password;
                    self.serial_port = draft.port;
                    self.config.endpoint = self.base.clone();
                    self.config.user = self.user.clone();
                    self.config.serial_port = self.serial_port.clone();
                    self.save_workspace();
                }
                if method == self.transport {
                    if let Some(enabled) = lifecycle { self.set_connection(method, enabled); }
                }
            }
            if response.should_close() {
                self.show_connection = false;
            }
        }
        if self.show_mapping {
            let mut open = true;
            let g = dialog::bounded_geometry(
                ctx.content_rect(),
                20.0,
                egui::vec2(700.0, 560.0),
                egui::vec2(520.0, 380.0),
                egui::vec2(950.0, 750.0),
            );
            egui::Window::new(format!("{}  {}", icons::GRID_FOUR, crate::ui::i18n::text("Cell mapping")))
                .id(egui::Id::new("cell-mapping"))
                .open(&mut open)
                .default_rect(g.default_rect)
                .min_size(g.min_size)
                .max_size(g.max_size)
                .constrain_to(g.bounds)
                .collapsible(false)
                .frame(dialog::opaque_window_frame_from_context(ctx))
                .show(ctx, |ui| {
                    ui.weak(self.mapping_status());
                    ui.horizontal(|ui| {
                        ui.label(crate::ui::i18n::text("Logical"));
                        let changed = ui
                            .add(egui::DragValue::new(&mut self.mapping_logical).range(1..=80))
                            .changed();
                        if changed {
                            self.select_mapping(self.mapping_logical);
                        }
                        ui.label(crate::ui::i18n::text("Physical"));
                        ui.add(egui::DragValue::new(&mut self.mapping_physical).range(1..=80));
                        ui.label(crate::ui::i18n::text("Name"));
                        ui.add_sized(
                            [ui.available_width().min(180.0), 28.0],
                            dialog::singleline_text_edit(&mut self.mapping_name).char_limit(15),
                        );
                    });
                    ui.add_space(8.0);
                    dialog::scroll_column(
                        ui,
                        "mapping-list",
                        Some((ui.available_height() - 50.0).max(200.0)),
                        |ui| {
                            egui::Grid::new("mapping-grid")
                                .num_columns(3)
                                .striped(true)
                                .spacing([24.0, 6.0])
                                .show(ui, |ui| {
                                    ui.strong(crate::ui::i18n::text("Logical cell"));
                                    ui.strong(crate::ui::i18n::text("Physical cell"));
                                    ui.strong(crate::ui::i18n::text("Name"));
                                    ui.end_row();
                                    for m in self.mappings.clone() {
                                        if ui
                                            .selectable_label(
                                                self.mapping_logical == m.logical_id,
                                                m.logical_id.to_string(),
                                            )
                                            .clicked()
                                        {
                                            self.select_mapping(m.logical_id);
                                        }
                                        ui.label(m.physical_cell.to_string());
                                        ui.label(&m.name);
                                        ui.end_row();
                                    }
                                });
                        },
                    );
                    ui.separator();
                    dialog::action_row(ui, false, |ui| {
                        if ui
                            .add_enabled(
                                !self.mappings.is_empty() && self.command_route()==Some(Transport::WiFi),
                                egui::Button::new(format!("{}  {}", icons::CHECK, crate::ui::i18n::text("Save mapping")))
                                    .min_size(egui::vec2(104.0, 28.0)),
                            )
                            .clicked()
                        {
                            self.post(
                                "/api/cells/mapping",
                                vec![
                                    ("logical_id".into(), self.mapping_logical.to_string()),
                                    ("physical_cell".into(), self.mapping_physical.to_string()),
                                    ("name".into(), self.mapping_name.clone()),
                                ],
                            );
                            self.last_mapping_poll = Instant::now() - Duration::from_secs(30);
                        }
                        if dialog::action_button(ui, icons::X, "Close").clicked() {
                            self.show_mapping = false;
                        }
                    });
                });
            if !open {
                self.show_mapping = false;
            }
        }
        if self.show_workspaces {
            self.draw_workspaces(ctx);
        }
        if self.show_about {
            let mut open = true;
            let app = self.peer_context().and_then(|p|p.application()).cloned();
            let name = app.as_ref().map(|a|a.name.as_str()).unwrap_or("PakarController");
            egui::Window::new(format!("{}  About {name}",icons::INFO)).id(egui::Id::new("about-pakar")).open(&mut open).default_size([520.0,300.0]).collapsible(false).frame(dialog::opaque_window_frame_from_context(ctx)).show(ctx,|ui|{
                ui.horizontal(|ui|{crate::branding::peer_logo(ui,48.0,app.as_ref());ui.vertical(|ui|{ui.heading(name);if let Some(app)=&app {ui.label(&app.title);ui.label(egui::RichText::new(&app.id).weak());}ui.label(format!("PakarController client {}",env!("CARGO_PKG_VERSION")));});});ui.add_space(16.0);
                dialog::section(ui,icons::APP_WINDOW,"Application foundation",|ui|{ui.label(crate::ui::i18n::text("Built from Pealayer’s desktop workspace, dialog controls, icon and color pickers, themes, fonts, notifications, and native instance ownership."));ui.label(crate::ui::i18n::text("MIT License · Pealayer © 2026 ToghrolTP"));ui.hyperlink_to("Pealayer source","https://github.com/ToghrolTP/pealayer");});ui.add_space(12.0);
                dialog::action_row(ui,false,|ui|{if dialog::action_button(ui,icons::X,"Close").clicked(){self.show_about=false;}});
            });
            if !open {
                self.show_about = false;
            }
        }
    }
    fn draw_workspaces(&mut self, ctx: &egui::Context) {
        let mut open = true;
        let g = dialog::bounded_geometry(
            ctx.content_rect(),
            20.0,
            egui::vec2(760.0, 480.0),
            egui::vec2(520.0, 320.0),
            egui::vec2(1000.0, 700.0),
        );
        egui::Window::new(format!("{}  {}", icons::TABS, crate::ui::i18n::text("Workspaces")))
            .id(egui::Id::new("workspace-profiles"))
            .open(&mut open)
            .default_rect(g.default_rect)
            .min_size(g.min_size)
            .max_size(g.max_size)
            .constrain_to(g.bounds)
            .collapsible(false)
            .frame(dialog::opaque_window_frame_from_context(ctx))
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.add_sized(
                        [ui.available_width() - 240.0, 28.0],
                        dialog::singleline_text_edit(&mut self.workspace_name)
                            .hint_text(crate::ui::i18n::text("Workspace name")),
                    );
                    icons::searchable_workspace_icon_picker(
                        ui,
                        "workspace-icon",
                        &mut self.workspace_icon,
                        90.0,
                        "Search icons",
                        "Workspace icons",
                        "No matching icons",
                        config::resolved_language(&self.config),
                    );
                    if dialog::primary_action_button(ui, icons::PLUS, "Save new").clicked()
                        && !self.workspace_name.trim().is_empty()
                    {
                        if let Ok(layout) = layout::serialize(&self.dock) {
                            self.config.workspace_profiles.insert(
                                uuid::Uuid::new_v4().to_string(),
                                config::WorkspaceProfile {
                                    name: self.workspace_name.trim().into(),
                                    icon: self.workspace_icon.clone(),
                                    layout,
                                },
                            );
                            self.workspace_name.clear();
                            self.save_workspace();
                        }
                    }
                });
                ui.add_space(8.0);
                ui.separator();
                dialog::scroll_column(
                    ui,
                    "workspace-list",
                    Some((ui.available_height() - 50.0).max(180.0)),
                    |ui| {
                        for (id, profile) in self.config.workspace_profiles.clone() {
                            egui::Frame::group(ui.style())
                                .inner_margin(egui::Margin::symmetric(10, 7))
                                .corner_radius(8.0)
                                .show(ui, |ui| {
                                    ui.horizontal(|ui| {
                                        ui.label(
                                            egui::RichText::new(icons::workspace_icon(
                                                &profile.icon,
                                            ))
                                            .color(ui.visuals().selection.bg_fill),
                                        );
                                        let mut name = profile.name.clone();
                                        if ui
                                            .add_sized(
                                                [ui.available_width() - 235.0, 28.0],
                                                dialog::singleline_text_edit(&mut name),
                                            )
                                            .changed()
                                            && !name.trim().is_empty()
                                        {
                                            self.config
                                                .workspace_profiles
                                                .get_mut(&id)
                                                .unwrap()
                                                .name = name;
                                        }
                                        if ui
                                            .button(format!(
                                                "{}  Restore",
                                                icons::ARROW_COUNTER_CLOCKWISE
                                            ))
                                            .clicked()
                                        {
                                            self.dock = layout::restore(&profile.layout);
                                            self.save_workspace();
                                        }
                                        if ui
                                            .button(icons::FLOPPY_DISK)
                                            .on_hover_text(crate::ui::i18n::text("Replace with current layout"))
                                            .clicked()
                                        {
                                            if let Ok(layout) = layout::serialize(&self.dock) {
                                                self.config
                                                    .workspace_profiles
                                                    .get_mut(&id)
                                                    .unwrap()
                                                    .layout = layout;
                                                self.save_workspace();
                                            }
                                        }
                                        if ui
                                            .button(icons::TRASH)
                                            .on_hover_text(crate::ui::i18n::text("Delete workspace"))
                                            .clicked()
                                        {
                                            self.config.workspace_profiles.remove(&id);
                                            self.save_workspace();
                                        }
                                    });
                                });
                            ui.add_space(4.0);
                        }
                    },
                );
                dialog::action_row(ui, false, |ui| {
                    if dialog::action_button(ui, icons::X, "Close").clicked() {
                        self.show_workspaces = false;
                        self.save_workspace();
                    }
                });
            });
        if !open {
            self.show_workspaces = false;
            self.save_workspace();
        }
    }
}
