//! Pealayer's shared dropdown, explicit endpoint and Connect/Disconnect lifecycle.
//! Discovery is an aid, not a connection method or permission to open a port.
use super::dropdown::{self, DropdownUiExt};
use super::{dialog, icons};
use crate::Transport;
use eframe::egui;

#[derive(Debug, PartialEq)]
pub enum Action {
    Method(Transport),
    Lifecycle(bool),
    Port(String),
    Configure,
}

pub fn method_label(method: Transport) -> &'static str {
    match method {
        Transport::WiFi => "Wi-Fi",
        Transport::Serial => "USB serial",
    }
}

/// Only a local draft is changed while painting; the owner serializes handoff.
pub fn method_picker(ui: &mut egui::Ui, method: &mut Transport, busy: bool) {
    ui.add_enabled_ui(!busy, |ui| {
        dropdown::ComboBox::from_id_salt("connection-method")
            .selected_text(method_label(*method))
            .width(180.0)
            .show_ui(ui, |ui| {
                ui.dropdown_value(method, Transport::WiFi, "Wi-Fi");
                ui.dropdown_value(method, Transport::Serial, "USB serial");
            });
    });
}

/// Called inside an open, enabled USB popup only. Disconnect never clears a port.
fn port_choices(
    ui: &mut egui::Ui,
    port: &mut String,
    ports: impl FnOnce() -> Result<Vec<String>, String>,
) {
    if !ui.is_enabled() {
        return; // A disabled egui sizing pass must not scan the OS.
    }
    match ports() {
        Ok(mut names) => {
            names.sort();
            names.dedup();
            if !port.is_empty() && !names.contains(port) {
                ui.add_enabled_ui(false, |ui| {
                    ui.dropdown_choice(false, format!("{port} · unavailable"))
                });
            }
            if names.is_empty() {
                ui.weak("No serial ports detected; enter a port or hardware path.");
            }
            for name in names {
                ui.dropdown_value(port, name.clone(), &name);
            }
        }
        Err(error) => {
            ui.colored_label(
                ui.visuals().error_fg_color,
                format!("Port scan failed: {error}"),
            );
        }
    }
}

pub struct State<'a> {
    pub method: Transport,
    pub enabled: bool,
    pub live: bool,
    pub endpoint: &'a str,
    pub port: &'a str,
    pub identity_pending: bool,
}

pub struct Responses {
    pub dropdown: egui::Response,
    pub lifecycle: egui::Response,
}

pub fn draw(
    ui: &mut egui::Ui,
    state: State<'_>,
    ports: impl FnOnce() -> Result<Vec<String>, String>,
    actions: &mut Vec<Action>,
) -> Responses {
    let owner = ui.make_persistent_id("connection-dropdown");
    let color = if state.live {
        super::palette::color(
            crate::config::live().color_palette,
            ui.visuals().dark_mode,
            "green",
        )
    } else {
        ui.visuals().text_color()
    };
    let menu = ui.menu_button(
        egui::RichText::new(format!(
            "●  {}  {}",
            method_label(state.method),
            icons::CARET_DOWN
        ))
        .color(color),
        |ui| {
            dropdown::menu_ui(ui, owner, |ui| {
                ui.set_min_width(240.0);
                ui.strong(if state.live {
                    "Connected"
                } else if state.enabled {
                    "Connecting…"
                } else {
                    "Disconnected"
                });
                ui.weak(match state.method {
                    Transport::WiFi => state.endpoint,
                    Transport::Serial if state.port.is_empty() => "No port selected",
                    Transport::Serial => state.port,
                });
                ui.separator();
                ui.weak("Connection method");
                ui.add_enabled_ui(!state.identity_pending, |ui| {
                    for method in [Transport::WiFi, Transport::Serial] {
                        if ui
                            .dropdown_choice(method == state.method, method_label(method))
                            .clicked()
                            && method != state.method
                        {
                            actions.push(Action::Method(method));
                        }
                    }
                    if state.method == Transport::Serial && !state.identity_pending {
                        ui.separator();
                        ui.weak("Serial port");
                        let mut selected = state.port.to_owned();
                        port_choices(ui, &mut selected, ports);
                        if selected != state.port {
                            actions.push(Action::Port(selected));
                        }
                    }
                });
                if state.identity_pending {
                    ui.weak("Identity write in progress; connection changes are locked.");
                }
                ui.separator();
                if ui
                    .dropdown_choice(false, format!("{}  Connection settings…", icons::GEAR))
                    .clicked()
                {
                    actions.push(Action::Configure);
                }
            })
        },
    );
    if menu.inner.is_none() {
        dropdown::clear_menu(ui.ctx(), owner);
    }
    let endpoint_present = match state.method {
        Transport::WiFi => !state.endpoint.trim().is_empty(),
        Transport::Serial => !state.port.trim().is_empty(),
    };
    let lifecycle = ui.add_enabled(
        !state.identity_pending && (state.enabled || endpoint_present),
        egui::Button::new(if state.enabled {
            "Disconnect"
        } else {
            "Connect"
        }),
    );
    if lifecycle.clicked() {
        actions.push(Action::Lifecycle(!state.enabled));
    }
    Responses {
        dropdown: menu.response,
        lifecycle,
    }
}

#[derive(Clone)]
pub struct Draft {
    pub endpoint: String,
    pub user: String,
    pub password: String,
    pub port: String,
}

fn field(ui: &mut egui::Ui, icon: &str, label: &str, value: &mut String, password: bool) {
    dialog::setting_row(ui, icon, label, None, |ui| {
        ui.add_sized(
            [ui.available_width().min(340.0), 28.0],
            dialog::singleline_text_edit(value).password(password),
        );
    });
}

pub fn details(
    ui: &mut egui::Ui,
    method: Transport,
    editable: bool,
    draft: &mut Draft,
    ports: impl FnOnce() -> Result<Vec<String>, String>,
) {
    ui.add_enabled_ui(editable, |ui| match method {
        Transport::WiFi => dialog::section(ui, icons::GLOBE, "Wi-Fi", |ui| {
            field(ui, icons::GLOBE, "Endpoint", &mut draft.endpoint, false);
            field(ui, icons::LOCK, "Username", &mut draft.user, false);
            field(ui, icons::LOCK, "Password", &mut draft.password, true);
        }),
        Transport::Serial => dialog::section(ui, icons::PLUG, "USB serial", |ui| {
            field(
                ui,
                icons::PLUG,
                "Port or hardware path",
                &mut draft.port,
                false,
            );
            dropdown::ComboBox::from_id_salt("connection-port")
                .width(240.0)
                .selected_text(if draft.port.is_empty() {
                    "Select a port"
                } else {
                    &draft.port
                })
                .show_ui(ui, |ui| port_choices(ui, &mut draft.port, ports));
        }),
    });
}

#[cfg(test)]
mod tests {
    use super::*;
    fn text(output: &egui::FullOutput) -> Vec<String> {
        output
            .shapes
            .iter()
            .filter_map(|s| match &s.shape {
                egui::epaint::Shape::Text(t) => Some(t.galley.job.text.clone()),
                _ => None,
            })
            .collect()
    }
    fn key(key: egui::Key) -> egui::Event {
        egui::Event::Key {
            key,
            physical_key: None,
            pressed: true,
            repeat: false,
            modifiers: egui::Modifiers::NONE,
        }
    }
    #[test]
    fn one_contextual_control_lazy_usb_scan_and_busy_guard() {
        for method in [Transport::WiFi, Transport::Serial] {
            for busy in [false, true] {
                let ctx = egui::Context::default();
                let scans = std::cell::Cell::new(0);
                let mut frame = |events| {
                    let mut responses = None;
                    let mut actions = Vec::new();
                    let mut out = ctx.run_ui(
                        egui::RawInput {
                            events,
                            ..Default::default()
                        },
                        |ui| {
                            ui.horizontal(|ui| {
                                responses = Some(draw(
                                    ui,
                                    State {
                                        method,
                                        enabled: false,
                                        live: false,
                                        endpoint: "http://ledboard.local",
                                        port: "COM7",
                                        identity_pending: busy,
                                    },
                                    || {
                                        scans.set(scans.get() + 1);
                                        Ok(vec!["COM7".into(), "COM8".into(), "COM8".into()])
                                    },
                                    &mut actions,
                                ))
                            });
                        },
                    );
                    out.textures_delta.clear();
                    (out, responses.unwrap(), actions)
                };
                let (out, initial, actions) = frame(vec![]);
                assert!(actions.is_empty());
                assert_eq!(scans.get(), 0);
                assert_eq!(
                    text(&out)
                        .iter()
                        .filter(|s| s.contains(icons::CARET_DOWN))
                        .count(),
                    1
                );
                assert_eq!(initial.lifecycle.enabled(), !busy);
                let popup = egui::Popup::default_response_id(&initial.dropdown);
                egui::Popup::open_id(&ctx, popup);
                frame(vec![]);
                let (out, _, _) = frame(vec![]);
                let texts = text(&out);
                assert_eq!(
                    texts.iter().any(|s| s == "http://ledboard.local"),
                    method == Transport::WiFi
                );
                assert_eq!(
                    texts.iter().any(|s| s == "Serial port"),
                    method == Transport::Serial && !busy
                );
                assert_eq!(scans.get() > 0, method == Transport::Serial && !busy);
                frame(vec![key(egui::Key::End)]);
                let (_, _, actions) = frame(vec![key(egui::Key::Enter)]);
                assert_eq!(
                    actions,
                    vec![Action::Configure],
                    "identity guard must not trap settings navigation"
                );
            }
        }
    }
    #[test]
    fn dialog_details_are_selected_method_only_and_do_not_probe_closed_ports() {
        for method in [Transport::WiFi, Transport::Serial] {
            let ctx = egui::Context::default();
            let mut draft = Draft {
                endpoint: "http://ledboard.local".into(),
                user: "operator".into(),
                password: "".into(),
                port: "COM7".into(),
            };
            let mut out = ctx.run_ui(egui::RawInput::default(), |ui| {
                details(ui, method, true, &mut draft, || {
                    panic!("closed port popup must not enumerate")
                });
            });
            out.textures_delta.clear();
            let texts = text(&out);
            assert_eq!(
                texts.iter().any(|s| s == "Endpoint"),
                method == Transport::WiFi
            );
            assert_eq!(
                texts.iter().any(|s| s == "Username"),
                method == Transport::WiFi
            );
            assert_eq!(
                texts.iter().any(|s| s == "Port or hardware path"),
                method == Transport::Serial
            );
            assert_eq!(draft.port, "COM7");
        }
    }
    #[test]
    fn lifecycle_is_one_explicit_action_and_disconnect_retains_port() {
        for enabled in [false, true] {
            let ctx = egui::Context::default();
            let mut frame = |events| {
                let mut response = None;
                let mut actions = Vec::new();
                let mut out = ctx.run_ui(
                    egui::RawInput {
                        events,
                        ..Default::default()
                    },
                    |ui| {
                        ui.horizontal(|ui| {
                            response = Some(draw(
                                ui,
                                State {
                                    method: Transport::Serial,
                                    enabled,
                                    live: enabled,
                                    endpoint: "",
                                    port: "COM7",
                                    identity_pending: false,
                                },
                                || panic!("lifecycle never scans ports"),
                                &mut actions,
                            ))
                        });
                    },
                );
                out.textures_delta.clear();
                (response.unwrap(), actions)
            };
            let (response, _) = frame(vec![]);
            let pos = response.lifecycle.rect.center();
            frame(vec![
                egui::Event::PointerMoved(pos),
                egui::Event::PointerButton {
                    pos,
                    button: egui::PointerButton::Primary,
                    pressed: true,
                    modifiers: egui::Modifiers::NONE,
                },
            ]);
            let (_, actions) = frame(vec![egui::Event::PointerButton {
                pos,
                button: egui::PointerButton::Primary,
                pressed: false,
                modifiers: egui::Modifiers::NONE,
            }]);
            assert_eq!(actions, vec![Action::Lifecycle(!enabled)]);
        }
    }
}
