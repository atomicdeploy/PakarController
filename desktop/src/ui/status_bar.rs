//! Pealayer-style persistent bottom status surface.

use eframe::egui;

pub struct StatusBar<'a> {
    pub peer_known: bool,
    pub message: &'a str,
    pub hostname: &'a str,
    pub station: &'a str,
    pub avr_online: bool,
    pub tm1637_ready: bool,
    pub websocket_live: bool,
    pub serial_live: bool,
}

pub fn draw(ui: &mut egui::Ui, status: StatusBar<'_>) {
    egui::Panel::bottom("pakar_status_bar").show(ui, |ui| {
        ui.horizontal(|ui| {
            let device_color = if status.avr_online {
                egui::Color32::LIGHT_GREEN
            } else {
                egui::Color32::GOLD
            };
            ui.colored_label(device_color, crate::ui::icons::CIRCUITRY);
            ui.strong(if status.hostname.is_empty() {
                "Discovering PakarController"
            } else {
                status.hostname
            });
            if !status.station.is_empty() {
                ui.label(status.station);
            }
            ui.separator();
            if status.peer_known {
                ui.label(format!(
                    "{} AVR · {} TM1637 · {} WS · {} USB",
                    if status.avr_online { "live" } else { "offline" },
                    if status.tm1637_ready {
                        "ready"
                    } else {
                        "no ACK"
                    },
                    if status.websocket_live {
                        "live"
                    } else {
                        "offline"
                    },
                    if status.serial_live {
                        "live"
                    } else {
                        "offline"
                    },
                ));
            } else {
                ui.label("Device state unavailable");
            }
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.add(
                    egui::Label::new(egui::RichText::new(status.message).small())
                        .truncate()
                        .selectable(false),
                );
            });
        });
    });
}
