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
    pub status_rgb: bool,
}
impl Default for Items {
    fn default() -> Self {
        Self {
            connection: true,
            telemetry: true,
            message: true,
            status_rgb: true,
        }
    }
}
#[derive(Clone, Debug, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct PhysicalStatusRgb {
    pub available: bool,
    pub have_output: bool,
    pub enabled: bool,
    pub rgb: Option<[u8; 3]>,
    pub brightness: Option<u8>,
    pub state: String,
    pub revision: u32,
}
impl PhysicalStatusRgb {
    pub fn output_color(&self) -> Option<egui::Color32> {
        let [red, green, blue] = self.rgb?;
        // Actual Pealayer status_led_display_color: authoritative emitted RGB,
        // never apply brightness a second time in the mirror.
        (self.available && self.have_output).then(|| egui::Color32::from_rgb(red, green, blue))
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
    pub status_rgb: Option<&'a PhysicalStatusRgb>,
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
            if items.status_rgb && status.peer_known && let Some(led) = status.status_rgb
                && let Some(color) = led.output_color() {
                // Actual donor slot/radius and hover contract, retained beside
                // Pakar's existing status items and per-item visibility menu.
                let (rect, response) = ui.allocate_exact_size(egui::vec2(20.0, 22.0), egui::Sense::click());
                ui.painter().circle_filled(rect.center(), 6.0, color);
                let response = response.on_hover_text(format!(
                    "Physical status RGB · {}\nRGB {}, {}, {} · brightness {} · revision {}",
                    led.state, color.r(), color.g(), color.b(),
                    led.brightness.map(|v| v.to_string()).unwrap_or_else(|| "unknown".into()), led.revision));
                if response.clicked() { actions.push(Action::Hardware); }
                response.context_menu(|ui| {
                    compact_menu(ui); ui.strong("Physical status RGB"); ui.separator();
                    if ui.button("Copy color").clicked() {
                        ui.ctx().copy_text(format!("#{:02X}{:02X}{:02X}", color.r(), color.g(), color.b())); ui.close();
                    }
                    if ui.button(format!("{}  Hide status RGB", icons::EYE_SLASH)).clicked() {
                        items.status_rgb = false; actions.push(Action::Save); ui.close();
                    }
                });
            }
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
            changed |= ui.checkbox(&mut items.status_rgb, "Physical status RGB").changed();
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
    fn rgb_requires_observed_output_and_does_not_double_dim() {
        let mut led = PhysicalStatusRgb {available:true,have_output:true,rgb:Some([0,255,0]),brightness:Some(32),..Default::default()};
        assert_eq!(led.output_color(), Some(egui::Color32::GREEN));
        led.have_output = false; assert_eq!(led.output_color(), None);
        led.have_output = true; led.available = false; assert_eq!(led.output_color(), None);
        assert_eq!(PhysicalStatusRgb::default().output_color(), None);
    }
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
