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
use platform::instance::Command;

pub fn launch_command(args: &[String]) -> Command {
    if let Some(at) = args.iter().position(|v| v == "--screenshot") {
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
    // Absolute ui.put calls move the cursor to the last (unshifted) cell.
    // Restore the complete 8.5-row extent before placing the footer.
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
                    egui::vec2(pitch * 10.0, height * 8.5),
                    egui::Sense::hover(),
                );
                let last_cell = egui::Rect::from_min_size(
                    area.min + egui::vec2(pitch * 9.0, height * 7.0),
                    egui::vec2(pitch - 7.0, height - 7.0),
                );
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
            Command::Show | Command::Status => {}
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
        self.event_log
            .push((kind.into(), message.chars().take(2048).collect()));
        if self.event_log.len() > 256 {
            self.event_log.remove(0);
        }
    }
    pub fn save_workspace(&mut self) {
        if let Ok(json) = layout::serialize(&self.dock) {
            self.config.dock_layout = json;
        }
        if config::AppConfig::load().ok().as_ref() != Some(&self.config) {
            if let Err(error) = self.config.save() {
                self.message = error;
            }
        }
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
    fn synchronize_context(&mut self, ctx: &egui::Context) {
        let available: Vec<_> = Panel::ALL.into_iter().filter(|p| p.available(self.peer_context())).collect();
        let key = self.peer_context().map(|c| format!("{:?}:{}:{}:{}", self.command_route(),
            if self.command_route() == Some(Transport::Serial) { &self.serial_port } else { &self.base }, c.board_kind, c.name));
        if key != self.active_context {
            if let Some(previous) = self.active_context.take() {
                // Runtime-only per-peer workspace cache. No discovery-driven SSD writes.
                if self.context_layouts.len() >= 16 { self.context_layouts.clear(); }
                self.context_layouts.insert(previous, self.dock.clone());
            }
            self.dock = key.as_ref().and_then(|key| self.context_layouts.get(key)).cloned()
                .unwrap_or_else(|| if key.is_some() { layout::restore(&self.config.dock_layout) }
                    else { egui_dock::DockState::new(vec![Panel::Events]) });
            self.active_context = key;
            self.device_name = self.peer_context().and_then(|c|crate::identity::normalize_name(&c.hostname).ok());
            self.board_display_name = self.peer_context().filter(|c|crate::identity::valid_display_name(&c.name)).map(|c|c.name.clone());
            self.identity_saved = self.device_name.clone().zip(self.board_display_name.clone());
            self.configuration_loaded = false;
            self.context_panels = available.clone();
            self.table.stop_pending_edits();
            self.table.selection.ids.clear(); self.cell_search.clear();
            self.table.pixel_sample = None; self.mappings.clear();
            self.last_mapping_poll = Instant::now() - Duration::from_secs(16);
            let _ = self.ws_tx.send(WsCommand::Mapping(Vec::new()));
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
    pub fn draw_shell(&mut self, ui: &mut egui::Ui) {
        ui.with_layout(super::i18n::vertical_layout(super::i18n::rtl()), |ui| {
            self.draw_shell_contents(ui);
        });
    }
    fn draw_shell_contents(&mut self, ui: &mut egui::Ui) {
        let ctx = ui.ctx().clone();
        while let Ok(command) = self.control.rx.try_recv() {
            self.dispatch(command, &ctx);
        }
        if let Some(action) = self.tray.as_mut().and_then(|tray| tray.poll()) {
            match action {
                platform::windows_tray::TrayAction::Show => self.dispatch(Command::Show, &ctx),
                platform::windows_tray::TrayAction::Preferences => {
                    self.dispatch(Command::Preferences, &ctx)
                }
                platform::windows_tray::TrayAction::DeviceSettings => {
                    self.dispatch(Command::DeviceSettings, &ctx)
                }
                platform::windows_tray::TrayAction::Workspaces => {
                    self.dispatch(Command::Workspaces, &ctx)
                }
                platform::windows_tray::TrayAction::Refresh => self.refresh(),
                platform::windows_tray::TrayAction::Exit => self.dispatch(Command::Quit, &ctx),
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
        self.poll();
        self.synchronize_context(&ctx);
        self.process_cell_queue(&ctx);
        self.deliver_notifications();
        #[cfg(windows)]
        if let Some(desktop) = &mut self.windows_desktop {
            let errors = desktop.sync(
                ui.visuals().dark_mode,
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
            value["peer_capabilities"] = serde_json::json!(self.peer_context());
            value["cell_management"] = serde_json::json!({"open":self.table.manager_open,"selection":self.table.selection.ids,"additive_selection":self.config.additive_cell_selection,"animations":self.config.animate_cells,"indicators":self.config.show_cell_indicators});
            value["screenshot"] = self.capture.status();
            value["notifications"] = self.notification_snapshot();
            value["tray_available"] = serde_json::json!(self.tray.is_some());
            value["fullscreen"] = serde_json::json!(fullscreen);
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
                peer_known: snapshot.is_some(),
                avr_available: ["display", "text", "buzzer", "inputs", "outputs", "settings"].iter().any(|v| self.has_feature(v)),
                display_available: self.has_feature("display"),
                network_available: self.has_feature("network"),
                telemetry_available: self.has_feature("telemetry"),
                message: &self.message,
                hostname: &s.hostname,
                station: &s.station_ssid,
                avr_online: s.avr_online,
                tm1637_ready: s.tm1637_ack,
                websocket_live: self.websocket_live,
                serial_live: self.serial_live,
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
                    ui::connection_controls::websocket(ui, self.websocket_live, &self.base,
                        self.transport, &mut connection_actions);
                    ui::connection_controls::usb(ui, self.serial_live, &self.serial_port,
                        self.transport, self.identity_pending, || serialport::available_ports()
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
                Action::Route(route) => { self.transport = route; self.refresh(); }
                Action::Port(port) if !self.identity_pending => {
                    self.serial_port = port;
                    self.config.serial_port = self.serial_port.clone();
                    self.save_workspace();
                }
                Action::ReconnectWebSocket => { self.last_ws_endpoint = None; self.refresh(); }
                Action::ReconnectUsb if !self.identity_pending && !self.serial_port.is_empty() => {
                    // A same-port request closes/reopens the worker too. Clear the
                    // old ACK immediately; reopening is not a successful connection.
                    self.serial_status = None;
                    self.last_serial_status_at = None;
                    self.serial_live = false;
                    let _ = self.serial_tx.send(SerialCommand::Port(self.serial_port.clone()));
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
        let aspect = if self.config.cell_shape == super::cell_pixels::Shape::Rounded { 0.72 } else { 1.0 };
        let pitch = (width / 10.0).min(visible_height / (8.5 * aspect)).min(94.0);
        let height = (pitch * aspect).max(20.0);
        let (area, _) =
            ui.allocate_exact_size(egui::vec2(width, height * 8.5), egui::Sense::hover());
        let grid_origin = area.min + egui::vec2((width - pitch * 10.0) * 0.5, 0.0);
        self.draw_cell_grid(ui, area, grid_origin, pitch, height);
        finish_table_area(ui, area);
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
        let modes: Vec<_> = self.peer_context().map(|c| c.modes().cloned().collect()).unwrap_or_default();
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
                    egui::ComboBox::from_label("Transition")
                        .selected_text(&self.cell_effect)
                        .show_ui(ui, |ui| {
                            for name in ["immediate", "fade", "pulse"] {
                                ui.selectable_value(&mut self.cell_effect, name.into(), name);
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
        if self.has_feature("leds") {
        dialog::section(ui, icons::SPARKLE, "Strip effect", |ui| {
            ui.add_enabled_ui(snapshot.is_some(), |ui| {
                egui::ComboBox::from_id_salt("strip-effect")
                    .width(ui.available_width() - 10.0)
                    .selected_text(&self.pattern)
                    .show_ui(ui, |ui| {
                        for name in ["none", "rainbow", "fire"] {
                            ui.selectable_value(&mut self.pattern, name.into(), name);
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
                    .add(egui::Slider::new(&mut self.brightness, 0..=255).text("Brightness"))
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
    pub(super) fn draw_front_panel(&mut self, ui: &mut egui::Ui) {
        let snapshot = self.live_status().cloned();
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
                egui::ComboBox::from_id_salt("display-repeat")
                    .selected_text(self.config.display.repeat.label())
                    .show_ui(ui, |ui| {
                        for value in [
                            crate::display::Repeat::Once,
                            crate::display::Repeat::Loop,
                            crate::display::Repeat::Interval,
                        ] {
                            ui.selectable_value(
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
                if ui.add_enabled_ui(!self.identity_pending, |ui| dialog::primary_action_button(ui, icons::CHECK, "Save board identity")).inner.clicked() {
                    match crate::identity::normalize_name(name).and_then(|n| if label.as_ref().is_some_and(|s| !crate::identity::valid_display_name(s)) { Err("Board name must be 1..63 UTF-8 bytes without controls or edge spaces".into()) } else { Ok(n) }) {
                        Ok(name) => {
                            self.identity_pending = true;
                            if self.command_route() == Some(Transport::Serial) {
                                let _ = self.serial_tx.send(crate::SerialCommand::Identity(self.serial_port.clone(),name,label.unwrap_or_default()));
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
        dialog::section(ui, icons::LOCK, "Device access", |ui| {
            if let Some(info) = &self.access_info {
                if info.get("ready").and_then(|v| v.as_bool()) != Some(true) {
                    ui.colored_label(
                        ui.visuals().error_fg_color,
                        "Device access storage is unavailable. Physical USB recovery is required.",
                    );
                    return;
                }
                if info.get("default_credentials").and_then(|v| v.as_bool()) == Some(true) {
                    ui.colored_label(
                        ui.visuals().warn_fg_color,
                        "Default login: admin / pakarcontroller. Change it here.",
                    );
                }
                field(
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
                );
                ui.label(egui::RichText::new(crate::ui::i18n::text("Uses your current connection password. Saved on the ESP across OTA; signs out every client. Wi-Fi credentials are unchanged. HTTP is not encrypted.")).small().weak());
                if dialog::primary_action_button(ui, icons::CHECK, "Save device credentials")
                    .clicked()
                {
                    match crate::access::validate(
                        &self.access_user,
                        &self.access_password,
                        &self.access_confirm,
                    ) {
                        Ok(()) => {
                            let next = crate::Endpoint {
                                base: self.base.clone(),
                                user: self.access_user.clone(),
                                password: self.access_password.clone(),
                            };
                            let _ = self
                                .http_tx
                                .send(crate::HttpCommand::AccessChange(self.endpoint(), next));
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
        for (kind, message) in self.event_log.iter().rev() {
            ui.horizontal_wrapped(|ui| {
                ui.label(egui::RichText::new(kind).color(if kind == "Error" {
                    ui.visuals().error_fg_color
                } else {
                    ui.visuals().weak_text_color()
                }));
                ui.label(message);
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
            let response = egui::Modal::new(egui::Id::new("connection-dialog")).show(ctx, |ui| {
                ui.set_width(480.0);
                ui.heading(format!("{}  {}", icons::PLUG, crate::ui::i18n::text("Connect to controller")));
                ui.add_space(12.0);
                dialog::section(ui, icons::GLOBE, "Wi-Fi", |ui| {
                    field(ui, icons::GLOBE, "Endpoint", &mut self.base, false);
                    field(ui, icons::LOCK, "Username", &mut self.user, false);
                    field(ui, icons::LOCK, "Password", &mut self.password, true);
                });
                ui.add_space(10.0);
                dialog::section(ui, icons::PLUG, "USB serial", |ui| {
                    // Pealayer's custom hardware-path fallback: discovery is not
                    // the only way to choose a stable OS serial endpoint.
                    field(ui, icons::PLUG, "Port or hardware path", &mut self.serial_port, false);
                    egui::ComboBox::from_label("Port")
                        .selected_text(if self.serial_port.is_empty() {
                            "Disconnected"
                        } else {
                            &self.serial_port
                        })
                        .show_ui(ui, |ui| {
                            ui.selectable_value(
                                &mut self.serial_port,
                                String::new(),
                                "Disconnected",
                            );
                            for port in serialport::available_ports().unwrap_or_default() {
                                ui.selectable_value(
                                    &mut self.serial_port,
                                    port.port_name.clone(),
                                    &port.port_name,
                                );
                            }
                        });
                    egui::ComboBox::from_label("Commands")
                        .selected_text(match self.transport {
                            Transport::WiFi => "Wi-Fi",
                            Transport::Serial => "Serial",
                            Transport::Both => "Wi-Fi + serial",
                        })
                        .show_ui(ui, |ui| {
                            ui.selectable_value(&mut self.transport, Transport::WiFi, "Wi-Fi");
                            ui.selectable_value(&mut self.transport, Transport::Serial, "Serial");
                            ui.selectable_value(
                                &mut self.transport,
                                Transport::Both,
                                "Wi-Fi + serial",
                            );
                        });
                });
                ui.add_space(12.0);
                dialog::action_row(ui, false, |ui| {
                    if dialog::primary_action_button(ui, icons::PLUG, "Connect").clicked() {
                        self.config.endpoint = self.base.clone();
                        self.config.user = self.user.clone();
                        self.config.serial_port = self.serial_port.clone();
                        self.last_ws_endpoint = None;
                        self.refresh();
                        self.show_connection = false;
                        self.save_workspace();
                    }
                    if dialog::action_button(ui, icons::X, "Close").clicked() {
                        self.show_connection = false;
                    }
                });
            });
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
                                !self.mappings.is_empty(),
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
