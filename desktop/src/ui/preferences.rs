//! Adapted from Pealayer's PreferencesDraft and rail/body/footer editor.
use crate::{
    PakarControllerApp,
    config::{AppConfig, AppDirection, AppLanguage, AppTheme, ColorPalette},
    ui::{dialog, icons},
};
use eframe::egui;
use super::dropdown::DropdownUiExt;
pub const PAGES: [&str; 7] = [
    "appearance",
    "application",
    "connection",
    "workspace",
    "notifications",
    "board",
    "updates",
];
pub fn page_index(name: &str) -> Option<usize> {
    PAGES.iter().position(|page| *page == name)
}

#[derive(Clone)]
pub struct PreferencesDraft {
    pub config: AppConfig,
    saved_config: AppConfig,
    previewed_config: AppConfig,
    tab: usize,
    search: String,
    status: String,
    confirm_close: bool,
    enrollment_token: String,
}
impl PreferencesDraft {
    pub fn select_page(&mut self, index: usize) {
        if index < PAGES.len() {
            self.tab = index;
            self.search.clear();
        }
    }
    pub fn new(config: AppConfig) -> Self {
        Self {
            saved_config: config.clone(),
            previewed_config: config.clone(),
            config,
            tab: 0,
            search: String::new(),
            status: String::new(),
            confirm_close: false,
            enrollment_token: String::new(),
        }
    }
    pub(super) fn is_dirty(&self) -> bool {
        self.config != self.saved_config
    }
    fn take_preview_update(&mut self) -> Option<AppConfig> {
        if self.config == self.previewed_config {
            return None;
        }
        self.previewed_config = self.config.clone();
        Some(self.config.clone())
    }
    fn matches(&self, text: &str) -> bool {
        self.search.is_empty() || text.to_lowercase().contains(&self.search.to_lowercase())
    }
    // Adapted from Pealayer PreferencesDraft::sync_external_appearance: live
    // owner fields update all three baselines without erasing unrelated drafts.
    fn sync_connection_owner(&mut self, method: crate::connection::Method, enabled: bool,
        endpoint: &str, user: &str, port: &str) {
        for target in [&mut self.config, &mut self.saved_config, &mut self.previewed_config] {
            target.connection_method = method;
            target.connection_enabled = enabled;
            target.endpoint = endpoint.to_owned();
            target.user = user.to_owned();
            target.serial_port = port.to_owned();
        }
    }
    fn sync_workspace_owner(&mut self, workspaces: &crate::native_workspace::Store) {
        for target in [&mut self.config, &mut self.saved_config, &mut self.previewed_config] {
            target.native_workspaces = workspaces.clone();
        }
    }
}
#[derive(Default)]
struct Outcome {
    save: bool,
    close: bool,
    discard: bool,
    test_notification: bool,
    connection_settings: bool,
    device_settings: bool,
    board_action: Option<Option<bool>>,
    client_action: Option<crate::client_management::Action>,
}
fn app_management(draft:&mut PreferencesDraft, ui:&mut egui::Ui)->Option<crate::client_management::Action>{
    use crate::client_management::{self,Action};
    let mut action=None;
    dialog::section(ui,icons::ARROW_CLOCKWISE,"App updates",|ui|{
        ui.weak(format!("PakarController {} · build {} · Windows x64",env!("CARGO_PKG_VERSION"),client_management::build()));
        ui.weak("Updates this desktop application, not the connected board firmware.");
        dialog::setting_row(ui,icons::GLOBE,"Management server",None,|ui|{ui.text_edit_singleline(&mut draft.config.client_management.base);});
        let settings=&mut draft.config.client_management;
        ui.checkbox(&mut settings.auto_check,"Automatically check for app updates");
        ui.checkbox(&mut settings.auto_download,"Download verified newer app packages");
        ui.checkbox(&mut settings.auto_apply,"Apply automatically when all editors and device operations are closed");
        ui.checkbox(&mut settings.auto_cleanup,"Remove obsolete owned pending packages");
        let status=client_management::snapshot();
        ui.strong(if status.state.is_empty(){"Not checked"}else{&status.state});
        if !status.detail.is_empty(){ui.weak(&status.detail);}
        if let Some(total)=status.expected_bytes.filter(|total|*total>0){
            ui.add(egui::ProgressBar::new((status.received_bytes as f64/total as f64).clamp(0.0,1.0) as f32)
                .text(format!("{} / {} bytes",status.received_bytes,total)));
        }
        if let Some(release)=status.release.as_ref(){ui.label(format!("Available: {} · build {} · {} bytes",release.version,release.build,release.bytes));}
        ui.weak(if status.managed_installation{"Managed installation confirmed; graceful replacement preserves settings and rollback."}
            else{"Automatic replacement is unavailable until the approved installer initializes this installation."});
        let ready=!draft.is_dirty()&&!status.applying;
        ui.add_enabled_ui(ready,|ui|{ui.horizontal_wrapped(|ui|{
            if dialog::action_button(ui,icons::ARROW_CLOCKWISE,"Check now").clicked(){action=Some(Action::Check);}
            if ui.add_enabled(status.release.is_some(),egui::Button::new("Download package")).clicked(){action=Some(Action::Download);}
            if ui.add_enabled(status.managed_installation&&status.package_verified,egui::Button::new("Apply and restart")).clicked(){action=Some(Action::Apply);}
            if dialog::action_button(ui,icons::TRASH,"Clean old pending packages").clicked(){action=Some(Action::Cleanup);}
        });});
        if !ready{ui.weak("Save or discard preference edits before performing app management actions.");}
    });
    ui.add_space(8.0);
    dialog::section(ui,icons::LOCK,"Client enrollment and local policy",|ui|{
        let settings=&mut draft.config.client_management;
        ui.checkbox(&mut settings.telemetry,"Send operational app telemetry");
        ui.checkbox(&mut settings.remote_commands,"Receive management commands");
        ui.checkbox(&mut settings.allow_remote_cleanup,"Allow remote cleanup of verified obsolete pending packages");
        ui.checkbox(&mut settings.allow_remote_uninstall,"Opt in to remote uninstall (requires a separately approved removal inventory)");
        ui.weak("Telemetry excludes passwords, enrollment credentials, board payloads and preference contents. Remote actions are claimed at most once; uncertain actions are never replayed.");
        let status=client_management::snapshot();
        ui.label(if status.enrolled{"Enrolled · credential protected by Windows"}else{"Not enrolled"});
        if let Some(id)=status.client_id{ui.weak(format!("Client: {id}"));}
        ui.add(egui::TextEdit::singleline(&mut draft.enrollment_token).password(true).hint_text("One-time enrollment token"));
        if ui.add_enabled(!draft.is_dirty()&&!draft.enrollment_token.is_empty(),egui::Button::new("Enroll this client")).clicked(){
            action=Some(Action::Enroll(std::mem::take(&mut draft.enrollment_token)));
        }
        ui.weak("Uninstall is not executable until an explicit local removal inventory is approved. Active app, settings, rollback and shared caches are never cleanup targets.");
    });
    action
}
struct BoardView {
    bridge: serde_json::Value,
    available: bool,
    device_available: bool,
    ready: bool,
    value: Option<bool>,
    pending: bool,
    persisted: bool,
    message: String,
}
fn board_controls(ui: &mut egui::Ui, board: &BoardView) -> Option<Option<bool>> {
    let mut action = None;
    dialog::section(ui, icons::SPEAKER_SLASH, "Board sound", |ui| {
        // Donor board_info settings are board-owned, not AppConfig drafts.
        // Unknown is a visible state, never an unchecked/false checkbox.
        dialog::setting_row(ui, icons::SPEAKER_SLASH, "Silent mode", None, |ui| {
            if let Some(mut value) = board.value {
                let label = super::i18n::text(if value { "On" } else { "Off" });
                if ui.add_enabled(board.ready && !board.pending,
                    egui::Checkbox::new(&mut value, label)).changed() {
                    action = Some(Some(value));
                }
            } else { ui.weak(super::i18n::text("Unknown")); }
            if board.pending { ui.spinner(); }
        });
        ui.label(egui::RichText::new(super::i18n::text(
            "Mutes the physical board buzzer, including welcome and menu sounds. Saved directly on the board; Windows notification sound is separate.")).small().weak());
        if !board.ready { ui.weak(super::i18n::text("Programming safety keeps the board muted.")); }
        if ui.add_enabled(board.ready && !board.pending,
            egui::Button::new(format!("{}  {}", icons::ARROW_CLOCKWISE, super::i18n::text("Refresh")))).clicked() {
            action = Some(None);
        }
        if !board.message.is_empty() { ui.weak(&board.message); }
        if board.value.is_some() && !board.persisted { ui.weak(super::i18n::text("Not persisted")); }
    });
    action
}
fn connection_summary(draft: &PreferencesDraft, ui: &mut egui::Ui) -> egui::Response {
    let config = &draft.config;
    ui.strong(format!("{} · {}", super::connection_controls::method_label(config.connection_method),
        if config.connection_enabled { "Enabled" } else { "Disconnected" }));
    ui.weak(match config.connection_method {
        crate::connection::Method::WiFi => config.endpoint.as_str(),
        crate::connection::Method::Serial if config.serial_port.is_empty() => "No port selected",
        crate::connection::Method::Serial => config.serial_port.as_str(),
    });
    ui.add_enabled(!draft.is_dirty(), egui::Button::new(format!("{}  Connection settings…", icons::GEAR)))
        .on_disabled_hover_text("Save or discard preference changes before opening connection settings.")
}
fn device_settings_link(draft: &PreferencesDraft, ui: &mut egui::Ui) -> egui::Response {
    ui.weak("Board identity, network, LEDs, front panel and AVR parameters belong to the connected device, not this PC.");
    ui.add_enabled(!draft.is_dirty(),egui::Button::new(format!("{}  Device settings…",icons::CIRCUITRY)))
        .on_disabled_hover_text("Save or discard application preference changes before opening device settings.")
}
fn editor(draft: &mut PreferencesDraft, ui: &mut egui::Ui, board: &BoardView) -> Outcome {
    crate::config::set_live(&draft.config);
    ui.with_layout(
        super::i18n::vertical_layout(crate::config::resolved_rtl(&draft.config)),
        |ui| editor_contents(draft, ui, board),
    )
    .inner
}
fn editor_contents(draft: &mut PreferencesDraft, ui: &mut egui::Ui, board: &BoardView) -> Outcome {
    let mut outcome = Outcome::default();
    ui.horizontal(|ui| {
        ui.label(icons::MAGNIFYING_GLASS);
        ui.add_sized(
            [ui.available_width(), 28.0],
            dialog::singleline_text_edit(&mut draft.search).hint_text(crate::ui::i18n::text("Search preferences")),
        );
    });
    ui.add_space(8.0);
    let body_height = (ui.available_height() - 48.0).max(260.0);
    ui.allocate_ui_with_layout(egui::vec2(ui.available_width(),body_height),super::i18n::layout(crate::config::resolved_rtl(&draft.config),egui::Align::Min),|ui|{
        ui.allocate_ui_with_layout(egui::vec2(142.0,body_height),super::i18n::vertical_layout(crate::config::resolved_rtl(&draft.config)),|ui|{
            for (tab,icon,title) in [(0,icons::PALETTE,"Appearance"),(1,icons::APP_WINDOW,"Application"),(2,icons::PLUG,"Connection"),(3,icons::TABS,"Workspace"),(4,icons::INFO,"Notifications"),(5,icons::CIRCUITRY,"Board"),(6,icons::ARROW_CLOCKWISE,"App updates")] {
                if tab==5 && !board.device_available { continue; }
                if dialog::navigation_button(ui,draft.tab==tab,icon,&super::i18n::text(title),138.0).clicked(){draft.tab=tab;}
            }
        });
        ui.separator();
        ui.allocate_ui_with_layout(egui::vec2(ui.available_width(),body_height),super::i18n::vertical_layout(crate::config::resolved_rtl(&draft.config)),|ui|{
            dialog::scroll_column(ui,"preferences-body",Some(body_height),|ui|{
                if (draft.tab==6||!draft.search.is_empty())&&draft.matches("App updates management releases telemetry enrollment cleanup uninstall server"){
                    outcome.client_action=app_management(draft,ui);
                }
                if draft.tab==0||!draft.search.is_empty(){
                    dialog::section(ui,icons::PALETTE,"Appearance",|ui|{
                        if draft.matches("Theme system light dark"){dialog::setting_row(ui,icons::SUN,"Theme",None,|ui|{super::dropdown::ComboBox::from_id_salt("theme").selected_text(super::i18n::text(&format!("{:?}",draft.config.theme))).show_ui(ui,|ui|{for t in [AppTheme::System,AppTheme::Light,AppTheme::Dark]{ui.dropdown_value(&mut draft.config.theme,t,super::i18n::text(&format!("{t:?}")));}});});}
                        if draft.matches("Palette Studio native"){dialog::setting_row(ui,icons::PALETTE,"Palette",None,|ui|{super::dropdown::ComboBox::from_id_salt("palette").selected_text(super::i18n::text(&format!("{:?}",draft.config.color_palette))).show_ui(ui,|ui|{for p in [ColorPalette::Studio,ColorPalette::Native]{ui.dropdown_value(&mut draft.config.color_palette,p,super::i18n::text(&format!("{p:?}")));}});});}
                        if draft.matches("Accent color"){dialog::setting_row(ui,icons::DIAMOND,"Accent color",None,|ui|{super::color_picker::accent_picker(ui,&mut draft.config.accent,180.0);});}
                        if draft.matches("UI scale size"){dialog::setting_row(ui,icons::MAGNIFYING_GLASS,"UI scale",None,|ui|{ui.add(egui::Slider::new(&mut draft.config.ui_scale,0.75..=2.0).step_by(0.05));});}
                        if draft.matches("Language System Persian English"){dialog::setting_row(ui,icons::GLOBE,"Language",None,|ui|{super::dropdown::ComboBox::from_id_salt("language").selected_text(super::i18n::text(&format!("{:?}",draft.config.language))).show_ui(ui,|ui|{ui.dropdown_value(&mut draft.config.language,AppLanguage::System,super::i18n::text("System language"));ui.dropdown_value(&mut draft.config.language,AppLanguage::English,"English");ui.dropdown_value(&mut draft.config.language,AppLanguage::Persian,"فارسی");});});}
                        if draft.matches("Direction automatic RTL LTR"){dialog::setting_row(ui,icons::GLOBE,"Direction",None,|ui|{super::dropdown::ComboBox::from_id_salt("direction").selected_text(match draft.config.direction{AppDirection::Auto=>super::i18n::text("Automatic"),AppDirection::Ltr=>super::i18n::text("Left to right"),AppDirection::Rtl=>super::i18n::text("Right to left")}).show_ui(ui,|ui|{for(d,label)in[(AppDirection::Auto,"Automatic"),(AppDirection::Ltr,"Left to right"),(AppDirection::Rtl,"Right to left")]{ui.dropdown_value(&mut draft.config.direction,d,super::i18n::text(label));}});});}
                    });ui.add_space(8.0);
                }
                if board.device_available && (draft.tab==5||!draft.search.is_empty()) && draft.matches("Board settings device identity network LEDs front panel AVR silent mode buzzer sound") {
                    dialog::section(ui,icons::CIRCUITRY,"Device settings",|ui| {
                        outcome.device_settings=device_settings_link(draft,ui).clicked();
                    });
                    if board.available { outcome.board_action = board_controls(ui, board); }
                    ui.add_space(8.0);
                }
                if draft.tab==1||!draft.search.is_empty(){
                    dialog::section(ui,icons::APP_WINDOW,"Application",|ui|{
                        if draft.matches("Single instance startup"){ui.checkbox(&mut draft.config.single_instance, crate::ui::i18n::text("Single instance")).on_hover_text(crate::ui::i18n::text("Additional launches activate this window. Changing this setting applies on the next launch."));}
                        if draft.matches("Close tray"){ui.checkbox(&mut draft.config.close_to_tray, crate::ui::i18n::text("Close to system tray"));}
                        if draft.matches("Always on top"){ui.checkbox(&mut draft.config.always_on_top, crate::ui::i18n::text("Always on top"));}
                        if draft.matches("Native dialog preferences"){ui.checkbox(&mut draft.config.native_dialogs, crate::ui::i18n::text("Open Preferences in a native window"));}
                    });ui.add_space(8.0);
                }
                if draft.tab==2||!draft.search.is_empty(){
                    dialog::section(ui,icons::PLUG,"Connection",|ui|{
                        if draft.matches("Automatically connect startup discover board") {
                            ui.checkbox(&mut draft.config.auto_connect,crate::ui::i18n::text("Automatically connect to a single discovered board"));
                        }
                        if draft.matches("Connection settings Endpoint hostname URL Username user USB serial port COM") {
                            outcome.connection_settings = connection_summary(draft, ui).clicked();
                        }
                    });ui.add_space(8.0);
                    if draft.matches("Share connection Bridge repeater USB TCP LAN discovery mDNS DNS-SD SSDP MNDP") {
                        dialog::section(ui,icons::BROADCAST,"Share connection",|ui|{
                            ui.checkbox(&mut draft.config.bridge.enabled,"Enable API bridge / repeater");
                            dialog::setting_row(ui,icons::GLOBE,"Listen address",None,|ui|{ui.text_edit_singleline(&mut draft.config.bridge.listen);});
                            dialog::setting_row(ui,icons::PLUG,"API port",None,|ui|{ui.add(egui::DragValue::new(&mut draft.config.bridge.port).range(1..=65535));});
                            dialog::setting_row(ui,icons::APP_WINDOW,"Hostname",None,|ui|{ui.text_edit_singleline(&mut draft.config.bridge.hostname);});
                            ui.horizontal_wrapped(|ui|{ui.checkbox(&mut draft.config.bridge.mdns,"mDNS / DNS-SD");ui.checkbox(&mut draft.config.bridge.ssdp,"SSDP");ui.checkbox(&mut draft.config.bridge.mndp,"MNDP");});
                            ui.weak("Shares only the selected connection. USB ownership and explicit Disconnect are preserved.");
                            ui.weak(if board.bridge["listening"] == true { "API listener running" } else { "API listener stopped" });
                            if let Some(error) = board.bridge["error"].as_str().filter(|s| !s.is_empty()) {
                                ui.colored_label(ui.visuals().error_fg_color, error);
                            }
                            if let Some(protocols) = board.bridge["discovery"]["active_protocols"].as_array() {
                                ui.weak(format!("Active discovery: {}", protocols.iter().filter_map(|v| v.as_str()).collect::<Vec<_>>().join(", ")));
                            }
                            if let Some(errors) = board.bridge["discovery"]["errors"].as_object() {
                                for (protocol,error) in errors { ui.colored_label(ui.visuals().error_fg_color, format!("{protocol}: {}", error.as_str().unwrap_or("Unavailable"))); }
                            }
                            if let Some(peers) = board.bridge["discovery"]["peers"].as_array() {
                                for peer in peers {
                                    if let Some(endpoint) = peer["endpoint"].as_str() {
                                        ui.horizontal_wrapped(|ui| {
                                            ui.label(endpoint);
                                            if ui.small_button(format!("{}  Copy endpoint",icons::COPY)).clicked() { ui.ctx().copy_text(endpoint.into()); }
                                        });
                                    }
                                }
                            }
                        });ui.add_space(8.0);
                    }
                }
                if draft.tab==4||!draft.search.is_empty(){
                    dialog::section(ui,icons::INFO,"Notifications",|ui|{
                        use crate::platform::notifications::Policy;
                        if draft.matches("In-app toast notification banner"){ui.checkbox(&mut draft.config.notifications.in_app, crate::ui::i18n::text("In-app toast banners"));}
                        if draft.matches("Windows system notification important all off"){dialog::setting_row(ui,icons::APP_WINDOW,"Windows notifications",None,|ui|{
                            super::dropdown::ComboBox::from_id_salt("notification-policy").selected_text(format!("{:?}",draft.config.notifications.system)).show_ui(ui,|ui|{
                                ui.dropdown_value(&mut draft.config.notifications.system,Policy::Off,"Off");
                                ui.dropdown_value(&mut draft.config.notifications.system,Policy::Important,"Important — failures, connection, settings, maintenance");
                                ui.dropdown_value(&mut draft.config.notifications.system,Policy::All,"All — includes routine command messages");
                            });
                        });}
                        if draft.matches("Notification sound silent"){ui.checkbox(&mut draft.config.notifications.sound, crate::ui::i18n::text("Allow Windows notification sound"));}
                        ui.label(crate::ui::i18n::text("Windows controls banner visibility and duration. Quiet-time settings are respected. System messages contain no raw API responses, credentials or device URLs."));
                        if ui.add_enabled(draft.config.notifications.system!=Policy::Off,egui::Button::new(crate::ui::i18n::text("Send test notification"))).clicked(){outcome.test_notification=true;}
                        ui.label(egui::RichText::new(crate::ui::i18n::text("Test uses saved notification preferences; save changes first. Delivery diagnostics are also available via --app-status.")).small().weak());
                    });ui.add_space(8.0);
                }
                if draft.tab==3||!draft.search.is_empty(){dialog::section(ui,icons::TABS,"Workspace",|ui|{
                    ui.checkbox(&mut draft.config.additive_cell_selection, crate::ui::i18n::text("Toggle multiple cells and rows without Ctrl")).on_hover_text(crate::ui::i18n::text("When off, Ctrl+click toggles items. Shift extends a range; dragging always replaces unless Ctrl/Shift is held."));
                    ui.checkbox(&mut draft.config.animate_cells, crate::ui::i18n::text("Animate cell state transitions")).on_hover_text(crate::ui::i18n::text("Disable for reduced motion. Cell geometry never changes."));
                    ui.checkbox(&mut draft.config.show_cell_indicators, crate::ui::i18n::text("Show cell LED-state indicators")).on_hover_text(crate::ui::i18n::text("Optional corner dots for white/red/off state. Hidden by default."));
                    super::dropdown::ComboBox::from_id_salt("preferences-cell-shape").selected_text(draft.config.cell_shape.label()).show_ui(ui,|ui|{for shape in [super::cell_pixels::Shape::Rounded,super::cell_pixels::Shape::Circle,super::cell_pixels::Shape::Hexagon]{ui.dropdown_value(&mut draft.config.cell_shape,shape,shape.label());}});
                    ui.checkbox(&mut draft.config.pixel_diagnostics, crate::ui::i18n::text("Show physical pixel indices (top first, CCW)")).on_hover_text(crate::ui::i18n::text("Applies to circle/hexagon cells. Zero-based 0–479; values are sampled from device RGB before brightness/correction."));
                    ui.label(format!("{} saved workspaces",draft.config.workspace_profiles.len()));
                    ui.label(crate::ui::i18n::text("Drag panel tabs to rearrange or detach them. Workspace profiles save the current layout."));
                    if ui.button(format!("{}  Export preferences…",icons::FLOPPY_DISK)).clicked(){
                        if let Some(path)=rfd::FileDialog::new().set_file_name("PakarController-preferences.json").add_filter("JSON",&["json"]).save_file(){match serde_json::to_vec_pretty(&draft.config).map_err(|e|e.to_string()).and_then(|v|std::fs::write(path,v).map_err(|e|e.to_string())){Ok(())=>draft.status="Exported preferences".into(),Err(e)=>draft.status=e}}
                    }
                    if ui.button(format!("{}  Import preferences…",icons::FOLDER_OPEN)).clicked(){
                        if let Some(path)=rfd::FileDialog::new().add_filter("JSON",&["json"]).pick_file(){match std::fs::read(path).map_err(|e|e.to_string()).and_then(|v|serde_json::from_slice::<AppConfig>(&v).map_err(|e|e.to_string())){Ok(v)=>draft.config=v,Err(e)=>draft.status=e}}
                    }
                });}
            });
        });
    });
    ui.separator();
    dialog::action_bar(
        ui,
        crate::config::resolved_rtl(&draft.config),
        |ui| {
            ui.label(
                egui::RichText::new(if draft.is_dirty() {
                    super::i18n::text("Unsaved changes")
                } else {
                    draft.status.clone()
                })
                .small()
                .weak(),
            );
        },
        |ui| {
            if dialog::primary_action_button(ui, icons::CHECK, "Save").clicked() {
                outcome.save = true;
            }
            if dialog::action_button(ui, icons::X, "Cancel").clicked() {
                outcome.close = true;
            }
        },
    );
    if draft.confirm_close {
        egui::Modal::new(egui::Id::new("preferences-discard-confirmation")).show(ui.ctx(), |ui| {
            ui.set_width(360.0);
            ui.heading(crate::ui::i18n::text("Discard preference changes?"));
            ui.label(crate::ui::i18n::text("Your saved preferences will be restored."));
            ui.add_space(12.0);
            dialog::action_row(ui, crate::config::resolved_rtl(&draft.config), |ui| {
                if dialog::primary_action_button(ui, icons::TRASH, "Discard").clicked() {
                    outcome.discard = true;
                }
                if dialog::action_button(ui, icons::X, "Keep editing").clicked() {
                    draft.confirm_close = false;
                }
            });
        });
    }
    outcome
}
pub fn draw(app: &mut PakarControllerApp, ctx: &egui::Context) {
    let Some(mut draft) = app.preferences.take() else {
        return;
    };
    draft.sync_connection_owner(app.transport, app.connection_enabled, &app.base, &app.user, &app.serial_port);
    // Workspace autosave is owner state, not a Preferences edit. Keep all three
    // baselines in sync so an open editor cannot erase newer per-board sessions.
    draft.sync_workspace_owner(&app.config.native_workspaces);
    app.board_silent.expire();
    let status = app.live_status();
    let available = status.is_some_and(crate::board_silent::supported);
    let device_available = status.is_some_and(|s| s.capabilities.is_some());
    let ready = available && status.is_some_and(|s| !s.programming_latch);
    let peer = status.and_then(crate::peer_http::Pin::from_status);
    if ready && app.board_silent.needs_refresh() { app.request_board_silent(None); }
    let board = BoardView {
        bridge: app.bridge.snapshot(),
        available, device_available, ready,
        value: app.board_silent.value(peer.as_ref(), app.command_route()),
        pending: app.board_silent.pending.is_some(), persisted: app.board_silent.persisted(),
        message: app.board_silent.message.clone(),
    };
    if draft.tab == 5 && !board.device_available { draft.tab = 2; }
    if draft.tab == 4 {
        let status = app.notification_snapshot();
        draft.status = format!(
            "OS accepted: {} · shown: {} · failed: {} · suppressed: {}",
            status["accepted"],
            status["shell_events"]["shown"],
            status["failed"],
            status["suppressed"]
        );
    }
    let mut open = true;
    let native = app.config.native_dialogs && !ctx.embed_viewports();
    let mut outcome = Outcome::default();
    if native {
        ctx.show_viewport_immediate(
            egui::ViewportId::from_hash_of("pakar-preferences"),
            egui::ViewportBuilder::default()
                .with_title(format!("{} — PakarController",super::i18n::text("Preferences")))
                .with_inner_size([780.0, 590.0])
                .with_min_inner_size([520.0, 420.0]),
            |child, _| {
                crate::config::apply(child, &draft.config);
                if child.input(|i| i.viewport().close_requested()) {
                    open = false;
                }
                egui::CentralPanel::default().show(child, |ui| {
                    egui::Frame::new().inner_margin(12).show(ui, |ui| {
                        outcome = editor(&mut draft, ui, &board);
                    });
                });
                if dialog::escape_pressed(child) {
                    outcome.close = true;
                }
            },
        );
    } else {
        let g = dialog::bounded_geometry(
            ctx.content_rect(),
            20.0,
            egui::vec2(780.0, 590.0),
            egui::vec2(520.0, 420.0),
            egui::vec2(1000.0, 780.0),
        );
        egui::Window::new(format!("{} {}", icons::GEAR, super::i18n::text("Preferences")))
            .id(egui::Id::new("pakar-preferences"))
            .open(&mut open)
            .default_rect(g.default_rect)
            .min_size(g.min_size)
            .max_size(g.max_size)
            .constrain_to(g.bounds)
            .frame(dialog::opaque_window_frame_from_context(ctx))
            .collapsible(false)
            .show(ctx, |ui| {
                outcome = editor(&mut draft, ui, &board);
            });
        if dialog::escape_pressed(ctx) {
            outcome.close = true;
        }
    }
    if let Some(action) = outcome.board_action { app.request_board_silent(action); }
    if let Some(action)=outcome.client_action { if !draft.is_dirty(){crate::client_management::request(action,app.config.client_management.clone(),ctx);} }
    if outcome.test_notification {
        app.notification_test();
    }
    if outcome.connection_settings && !draft.is_dirty() {
        app.show_connection = true;
        crate::config::apply(ctx, &app.config);
        return; // Close only this clean Preferences editor; the canonical dialog owns connection edits.
    }
    if outcome.device_settings && !draft.is_dirty() {
        app.device_settings.open=true;
        app.device_settings.page=0;
        crate::config::apply(ctx,&app.config);
        return;
    }
    if outcome.save {
        // Layout belongs to the workspace and can change while Preferences is open.
        draft.config.dock_layout = super::layout::serialize(&app.dock).unwrap_or_default();
        draft.sync_connection_owner(app.transport, app.connection_enabled, &app.base, &app.user, &app.serial_port);
        match draft.config.save() {
            Ok(()) => {
                app.config = draft.config.clone();
                app.palette = app.config.color_palette;
                crate::config::apply(ctx, &app.config);
                app.notify(
                    "Preferences saved",
                    "Application preferences were saved",
                    crate::messaging::Severity::Success,
                );
                return;
            }
            Err(e) => draft.status = e,
        }
    }
    if outcome.discard || ((!open || outcome.close) && !draft.is_dirty()) {
        crate::config::apply(ctx, &app.config);
        return;
    }
    if !open || outcome.close {
        draft.confirm_close = true;
    }
    if let Some(preview) = draft.take_preview_update() {
        crate::config::apply(ctx, &preview);
        app.palette = preview.color_palette;
    }
    app.preferences = Some(draft);
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn board_silent_preferences_distinguish_unknown_from_off_without_editing_app_config() {
        let saved = AppConfig::default();
        let draft = PreferencesDraft::new(saved.clone());
        for value in [None, Some(false), Some(true)] {
            let ctx = egui::Context::default();
            let board = BoardView { bridge: serde_json::Value::Null, available: true, device_available: true, ready: true, value, pending: false, persisted: true, message: String::new() };
            let mut action = None;
            let mut output = ctx.run_ui(egui::RawInput::default(), |ui| { action = board_controls(ui, &board); });
            output.textures_delta.clear();
            let texts: Vec<_> = output.shapes.iter().filter_map(|s| match &s.shape {
                egui::epaint::Shape::Text(t) => Some(t.galley.job.text.as_str()), _ => None,
            }).collect();
            assert!(texts.contains(&super::super::i18n::text(match value { None => "Unknown", Some(false) => "Off", Some(true) => "On" }).as_str()));
            assert!(action.is_none());
            assert_eq!(draft.config, saved); assert!(!draft.is_dirty());
        }
        assert_eq!(page_index("board"), Some(5));
    }
    #[test]
    fn board_settings_navigation_preserves_unsaved_application_preferences() {
        for dirty in [false,true] {
            let ctx=egui::Context::default();
            let mut draft=PreferencesDraft::new(AppConfig::default());
            if dirty { draft.config.ui_scale=1.25; }
            let before=draft.config.clone(); let mut enabled=false;
            let mut output=ctx.run_ui(egui::RawInput::default(),|ui| { enabled=device_settings_link(&draft,ui).enabled(); });
            output.textures_delta.clear();
            assert_eq!(enabled,!dirty); assert_eq!(draft.config,before);
        }
    }
    #[test]
    fn draft_preview_and_cancel_preserve_saved_preferences() {
        let saved = AppConfig::default();
        let mut d = PreferencesDraft::new(saved.clone());
        assert!(!d.is_dirty());
        d.config.close_to_tray = true;
        assert!(d.is_dirty());
        assert!(d.take_preview_update().is_some());
        assert!(d.take_preview_update().is_none());
        assert_eq!(d.saved_config, saved);
    }
    #[test]
    fn notification_deep_link_is_exact_and_preserves_dirty_draft() {
        let mut d = PreferencesDraft::new(AppConfig::default());
        d.config.notifications.system = crate::platform::notifications::Policy::Off;
        d.select_page(page_index("notifications").unwrap());
        assert_eq!(d.tab, 4);
        assert!(d.is_dirty());
        assert!(page_index("notification").is_none());
        d.select_page(50);
        assert_eq!(d.tab, 4);
    }
    #[test]
    fn external_connection_owner_updates_all_baselines_without_losing_unrelated_drafts() {
        for dirty in [false, true] {
            let mut draft = PreferencesDraft::new(AppConfig::default());
            if dirty { draft.config.close_to_tray = !draft.config.close_to_tray; }
            let close_to_tray = draft.config.close_to_tray;
            draft.sync_connection_owner(crate::connection::Method::Serial, true,
                "http://new-board.local", "operator", "COM22");
            assert_eq!(draft.is_dirty(), dirty, "a clean editor must not become dirty after IPC changes method");
            assert_eq!(draft.config.close_to_tray, close_to_tray);
            for baseline in [&draft.config, &draft.saved_config, &draft.previewed_config] {
                assert_eq!(baseline.connection_method, crate::connection::Method::Serial);
                assert!(baseline.connection_enabled);
                assert_eq!(baseline.endpoint, "http://new-board.local");
                assert_eq!(baseline.user, "operator");
                assert_eq!(baseline.serial_port, "COM22");
            }
        }
    }
    #[test]
    fn workspace_autosave_does_not_dirty_or_erase_an_open_preferences_draft() {
        let workspaces: crate::native_workspace::Store = serde_json::from_value(serde_json::json!({
            "entries":[{"identity":{"board":"board-a","application":"table"},
                "snapshot":{"selection":[1,21],"find":"1, 21"}}]
        })).unwrap();
        for dirty in [false, true] {
            let mut draft = PreferencesDraft::new(AppConfig::default());
            if dirty { draft.config.close_to_tray = true; }
            draft.sync_workspace_owner(&workspaces);
            assert_eq!(draft.is_dirty(), dirty);
            assert_eq!(draft.config.close_to_tray, dirty);
            for baseline in [&draft.config, &draft.saved_config, &draft.previewed_config] {
                assert_eq!(baseline.native_workspaces, workspaces);
            }
        }
    }
    #[test]
    fn connection_summary_is_contextual_and_dirty_preferences_cannot_open_another_editor() {
        for method in [crate::connection::Method::WiFi, crate::connection::Method::Serial] {
            for dirty in [false, true] {
                let ctx = egui::Context::default();
                let mut draft = PreferencesDraft::new(AppConfig::default());
                draft.sync_connection_owner(method, false, "http://ledboard.local", "operator", "COM22");
                if dirty { draft.config.close_to_tray = !draft.config.close_to_tray; }
                let mut enabled = false;
                let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                    enabled = connection_summary(&draft, ui).enabled();
                });
                output.textures_delta.clear();
                let texts: Vec<_> = output.shapes.iter().filter_map(|shape| match &shape.shape {
                    egui::epaint::Shape::Text(text) => Some(text.galley.job.text.as_str()), _ => None,
                }).collect();
                assert_eq!(enabled, !dirty);
                assert_eq!(texts.contains(&"http://ledboard.local"), method == crate::connection::Method::WiFi);
                assert_eq!(texts.contains(&"COM22"), method == crate::connection::Method::Serial);
                assert!(!texts.contains(&"operator"), "no duplicate authentication editor");
                assert!(!texts.contains(&"Connect") && !texts.contains(&"Disconnect"), "no second lifecycle action");
            }
        }
    }
}
