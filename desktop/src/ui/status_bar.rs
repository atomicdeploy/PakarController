//! Pealayer status-bar preferences and per-item hide menus, controller-adapted.
use super::{cell_table::compact_menu, icons};
use eframe::egui;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Items {
    pub connection: bool,
    pub telemetry: bool,
    pub message: bool,
}
impl Default for Items {
    fn default() -> Self {
        Self {
            connection: true,
            telemetry: true,
            message: true,
        }
    }
}
pub enum Action {
    Save,
    Refresh,
    Connect,
    Hardware,
    Network,
    Preferences,
}
pub struct StatusBar<'a> {
    pub peer_known: bool,
    pub avr_available: bool,
    pub display_available: bool,
    pub network_available: bool,
    pub telemetry_available: bool,
    pub message: &'a str,
    pub hostname: &'a str,
    pub station: &'a str,
    pub avr_online: bool,
    pub tm1637_ready: bool,
    pub websocket_live: bool,
    pub serial_live: bool,
}
fn hide_menu(response: egui::Response, label: &str, visible: &mut bool) -> bool {
    let mut changed = false;
    response.context_menu(|ui| {
        compact_menu(ui);
        ui.strong(label);
        ui.separator();
        if ui
            .button(format!("{}  Hide {label}", icons::EYE_SLASH))
            .clicked()
        {
            *visible = false;
            changed = true;
            ui.close();
        }
    });
    changed
}
pub fn draw(ui: &mut egui::Ui, status: StatusBar<'_>, items: &mut Items) -> Vec<Action> {
    let mut actions = Vec::new();
    let panel = egui::Panel::bottom("pakar_status_bar").show(ui, |ui| {
        ui.set_min_height(22.0); // Keep an empty right-click restore target.
        ui.horizontal(|ui| {
            if items.connection {
                let text = if status.hostname.is_empty() {
                    "Discovering PakarController"
                } else {
                    status.hostname
                };
                let response = ui.add(
                    egui::Label::new(format!("{}  {text}  {}", icons::CIRCUITRY, status.station))
                        .sense(egui::Sense::click())
                        .selectable(false),
                );
                if response.clicked() {
                    actions.push(Action::Connect);
                }
                if hide_menu(response, "Connection", &mut items.connection) {
                    actions.push(Action::Save);
                }
            }
            if items.telemetry && status.telemetry_available {
                let text = if status.peer_known {
                    let mut parts = Vec::new();
                    if status.avr_available { parts.push(if status.avr_online { "AVR live" } else { "AVR offline" }); }
                    if status.display_available { parts.push(if status.tm1637_ready { "TM1637 ready" } else { "TM1637 no ACK" }); }
                    if status.network_available { parts.push(if status.websocket_live { "WS live" } else { "WS offline" }); }
                    if status.serial_live { parts.push("USB live"); }
                    parts.join(" · ")
                } else {
                    "Device state unavailable".into()
                };
                let response = ui.add(
                    egui::Label::new(text)
                        .sense(egui::Sense::click())
                        .selectable(false),
                );
                if response.clicked() {
                    actions.push(Action::Hardware);
                }
                if hide_menu(response, "Telemetry", &mut items.telemetry) {
                    actions.push(Action::Save);
                }
            }
            if items.message {
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let response = ui.add(
                        egui::Label::new(egui::RichText::new(status.message).small())
                            .truncate()
                            .selectable(false),
                    );
                    if hide_menu(response, "Message", &mut items.message) {
                        actions.push(Action::Save);
                    }
                });
            }
        });
    });
    panel
        .response
        .interact(egui::Sense::click())
        .context_menu(|ui| {
            compact_menu(ui);
            ui.strong("Status bar");
            ui.separator();
            let mut changed = ui.checkbox(&mut items.connection, "Connection").changed();
            changed |= ui
                .checkbox(&mut items.telemetry, "Hardware telemetry")
                .changed();
            changed |= ui.checkbox(&mut items.message, "Status message").changed();
            if changed {
                actions.push(Action::Save);
            }
            ui.separator();
            for (icon, label, action) in [
                (icons::ARROW_CLOCKWISE, "Refresh", Action::Refresh),
                (icons::PLUG, "Connect…", Action::Connect),
                (icons::CIRCUITRY, "Hardware monitor", Action::Hardware),
                (icons::GLOBE, "Network & cloud", Action::Network),
                (icons::GEAR, "Preferences…", Action::Preferences),
            ] {
                if matches!(&action, Action::Hardware) && !status.telemetry_available { continue; }
                if matches!(&action, Action::Network) && !status.network_available { continue; }
                if ui.button(format!("{icon}  {label}")).clicked() {
                    actions.push(action);
                    ui.close();
                }
            }
        });
    actions
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn status_preferences_roundtrip_and_missing_fields_default() {
        let mut items = Items::default();
        items.telemetry = false;
        assert_eq!(
            serde_json::from_str::<Items>(&serde_json::to_string(&items).unwrap()).unwrap(),
            items
        );
        assert_eq!(
            serde_json::from_str::<Items>("{}").unwrap(),
            Items::default()
        );
    }
}
