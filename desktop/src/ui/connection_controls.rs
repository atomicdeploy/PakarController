//! Pealayer src/ui/menu.rs connection-dropdown idioms adapted to Pakar's
//! independent WebSocket/USB workers. Port enumeration is lazy and factual.
use super::icons;
use crate::Transport;
use eframe::egui;

#[derive(Debug, PartialEq)]
pub enum Action {
    Route(Transport),
    Port(String),
    ReconnectWebSocket,
    ReconnectUsb,
    Configure,
}

fn routes(ui: &mut egui::Ui, route: Transport, actions: &mut Vec<Action>) {
    ui.label(egui::RichText::new("Send commands through").small().weak());
    for (value, label) in [
        (Transport::WiFi, "Wi-Fi"),
        (Transport::Serial, "USB serial"),
        (Transport::Both, "Automatic · Wi-Fi, then USB"),
    ] {
        if ui.selectable_label(route == value, label).clicked() {
            if route != value { actions.push(Action::Route(value)); }
            ui.close();
        }
    }
}

fn heading(live: bool, label: &str, ui: &egui::Ui) -> egui::RichText {
    egui::RichText::new(format!("●  {label}  ▾")).color(if live {
        super::palette::color(crate::config::live().color_palette, ui.visuals().dark_mode, "green")
    } else {
        ui.visuals().text_color()
    })
}

pub fn websocket(ui: &mut egui::Ui, live: bool, endpoint: &str, route: Transport,
    actions: &mut Vec<Action>) -> egui::Response {
    ui.push_id("websocket-control", |ui| {
        ui.menu_button(heading(live, "WebSocket", ui), |ui| {
            ui.strong(if live { "WebSocket connected" } else { "WebSocket offline" });
            ui.label(egui::RichText::new(endpoint).small().weak());
            ui.separator();
            routes(ui, route, actions);
            ui.separator();
            if ui.button(format!("{}  Reconnect WebSocket", icons::ARROW_CLOCKWISE)).clicked() {
                actions.push(Action::ReconnectWebSocket);
                ui.close();
            }
            if ui.button(format!("{}  Connection settings…", icons::GEAR)).clicked() {
                actions.push(Action::Configure);
                ui.close();
            }
        }).response.on_hover_text("WebSocket connection and command transport")
    }).inner
}

pub fn usb(ui: &mut egui::Ui, live: bool, port: &str, route: Transport,
    identity_pending: bool, ports: impl FnOnce() -> Result<Vec<String>, String>,
    actions: &mut Vec<Action>) -> egui::Response {
    ui.push_id("usb-control", |ui| {
        ui.menu_button(heading(live, "USB", ui), |ui| {
            ui.strong(if live { "USB connected" } else { "USB offline" });
            ui.label(egui::RichText::new(if port.is_empty() { "No port selected" } else { port }).small().weak());
            ui.separator();
            routes(ui, route, actions);
            ui.separator();
            ui.label(egui::RichText::new("Serial port").small().weak());
            ui.add_enabled_ui(!identity_pending, |ui| {
                if ui.selectable_label(port.is_empty(), "Disconnected").clicked() {
                    if !port.is_empty() { actions.push(Action::Port(String::new())); }
                    ui.close();
                }
                match ports() {
                    Ok(mut names) => {
                        names.sort();
                        names.dedup();
                        if !port.is_empty() && !names.iter().any(|name| name == port) {
                            ui.add_enabled(false, egui::Button::new(format!("{port} · unavailable")));
                        }
                        if names.is_empty() { ui.label(egui::RichText::new("No serial ports detected").weak()); }
                        for name in names {
                            if ui.selectable_label(port == name, &name).clicked() {
                                if port != name { actions.push(Action::Port(name)); }
                                ui.close();
                            }
                        }
                    }
                    Err(error) => { ui.colored_label(ui.visuals().error_fg_color, format!("Port scan failed: {error}")); }
                }
                ui.separator();
                if ui.add_enabled(!port.is_empty(), egui::Button::new(format!("{}  Reconnect USB", icons::ARROW_CLOCKWISE))).clicked() {
                    actions.push(Action::ReconnectUsb);
                    ui.close();
                }
            });
            if identity_pending { ui.label("Identity write in progress; wait before changing the port."); }
            if ui.button(format!("{}  Connection settings…", icons::GEAR)).clicked() {
                actions.push(Action::Configure);
                ui.close();
            }
        }).response.on_hover_text("USB port, reconnect and command transport")
    }).inner
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn offline_dropdowns_open_independently_and_usb_enumerates_only_when_open() {
        let ctx = egui::Context::default();
        let scans = std::cell::Cell::new(0);
        let mut responses = None;
        let mut frame = |events| {
            let mut actions = Vec::new();
            let out = ctx.run_ui(egui::RawInput { events, ..Default::default() }, |ui| {
                ui.horizontal(|ui| {
                    let ws = websocket(ui, false, "http://ledboard.local", Transport::Both, &mut actions);
                    let usb = usb(ui, false, "", Transport::Both, false, || {
                        scans.set(scans.get() + 1);
                        Ok(vec!["COM7".into(), "COM7".into()])
                    }, &mut actions);
                    responses = Some([ws, usb]);
                });
            });
            (out, actions, responses.clone().unwrap())
        };
        let (_, actions, initial) = frame(vec![]);
        assert!(actions.is_empty());
        assert_eq!(scans.get(), 0);
        let ws_id = egui::Popup::default_response_id(&initial[0]);
        let usb_id = egui::Popup::default_response_id(&initial[1]);
        assert_ne!(ws_id, usb_id);
        egui::Popup::open_id(&ctx, usb_id);
        frame(vec![]); // First popup frame measures its area.
        let (out, actions, _) = frame(vec![]);
        assert!(actions.is_empty());
        assert_eq!(scans.get(), 2);
        let port = out.shapes.iter().find_map(|s| match &s.shape {
            egui::epaint::Shape::Text(t) if t.galley.job.text == "COM7" => Some(t.pos + t.galley.rect.center().to_vec2()),
            _ => None,
        }).expect("actual detected port is in the popup");
        frame(vec![egui::Event::PointerMoved(port), egui::Event::PointerButton {
            pos: port, button: egui::PointerButton::Primary, pressed: true, modifiers: egui::Modifiers::NONE,
        }]);
        let (_, actions, _) = frame(vec![egui::Event::PointerButton {
            pos: port, button: egui::PointerButton::Primary, pressed: false, modifiers: egui::Modifiers::NONE,
        }]);
        assert_eq!(actions, vec![Action::Port("COM7".into())]);
        assert!(!egui::Popup::is_id_open(&ctx, usb_id));
        egui::Popup::open_id(&ctx, ws_id);
        let before = scans.get();
        frame(vec![]);
        assert_eq!(scans.get(), before, "WebSocket popup must not scan USB");
    }
}
