//! Uses the existing fixed-geometry dialog/dropdown controls; never joins while painting.
use super::{dialog, dropdown, dropdown::DropdownUiExt, icons};
use crate::platform::host_wifi::{self, Profile};
use eframe::egui;
use std::sync::{Arc, Mutex};

#[derive(Default)]
struct State {
    profiles: Vec<Profile>,
    selected: Option<Profile>,
    busy: bool,
    message: String,
    confirm: bool,
}
enum Action {
    Refresh,
    Join(Profile),
    Settings,
}

pub fn draw(ui: &mut egui::Ui, enabled: bool) {
    let id = egui::Id::new("host-wifi-controls");
    let shared = ui.ctx().data_mut(|data| {
        data.get_temp_mut_or_default::<Arc<Mutex<State>>>(id)
            .clone()
    });
    let mut action = None;
    let Ok(mut state) = shared.lock() else {
        return;
    };
    ui.collapsing(format!("{}  This PC’s Wi-Fi", icons::WIFI_HIGH), |ui| {
        ui.weak("Join the board’s access point or its router. This changes the PC’s network, not board settings.");
        ui.add_enabled_ui(enabled && !state.busy, |ui| {
            dropdown::ComboBox::from_id_salt("host-wifi-profile").width(340.0)
                .selected_text(state.selected.as_ref().map(|p| p.name.as_str()).unwrap_or("Select a saved Wi-Fi profile"))
                .show_ui(ui, |ui| {
                    for profile in state.profiles.clone() {
                        let label = format!("{} · {}", profile.name, profile.adapter);
                        ui.dropdown_value(&mut state.selected, Some(profile), label);
                    }
                });
            ui.horizontal_wrapped(|ui| {
                if dialog::action_button(ui, icons::ARROW_CLOCKWISE, "Refresh profiles").clicked() { action = Some(Action::Refresh); }
                if dialog::action_button(ui, icons::GEAR, "Wi-Fi settings…").clicked() { action = Some(Action::Settings); }
                if ui.add_enabled_ui(state.selected.is_some(), |ui| dialog::primary_action_button(ui, icons::WIFI_HIGH, "Join…")).inner.clicked() {
                    state.confirm = true;
                }
            });
            if state.confirm {
                ui.weak("Switch Wi-Fi now? Other network sessions may disconnect.");
                ui.horizontal(|ui| {
                    if dialog::action_button(ui, icons::CHECK, "Switch network").clicked() {
                        if let Some(profile) = state.selected.clone() { action = Some(Action::Join(profile)); }
                        state.confirm = false;
                    }
                    if dialog::action_button(ui, icons::X, "Cancel").clicked() { state.confirm = false; }
                });
            }
        });
        if state.busy { ui.spinner(); }
        if !state.message.is_empty() { ui.label(&state.message); }
    });
    if let Some(action) = action {
        state.busy = true;
        state.message.clear();
        let shared = shared.clone();
        let ctx = ui.ctx().clone();
        // WLAN service calls never block the render/transport thread.
        std::thread::spawn(move || {
            let result = match action {
                Action::Refresh => host_wifi::profiles().map(|profiles| {
                    (
                        Some(profiles),
                        "Profiles read from Windows. New networks can be added in Wi-Fi settings."
                            .into(),
                    )
                }),
                Action::Join(profile) => host_wifi::join(&profile).map(|message| (None, message)),
                Action::Settings => host_wifi::settings().map(|message| (None, message)),
            };
            if let Ok(mut state) = shared.lock() {
                state.busy = false;
                match result {
                    Ok((profiles, message)) => {
                        if let Some(profiles) = profiles {
                            if state
                                .selected
                                .as_ref()
                                .is_some_and(|p| !profiles.contains(p))
                            {
                                state.selected = None;
                            }
                            state.profiles = profiles;
                        }
                        state.message = message;
                    }
                    Err(error) => state.message = error,
                }
            }
            ctx.request_repaint();
        });
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn rendering_does_not_join_or_scan() {
        let ctx = eframe::egui::Context::default();
        let _ = ctx.run_ui(Default::default(), |ui| super::draw(ui, true));
        let state = ctx
            .data(|data| {
                data.get_temp::<super::Arc<super::Mutex<super::State>>>(eframe::egui::Id::new(
                    "host-wifi-controls",
                ))
            })
            .unwrap();
        let state = state.lock().unwrap();
        assert!(!state.busy && state.profiles.is_empty() && !state.confirm);
    }
}
