//! Pealayer application shell adapted for a controller rather than media playback.
//! Menu -> typed action -> app dispatcher is shared with native launch/IPC/tray.
use crate::{
    ui::{
        dialog, icons,
        layout::{self, Panel},
    },
    *,
};
use eframe::egui;
use platform::instance::Command;

pub fn launch_command(args: &[String]) -> Command {
    if args.iter().any(|v| v == "--connect-dialog") {
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
impl PakarControllerApp {
    pub fn receive_configuration(&mut self, path: &str, value: serde_json::Value) {
        let flag = |key: &str| value.get(key).and_then(|v| v.as_bool());
        let text = |key: &str| value.get(key).and_then(|v| v.as_str()).map(str::to_owned);
        let port = |key: &str| {
            value
                .get(key)
                .and_then(|v| v.as_u64())
                .and_then(|v| u16::try_from(v).ok())
        };
        if path == "/api/v1/device/name" {
            if let Some(name) = text("hostname") {
                if crate::identity::normalize_name(&name).is_ok() {
                    self.device_name = Some(name);
                }
            }
        } else if path == "/api/v1/network" {
            if let (Some(station), Some(ap), Some(ssid)) = (
                flag("station_enabled"),
                flag("ap_enabled"),
                text("station_ssid"),
            ) {
                self.station_enabled = station;
                self.ap_enabled = ap;
                self.wifi_ssid = ssid;
            }
        } else if path == "/api/v1/cloud" {
            if let (Some(enabled), Some(tls), Some(host), Some(p), Some(user), Some(topic)) = (
                flag("mqtt_enabled"),
                flag("mqtt_tls"),
                text("mqtt_host"),
                port("mqtt_port"),
                text("mqtt_user"),
                text("mqtt_base_topic"),
            ) {
                self.mqtt_enabled = enabled;
                self.mqtt_tls = tls;
                self.mqtt_host = host;
                self.mqtt_port = p;
                self.mqtt_user = user;
                self.mqtt_topic = topic;
            }
            if let (Some(enabled), Some(tls), Some(host), Some(p), Some(path)) = (
                flag("websocket_enabled"),
                flag("websocket_tls"),
                text("websocket_host"),
                port("websocket_port"),
                text("websocket_path"),
            ) {
                self.cloud_ws_enabled = enabled;
                self.cloud_ws_tls = tls;
                self.cloud_ws_host = host;
                self.cloud_ws_port = p;
                self.cloud_ws_path = path;
            }
            if let Some(seconds) = port("telemetry_seconds") {
                self.telemetry_seconds = seconds;
            }
        }
    }
    pub fn open_preferences(&mut self) {
        if self.preferences.is_none() {
            self.preferences = Some(ui::preferences::PreferencesDraft::new(self.config.clone()));
        }
    }
    pub fn dispatch(&mut self, command: Command, ctx: &egui::Context) {
        match command {
            Command::Quit => {
                self.exiting = true;
                self.save_workspace();
                ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                return;
            }
            Command::Preferences => self.open_preferences(),
            Command::Connect => self.show_connection = true,
            Command::About => self.show_about = true,
            Command::Workspaces => self.show_workspaces = true,
            Command::FindCell { query } => {
                if let Ok(matches) = ui::cell_search::parse(&query) {
                    self.cell_search = query;
                    if let Some(id) = matches.first() {
                        self.selected_cell = *id;
                    }
                    self.reveal_panels.push(Panel::Table);
                    self.focus_cell_search = true;
                }
            }
            Command::Panel { name } => {
                if let Some(panel) = Panel::from_name(&name) {
                    self.reveal_panels.push(panel);
                }
            }
            Command::Show | Command::Status => {}
        }
        ctx.send_viewport_cmd(egui::ViewportCommand::Visible(true));
        ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(false));
        ctx.send_viewport_cmd(egui::ViewportCommand::Focus);
        ctx.request_repaint();
    }
    pub fn notify(&self, title: &str, message: &str, severity: messaging::Severity) {
        let _ = messaging::publish(
            messaging::ToastRequest {
                id: Some("pakar.command".into()),
                title: title.into(),
                message: message.chars().take(2048).collect(),
                severity,
                timeout_ms: 5000,
            },
            "desktop",
        );
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
    fn live_status(&self) -> Option<&DeviceStatus> {
        match self.command_route() {
            Some(Transport::WiFi) => self.status.as_ref(),
            Some(Transport::Serial) => self.serial_status.as_ref(),
            _ => None,
        }
    }
    fn refresh(&mut self) {
        self.configuration_loaded = false;
        self.last_poll = Instant::now() - Duration::from_secs(2);
        self.last_mapping_poll = Instant::now() - Duration::from_secs(30);
    }
    pub fn draw_shell(&mut self, ui: &mut egui::Ui) {
        let ctx = ui.ctx().clone();
        while let Ok(command) = self.control.rx.try_recv() {
            self.dispatch(command, &ctx);
        }
        if let Some(action) = self.tray.as_ref().and_then(|tray| tray.poll()) {
            match action {
                platform::windows_tray::TrayAction::Show => self.dispatch(Command::Show, &ctx),
                platform::windows_tray::TrayAction::Preferences => {
                    self.dispatch(Command::Preferences, &ctx)
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
            && ctx.input(|i| i.viewport().close_requested())
        {
            ctx.send_viewport_cmd(egui::ViewportCommand::CancelClose);
            ctx.send_viewport_cmd(egui::ViewportCommand::Visible(false));
            self.save_workspace();
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::Comma)) {
            self.open_preferences();
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::K)) {
            self.show_connection = true;
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::M)) {
            self.show_mapping = true;
        }
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::F5)) {
            self.refresh();
        }
        if self.preferences.is_none() && !self.show_connection && !self.confirm_reset {
            if ctx.input_mut(|i| i.consume_key(egui::Modifiers::CTRL, egui::Key::F)) {
                self.reveal_panels.push(Panel::Table);
                self.focus_cell_search = true;
            }
            let previous = ctx.input_mut(|i| i.consume_key(egui::Modifiers::SHIFT, egui::Key::F3));
            let next = ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::F3));
            if previous || next {
                if let Ok(matches) = ui::cell_search::parse(&self.cell_search) {
                    if let Some(id) =
                        ui::cell_search::navigate(&matches, self.selected_cell, previous)
                    {
                        self.selected_cell = id;
                        self.reveal_panels.push(Panel::Table);
                    }
                }
            }
        }
        self.poll();
        ctx.request_repaint_after(Duration::from_millis(250));
        let snapshot = self.live_status().cloned();
        if let Ok(mut value) = self.control.snapshot.lock() {
            *value = serde_json::json!({"application":"PakarController","version":env!("CARGO_PKG_VERSION"),"configured_endpoint":self.base,"process_id":std::process::id(),"ready":true,"single_instance":self.config.single_instance,"panels":self.dock.iter_all_tabs().map(|(_,p)|p.name()).collect::<Vec<_>>(),"preferences_open":self.preferences.is_some(),"mapping_open":self.show_mapping,"workspaces_open":self.show_workspaces,"about_open":self.show_about,"device_online":snapshot.is_some(),"websocket_live":self.websocket_live,"serial_live":self.serial_live,"cell_search":self.cell_search,"search_matches":ui::cell_search::parse(&self.cell_search).unwrap_or_default(),"selected_cell":self.selected_cell});
        }
        egui::Panel::top("pakar-menu").show(ui, |ui| {
            egui::MenuBar::new().ui(ui, |ui| {
                ui.menu_button("File", |ui| {
                    if ui
                        .button(format!("{}  Connect…       Ctrl+K", icons::PLUG))
                        .clicked()
                    {
                        self.show_connection = true;
                        ui.close();
                    }
                    if ui
                        .button(format!("{}  Cell mapping…  Ctrl+M", icons::GRID_FOUR))
                        .clicked()
                    {
                        self.show_mapping = true;
                        ui.close();
                    }
                    ui.separator();
                    if ui
                        .button(format!("{}  Preferences…   Ctrl+,", icons::GEAR))
                        .clicked()
                    {
                        self.open_preferences();
                        ui.close();
                    }
                    ui.separator();
                    if ui.button(format!("{}  Quit", icons::POWER)).clicked() {
                        self.dispatch(Command::Quit, &ctx);
                    }
                });
                ui.menu_button("Workspace", |ui| {
                    ui.strong("Panels");
                    layout::draw_workspace_tab_menu(self, ui);
                    ui.separator();
                    if ui
                        .button(format!("{}  Manage workspaces…", icons::TABS))
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
                ui.menu_button("Device", |ui| {
                    if ui
                        .button(format!("{}  Refresh         F5", icons::ARROW_CLOCKWISE))
                        .clicked()
                    {
                        self.refresh();
                        ui.close();
                    }
                    if ui
                        .button(format!("{}  Network & cloud", icons::GLOBE))
                        .clicked()
                    {
                        self.reveal_panels.push(Panel::Network);
                        ui.close();
                    }
                    if ui
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
                ui.menu_button("Help", |ui| {
                    if ui
                        .button(format!("{}  About PakarController…", icons::INFO))
                        .clicked()
                    {
                        self.show_about = true;
                        ui.close();
                    }
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui
                        .button(icons::GEAR)
                        .on_hover_text("Preferences (Ctrl+,)")
                        .clicked()
                    {
                        self.open_preferences();
                    }
                    if ui
                        .button(icons::PLUG)
                        .on_hover_text("Connect (Ctrl+K)")
                        .clicked()
                    {
                        self.show_connection = true;
                    }
                    ui.label(egui::RichText::new("PakarController").strong());
                    ui.label(
                        egui::RichText::new(icons::CIRCUITRY).color(ui.visuals().selection.bg_fill),
                    );
                });
            });
        });
        let s = snapshot.clone().unwrap_or_default();
        ui::status_bar::draw(
            ui,
            ui::status_bar::StatusBar {
                peer_known: snapshot.is_some(),
                message: &self.message,
                hostname: &s.hostname,
                station: &s.station_ssid,
                avr_online: s.avr_online,
                tm1637_ready: s.tm1637_ack,
                websocket_live: self.websocket_live,
                serial_live: self.serial_live,
            },
        );
        egui::Panel::top("pakar-workspace-toolbar")
            .frame(egui::Frame::new().inner_margin(egui::Margin::symmetric(12, 8)))
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.label(
                        egui::RichText::new(icons::GRID_FOUR)
                            .size(19.0)
                            .color(ui.visuals().selection.bg_fill),
                    );
                    ui.strong("Table workspace");
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
                    badge(ui, "WebSocket", self.websocket_live);
                    badge(ui, "USB", self.serial_live);
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if ui.button(format!("{}  Workspaces", icons::TABS)).clicked() {
                            self.show_workspaces = true;
                        }
                        if ui
                            .button(format!("{}  Mapping", icons::GRID_FOUR))
                            .clicked()
                        {
                            self.show_mapping = true;
                        }
                        if ui
                            .button(icons::ARROW_CLOCKWISE)
                            .on_hover_text("Refresh")
                            .clicked()
                        {
                            self.refresh();
                        }
                    });
                });
            });
        for panel in std::mem::take(&mut self.reveal_panels) {
            layout::reveal(&mut self.dock, panel);
        }
        egui::CentralPanel::default()
            .frame(egui::Frame::new().inner_margin(0))
            .show(ui, |ui| {
                if self.dock.iter_all_tabs().count() == 0 {
                    ui.centered_and_justified(|ui| {
                        if ui
                            .button(format!("{}  Restore workspace", icons::TABS))
                            .clicked()
                        {
                            self.dock = layout::create_initial_layout();
                        }
                    });
                } else {
                    let mut dock =
                        std::mem::replace(&mut self.dock, egui_dock::DockState::new(vec![]));
                    let style = egui_dock::Style::from_egui(ui.style().as_ref());
                    egui_dock::DockArea::new(&mut dock)
                        .style(style)
                        .show_leaf_collapse_buttons(true)
                        .show_inside(ui, &mut layout::PakarTabViewer { app: self });
                    self.dock = dock;
                }
            });
        ui::preferences::draw(self, &ctx);
        self.draw_dialogs(&ctx);
        ui::toasts::draw(&ctx);
    }
    pub fn draw_panel(&mut self, ui: &mut egui::Ui, panel: Panel) {
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
        let snapshot = self.live_status().cloned();
        ui.horizontal(|ui| {
            ui.heading("Pakar table");
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
                    .hint_text("Find cell: 12, 3–8  ·  Ctrl+F")
                    .char_limit(256),
            ).on_hover_text("Exact logical cell IDs, lists and ranges. Search does not change LEDs. Enter/F3: next; Shift+F3: previous; Escape: clear.");
            if self.focus_cell_search {
                search.request_focus();
                self.focus_cell_search = false;
            }
            if search.has_focus() && ui.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::Escape)) {
                self.cell_search.clear();
            }
            let result = ui::cell_search::parse(&self.cell_search);
            let matches = result.as_ref().map(Vec::as_slice).unwrap_or_default();
            if search.changed() && let Some(id) = matches.first() {
                self.selected_cell = *id;
            }
            let previous = ui.add_enabled(!matches.is_empty(), egui::Button::new(icons::CARET_LEFT))
                .on_hover_text("Previous match (Shift+F3)").clicked();
            let next = ui.add_enabled(!matches.is_empty(), egui::Button::new(icons::CARET_RIGHT))
                .on_hover_text("Next match (Enter / F3)").clicked()
                || (search.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter)));
            if (previous || next) && let Some(id) = ui::cell_search::navigate(matches, self.selected_cell, previous) {
                self.selected_cell = id;
                if search.lost_focus() { search.request_focus(); }
            }
            if ui.add_enabled(!self.cell_search.is_empty(), egui::Button::new(icons::X))
                .on_hover_text("Clear cell search").clicked() {
                self.cell_search.clear();
            }
        });
        let search_result = ui::cell_search::parse(&self.cell_search);
        if let Err(error) = &search_result {
            ui.colored_label(ui.visuals().error_fg_color, error);
        } else if !self.cell_search.trim().is_empty() {
            let matches = search_result.as_ref().unwrap();
            let current = matches.iter().position(|id| *id == self.selected_cell);
            ui.label(
                egui::RichText::new(match current {
                    Some(index) => format!(
                        "Match {} of {} · logical cell {}",
                        index + 1,
                        matches.len(),
                        self.selected_cell
                    ),
                    None => format!("{} matching cells", matches.len()),
                })
                .small()
                .color(ui.visuals().warn_fg_color),
            );
        }
        let matches = search_result.unwrap_or_default();
        ui.add_space(8.0);
        let width = ui.available_width().max(200.0);
        let visible_height = (ui.clip_rect().bottom() - ui.cursor().min.y - 38.0).max(180.0);
        let pitch = (width / 10.0).min(visible_height / (8.5 * 0.72)).min(94.0);
        let height = (pitch * 0.72).max(20.0);
        let (area, _) =
            ui.allocate_exact_size(egui::vec2(width, height * 8.5), egui::Sense::hover());
        let grid_origin = area.min + egui::vec2((width - pitch * 10.0) * 0.5, 0.0);
        let palette = config::live().color_palette;
        for column in 0..10 {
            let band = egui::Rect::from_min_size(
                grid_origin + egui::vec2(column as f32 * pitch - 2.0, -3.0),
                egui::vec2(pitch - 3.0, height * 8.5 + 1.0),
            );
            ui.painter().rect_filled(
                band,
                7.0,
                ui::palette::color(
                    palette,
                    ui.visuals().dark_mode,
                    if column % 2 == 0 {
                        "surface-0"
                    } else {
                        "surface-2"
                    },
                ),
            );
        }
        for row in 0..8u8 {
            for column in 0..10u8 {
                let id = row * 10 + column + 1;
                let state = snapshot.as_ref().and_then(|s| s.cells.get(id as usize - 1));
                let mapping = self.mappings.iter().find(|entry| entry.logical_id == id);
                let label = mapping
                    .map(|m| m.name.as_str())
                    .filter(|v| !v.is_empty())
                    .map(str::to_owned)
                    .unwrap_or_else(|| id.to_string());
                let pos = grid_origin
                    + egui::vec2(
                        column as f32 * pitch,
                        row as f32 * height + if column % 2 == 0 { height * 0.5 } else { 0.0 },
                    );
                let rect = egui::Rect::from_min_size(pos, egui::vec2(pitch - 7.0, height - 7.0));
                let fill = state
                    .map(|s| Self::cell_color(s))
                    .unwrap_or(ui.visuals().faint_bg_color);
                let ink = if state.is_some_and(|s| s == "white") {
                    Color32::from_rgb(28, 34, 42)
                } else {
                    ui.visuals().text_color()
                };
                let matched = matches.contains(&id);
                let stroke = if matched {
                    egui::Stroke::new(3.0, ui.visuals().warn_fg_color)
                } else if self.selected_cell == id {
                    egui::Stroke::new(2.5, ui.visuals().selection.bg_fill)
                } else {
                    egui::Stroke::new(1.0, ui.visuals().widgets.noninteractive.bg_stroke.color)
                };
                let response = ui.put(
                    rect,
                    egui::Button::new(RichText::new(label).size(14.0).strong().color(ink))
                        .fill(fill)
                        .stroke(stroke)
                        .wrap_mode(egui::TextWrapMode::Truncate)
                        .corner_radius(9.0),
                );
                if matched && self.selected_cell == id {
                    ui.painter().rect_stroke(
                        rect.shrink(3.5),
                        6.0,
                        egui::Stroke::new(1.5, ui.visuals().selection.bg_fill),
                        egui::StrokeKind::Inside,
                    );
                }
                // A renamed cell still exposes its searchable logical ID.
                if mapping.is_some_and(|m| !m.name.is_empty() && m.name != id.to_string()) {
                    ui.painter().text(
                        rect.right_bottom() - egui::vec2(5.0, 3.0),
                        egui::Align2::RIGHT_BOTTOM,
                        format!("#{id}"),
                        egui::FontId::proportional(10.0),
                        ink,
                    );
                }
                let response = response.on_hover_text(format!(
                    "Cell {id} · {}",
                    mapping
                        .map(|m| format!("physical {}", m.physical_cell))
                        .unwrap_or_else(|| "mapping unavailable".into())
                ));
                if response.clicked() {
                    self.selected_cell = id;
                }
                if response.double_clicked() {
                    if let Some(state) = state {
                        let (next, value) = Self::next_cell_state(state);
                        self.set_cell(id, next, value);
                    }
                }
                response.context_menu(|ui| {
                    ui.strong(format!("Cell {id}"));
                    ui.separator();
                    for (name, value) in [("White", 0), ("Red", 1), ("Off", 2)] {
                        if ui
                            .add_enabled(snapshot.is_some(), egui::Button::new(name))
                            .clicked()
                        {
                            self.selected_cell = id;
                            self.set_cell(id, &name.to_lowercase(), value);
                            ui.close();
                        }
                    }
                    ui.separator();
                    if ui
                        .button(format!("{}  Edit mapping…", icons::PENCIL_SIMPLE))
                        .clicked()
                    {
                        self.select_mapping(id);
                        self.show_mapping = true;
                        ui.close();
                    }
                });
            }
        }
        ui.add_space(8.0);
        ui.horizontal(|ui| {
            ui.label(egui::RichText::new(format!("Cell {} selected", self.selected_cell)).weak());
            if snapshot.is_none() {
                if ui.button(format!("{}  Connect…", icons::PLUG)).clicked() {
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
            "/api/v1/cell",
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
        dialog::section(ui, icons::PLAY, "Game mode", |ui| {
            ui.add_enabled_ui(snapshot.is_some(), |ui| {
                for (label, name, value) in [
                    ("80 cells", "cell", 1),
                    ("First wins", "first-wins", 2),
                    ("Snake", "snake", 3),
                    ("Idle", "idle", 0),
                ] {
                    let selected = snapshot.as_ref().is_some_and(|s| s.mode == name);
                    if ui
                        .add_sized(
                            [ui.available_width(), 30.0],
                            egui::Button::new(label).selected(selected),
                        )
                        .clicked()
                    {
                        self.action(
                            "/api/v1/mode",
                            vec![("value".into(), name.into())],
                            OP_SET_MODE,
                            vec![value],
                        );
                    }
                }
                if ui
                    .button(format!("{}  Reset game…", icons::ARROW_COUNTER_CLOCKWISE))
                    .clicked()
                {
                    self.confirm_reset = true;
                }
            });
        });
        ui.add_space(10.0);
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
                    .button(format!("{}  Edit identity…", icons::PENCIL_SIMPLE))
                    .clicked()
                {
                    self.select_mapping(self.selected_cell);
                    self.show_mapping = true;
                }
            },
        );
        ui.add_space(10.0);
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
                        "/api/v1/effect",
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
                        "/api/v1/brightness",
                        vec![("value".into(), self.brightness.to_string())],
                        OP_BRIGHTNESS,
                        vec![self.brightness],
                    );
                }
            });
        });
        if snapshot.as_ref().is_some_and(|s| s.mode == "snake") {
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
                                "/api/v1/snake/direction",
                                vec![("value".into(), direction.into())],
                            );
                        }
                    }
                });
            });
        }
    }
    fn draw_front_panel(&mut self, ui: &mut egui::Ui) {
        let snapshot = self.live_status().cloned();
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
                    ui.label("Segment readback unavailable");
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
                ui.label(egui::RichText::new("Display state unavailable").weak());
            }
        });
        ui.add_space(10.0);
        dialog::section(ui, icons::TEXT_ALIGN_LEFT, "Text", |ui| {
            ui.add_sized(
                [ui.available_width(), 28.0],
                dialog::singleline_text_edit(&mut self.display_text)
                    .char_limit(40)
                    .hint_text("Front-panel text"),
            );
            ui.horizontal_wrapped(|ui| {
                ui.label("Step");
                ui.add(
                    egui::DragValue::new(&mut self.config.display.speed_ms)
                        .range(80..=5000)
                        .suffix(" ms"),
                );
                ui.checkbox(&mut self.config.display.force_scroll, "Scroll short text");
            });
            ui.horizontal_wrapped(|ui| {
                ui.label("Repeat");
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
                    ui.label("Pause");
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
                    ui.label("Hold");
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
                        egui::Button::new(format!("{}  Present", icons::PLAY)),
                    )
                    .clicked()
                {
                    self.action(
                        "/api/v1/display",
                        self.config.display.form(&self.display_text),
                        OP_DISPLAY,
                        payload.clone().unwrap(),
                    );
                    self.save_workspace();
                }
                if snapshot.as_ref().is_some_and(|s| s.tm1637_host_active)
                    && ui.button("Stop text").clicked()
                {
                    self.action(
                        "/api/v1/display",
                        vec![("release".into(), "true".into())],
                        OP_DISPLAY,
                        crate::display::RELEASE.to_vec(),
                    );
                }
            });
            ui.separator();
            ui.horizontal(|ui| {
                ui.add(egui::DragValue::new(&mut self.display_value).range(-999..=9999));
                if ui
                    .add_enabled(snapshot.is_some(), egui::Button::new("Show number"))
                    .clicked()
                {
                    self.action(
                        "/api/v1/display",
                        vec![("value".into(), self.display_value.to_string())],
                        OP_DISPLAY,
                        self.display_value.to_le_bytes().to_vec(),
                    );
                }
            });
        });
        ui.add_space(10.0);
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
                                    "/api/v1/audio",
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
                        egui::Button::new(format!("{}  Play tone", icons::PLAY)),
                    )
                    .clicked()
                {
                    let mut p = self.buzzer_frequency.to_le_bytes().to_vec();
                    p.extend(self.buzzer_duration.to_le_bytes());
                    self.action(
                        "/api/v1/buzzer",
                        vec![
                            ("frequency".into(), self.buzzer_frequency.to_string()),
                            ("duration_ms".into(), self.buzzer_duration.to_string()),
                        ],
                        OP_BUZZER,
                        p,
                    );
                }
            }
        });
    }
    fn draw_hardware(&mut self, ui: &mut egui::Ui) {
        let Some(s) = self.live_status().cloned() else {
            ui.label("Hardware state unavailable");
            return;
        };
        dialog::section(ui, icons::CPU, "Controllers", |ui| {
            badge(ui, "ATmega328P", s.avr_online);
            badge(ui, "TM1637", s.tm1637_ack);
            egui::Grid::new("controller-health")
                .num_columns(2)
                .spacing([16.0, 8.0])
                .show(ui, |ui| {
                    for (name, value) in [
                        ("Firmware", s.version.clone()),
                        ("Host", s.hostname.clone()),
                        ("Link", s.link_mode.clone()),
                        (
                            "Program latch",
                            if s.programming_latch {
                                "Active"
                            } else {
                                "Released"
                            }
                            .into(),
                        ),
                        (
                            "UART errors",
                            format!("ESP {} / AVR {}", s.esp_uart_errors, s.avr_uart_errors),
                        ),
                        ("USB errors", s.usb_serial_errors.to_string()),
                    ] {
                        ui.label(egui::RichText::new(name).weak());
                        ui.label(value);
                        ui.end_row();
                    }
                });
        });
        ui.add_space(10.0);
        dialog::section(ui, icons::HAND_TAP, "Inputs", |ui| {
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
        });
        ui.add_space(10.0);
        dialog::section(ui, icons::LIGHTBULB, "Shift-register outputs", |ui| {
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
                                "/api/v1/outputs",
                                vec![("mask".into(), mask.to_string())],
                                OP_OUTPUT_MASK,
                                vec![mask],
                            );
                        }
                    }
                });
            });
        });
    }
    fn draw_network(&mut self, ui: &mut egui::Ui) {
        dialog::section(ui, icons::GLOBE, "Device name", |ui| {
            if let Some(name) = &mut self.device_name {
                field(ui, icons::GLOBE, "Hostname", name, false);
                ui.label(egui::RichText::new("Use a label or .local name. Saved across OTA; AP name and cloud identity follow it. Wi-Fi/login passwords stay unchanged. The connection briefly drops.").small().weak());
                if dialog::primary_action_button(ui, icons::CHECK, "Save device name").clicked() {
                    match crate::identity::normalize_name(name) {
                        Ok(name) => {
                            let _ = self
                                .http_tx
                                .send(crate::HttpCommand::NameChange(self.endpoint(), name));
                        }
                        Err(error) => self.notify(
                            "Invalid device name",
                            &error,
                            crate::messaging::Severity::Warning,
                        ),
                    }
                }
            } else {
                ui.label("Device naming has not been advertised by the connected firmware.");
            }
        });
        ui.add_space(10.0);
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
                ui.label(egui::RichText::new("Uses your current connection password. Saved on the ESP across OTA; signs out every client. Wi-Fi credentials are unchanged. HTTP is not encrypted.").small().weak());
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
                ui.label("Credential settings have not been advertised by the device. Connect to the updated firmware to configure them.");
            }
        });
        ui.add_space(10.0);
        if let Some(s) = self.live_status().cloned() {
            dialog::section(ui, icons::BROADCAST, "Live network", |ui| {
                ui.horizontal_wrapped(|ui| {
                    badge(ui, "Station", s.station_connected);
                    badge(ui, "AP", s.ap_started);
                    badge(ui, "NTP", s.ntp_synced);
                    badge(ui, "MQTT", s.mqtt_connected);
                    badge(ui, "Cloud WS", s.cloud_websocket_connected);
                });
                ui.label(format!(
                    "{} · {} dBm · {} clients",
                    s.station_ssid, s.station_rssi, s.websocket_clients
                ));
                ui.hyperlink_to(&s.canonical_url, &s.canonical_url);
                ui.label(
                    egui::RichText::new(format!(
                        "mDNS {} · SSDP {} · NetBIOS {}",
                        s.mdns_started, s.ssdp_started, s.netbios_started
                    ))
                    .small()
                    .weak(),
                );
                if !s.cloud_error.is_empty() {
                    ui.colored_label(ui.visuals().error_fg_color, s.cloud_error);
                }
            });
            ui.add_space(10.0);
        }
        dialog::section(ui, icons::WIFI_HIGH, "Wi-Fi policy", |ui| {
            ui.horizontal(|ui| {
                ui.checkbox(&mut self.station_enabled, "Station");
                ui.checkbox(&mut self.ap_enabled, "Recovery AP");
            });
            field(ui, icons::WIFI_HIGH, "SSID", &mut self.wifi_ssid, false);
            field(ui, icons::LOCK, "Password", &mut self.wifi_password, true);
            if dialog::primary_action_button(ui, icons::CHECK, "Apply network").clicked() {
                self.apply_network();
            }
        });
        ui.add_space(10.0);
        dialog::section(ui, icons::GLOBE, "MQTT", |ui| {
            ui.horizontal(|ui| {
                ui.checkbox(&mut self.mqtt_enabled, "Enabled");
                ui.checkbox(&mut self.mqtt_tls, "TLS");
                ui.label("Port");
                ui.add(egui::DragValue::new(&mut self.mqtt_port).range(1..=65535));
            });
            field(ui, icons::GLOBE, "Broker", &mut self.mqtt_host, false);
            field(ui, icons::LIST, "Topic", &mut self.mqtt_topic, false);
            field(ui, icons::LOCK, "Username", &mut self.mqtt_user, false);
            field(ui, icons::LOCK, "Password", &mut self.mqtt_password, true);
        });
        ui.add_space(10.0);
        dialog::section(ui, icons::BROADCAST, "Outbound WebSocket", |ui| {
            ui.horizontal(|ui| {
                ui.checkbox(&mut self.cloud_ws_enabled, "Enabled");
                ui.checkbox(&mut self.cloud_ws_tls, "TLS");
                ui.label("Port");
                ui.add(egui::DragValue::new(&mut self.cloud_ws_port).range(1..=65535));
            });
            field(ui, icons::GLOBE, "Host", &mut self.cloud_ws_host, false);
            field(ui, icons::LINK, "Path", &mut self.cloud_ws_path, false);
            field(
                ui,
                icons::LOCK,
                "Bearer token",
                &mut self.cloud_ws_token,
                true,
            );
            ui.horizontal(|ui| {
                ui.label("Telemetry");
                ui.add(
                    egui::DragValue::new(&mut self.telemetry_seconds)
                        .range(1..=3600)
                        .suffix(" s"),
                );
            });
        });
        ui.add_space(8.0);
        if ui
            .add_enabled(
                self.transport != Transport::Serial,
                egui::Button::new(format!("{}  Apply cloud settings", icons::CHECK)),
            )
            .clicked()
        {
            self.apply_cloud();
        }
    }
    fn draw_streaming(&mut self, ui: &mut egui::Ui) {
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
                    self.websocket_live,
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
                .on_hover_text("Clear events")
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
    fn select_mapping(&mut self, id: u8) {
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
                ui.heading("Reset the current game?");
                ui.label("The winner and current game state will be cleared.");
                ui.add_space(12.0);
                dialog::action_row(ui, false, |ui| {
                    if dialog::primary_action_button(ui, icons::ARROW_COUNTER_CLOCKWISE, "Reset")
                        .clicked()
                    {
                        self.action("/api/v1/game/reset", vec![], OP_RESET, vec![]);
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
                ui.heading(format!("{}  Connect to controller", icons::PLUG));
                ui.add_space(12.0);
                dialog::section(ui, icons::GLOBE, "Wi-Fi", |ui| {
                    field(ui, icons::GLOBE, "Endpoint", &mut self.base, false);
                    field(ui, icons::LOCK, "Username", &mut self.user, false);
                    field(ui, icons::LOCK, "Password", &mut self.password, true);
                });
                ui.add_space(10.0);
                dialog::section(ui, icons::PLUG, "USB serial", |ui| {
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
            egui::Window::new(format!("{}  Cell mapping", icons::GRID_FOUR))
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
                        ui.label("Logical");
                        let changed = ui
                            .add(egui::DragValue::new(&mut self.mapping_logical).range(1..=80))
                            .changed();
                        if changed {
                            self.select_mapping(self.mapping_logical);
                        }
                        ui.label("Physical");
                        ui.add(egui::DragValue::new(&mut self.mapping_physical).range(1..=80));
                        ui.label("Name");
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
                                    ui.strong("Logical cell");
                                    ui.strong("Physical cell");
                                    ui.strong("Name");
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
                                egui::Button::new(format!("{}  Save mapping", icons::CHECK))
                                    .min_size(egui::vec2(104.0, 28.0)),
                            )
                            .clicked()
                        {
                            self.post(
                                "/api/v1/cells/mapping",
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
            egui::Window::new(format!("{}  About PakarController",icons::INFO)).id(egui::Id::new("about-pakar")).open(&mut open).default_size([520.0,300.0]).collapsible(false).frame(dialog::opaque_window_frame_from_context(ctx)).show(ctx,|ui|{
                ui.horizontal(|ui|{ui.label(egui::RichText::new(icons::CIRCUITRY).size(48.0).color(ui.visuals().selection.bg_fill));ui.vertical(|ui|{ui.heading("PakarController");ui.label(format!("Version {}",env!("CARGO_PKG_VERSION")));ui.label("ESP + AVR control workstation");});});ui.add_space(16.0);
                dialog::section(ui,icons::APP_WINDOW,"Application foundation",|ui|{ui.label("Built from Pealayer’s desktop workspace, dialog controls, icon and color pickers, themes, fonts, notifications, and native instance ownership.");ui.label("MIT License · Pealayer © 2026 ToghrolTP");ui.hyperlink_to("Pealayer source","https://github.com/ToghrolTP/pealayer");});ui.add_space(12.0);
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
        egui::Window::new(format!("{}  Workspaces", icons::TABS))
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
                            .hint_text("Workspace name"),
                    );
                    icons::searchable_workspace_icon_picker(
                        ui,
                        "workspace-icon",
                        &mut self.workspace_icon,
                        90.0,
                        "Search icons",
                        "Workspace icons",
                        "No matching icons",
                        config::AppLanguage::English,
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
                                            .on_hover_text("Replace with current layout")
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
                                            .on_hover_text("Delete workspace")
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
