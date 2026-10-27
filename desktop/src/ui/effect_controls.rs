//! Output-panel controls corresponding to Flutter's OutputPanel. The actual
//! Pealayer dialog::section/action_button and control_density helpers own card
//! geometry; capability snapshots decide availability, never a remembered draft.
use super::{control_density, dialog, icons};
use crate::capabilities::Mode;
use eframe::egui;
use super::dropdown::DropdownUiExt;

/// Reuse Pealayer's actual dropdown focus/geometry and authoritative board
/// settings ownership. A selected item is the observed driver, never a draft.
pub fn backend_picker(ui: &mut egui::Ui, status: &crate::DeviceStatus) -> Option<&'static str> {
    let capability = status.capabilities.as_ref()?.led_output()?;
    let driver = &status.led_output["driver"];
    let active = driver["backend"].as_str().unwrap_or(&capability.backend);
    let choices: Vec<_> = ["gpio-static", "rmt-dma"].into_iter()
        .filter(|backend| capability.supports_backend(backend)).collect();
    let mut request = None;
    dialog::setting_row(ui, icons::CIRCUITRY, "LED driver", None, |ui| {
        if choices.is_empty() {
            ui.weak(crate::led_backend::label(active));
        } else {
            let pending = driver["switch_pending"] == true;
            ui.add_enabled_ui(!pending && !status.programming_latch, |ui| {
                super::dropdown::ComboBox::from_id_salt("led-wire-backend")
                    .width(ui.available_width().clamp(110.0, 220.0))
                    .selected_text(crate::led_backend::label(active))
                    .show_ui(ui, |ui| {
                        for backend in choices {
                            if ui.dropdown_choice(active == backend, crate::led_backend::label(backend)).clicked()
                                && (active != backend || !crate::led_backend::confirmed(status, backend))
                                && crate::led_backend::allowed(status, backend) {
                                request = Some(backend);
                            }
                        }
                    });
            });
        }
    });
    if driver["switch_pending"] == true {
        ui.horizontal(|ui| {
            ui.spinner();
            ui.weak(format!("Switching to {}…", crate::led_backend::label(driver["requested_backend"].as_str().unwrap_or(""))));
        });
    } else if driver["switch_error"].as_i64().is_some_and(|error| error != 0)
        || status.led_output["backend_settings_error"].as_str().is_some_and(|error| !error.is_empty()) {
        ui.colored_label(ui.visuals().warn_fg_color, format!("Driver {} · switch error {} · settings error {}",
            crate::led_backend::label(active), driver["switch_error"], status.led_output["backend_settings_error"]));
    }
    request
}

pub fn visible_modes(modes: impl IntoIterator<Item = Mode>) -> Vec<Mode> {
    let mut visible = Vec::<Mode>::new();
    for mode in modes {
        if mode.id != "snake"
            && !mode.id.is_empty()
            && !visible.iter().any(|known| known.id == mode.id)
        {
            visible.push(mode);
        }
    }
    visible
}

/// No heading, padding or empty card is allocated for a fixed-role board.
pub fn mode_picker(
    ui: &mut egui::Ui,
    modes: &[Mode],
    current: Option<&str>,
    live_modes: &[Mode],
) -> Option<Mode> {
    if modes.len() < 2 {
        return None;
    }
    let mut requested = None;
    dialog::section(ui, icons::PLAY, "Board modes", |ui| {
        ui.horizontal_wrapped(|ui| {
            for mode in modes {
                let live = live_modes.iter().any(|known| known.id == mode.id);
                let selected = current == Some(mode.id.as_str());
                if choice(ui, icons::PLAY, &mode.label, selected, live).clicked() {
                    requested = Some(mode.clone());
                }
            }
        });
    });
    ui.add_space(10.0);
    requested
}

/// The first-wins reset is independent of the optional mode selector.
pub fn reset_available(status: &crate::DeviceStatus) -> bool {
    status
        .capabilities
        .as_ref()
        .is_some_and(|capabilities| capabilities.modes().any(|mode| mode.id == "first-wins"))
        && status.avr_online
        && !status.programming_latch
}

/// Match the board and Flutter admission: an online AVR is insufficient until
/// it explicitly acknowledges the enabled-box mask for this observation.
pub fn game_press_available(status: &crate::DeviceStatus) -> bool {
    status.avr_online
        && !status.programming_latch
        && status.key_mask_applied == Some(true)
        && status.capabilities.as_ref().is_some_and(|capabilities| {
            capabilities.role == "first-wins" && capabilities.has("game-press")
        })
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BrightnessOwner {
    pub route: crate::Transport,
    pub epoch: u64,
    pub peer: crate::peer_http::Pin,
}

#[derive(Clone, Default)]
struct BrightnessDraft {
    owner: Option<BrightnessOwner>,
    value: Option<u8>,
    last_frame: Option<u64>,
    cancelled_until_release: bool,
}

/// Only a pointer gesture owns a draft. Outside that gesture the selected
/// peer's current observation owns the slider, on USB and network alike.
pub fn brightness(
    ui: &mut egui::Ui,
    owner: Option<BrightnessOwner>,
    observed: Option<u8>,
) -> egui::InnerResponse<Option<u8>> {
    let id = ui.make_persistent_id("brightness-draft");
    let mut draft = ui
        .data(|data| data.get_temp::<BrightnessDraft>(id))
        .unwrap_or_default();
    let observed = observed.filter(|value| *value > 0);
    let frame = ui.ctx().cumulative_frame_nr();
    let pointer_down = ui.input(|input| input.pointer.primary_down());
    let focused = ui.input(|input| input.focused);
    let escape = draft.value.is_some()
        && ui.input_mut(|input| input.consume_key(egui::Modifiers::NONE, egui::Key::Escape));
    let writable = ui.is_enabled() && owner.is_some() && observed.is_some();
    if draft.owner != owner
        || !writable
        || !focused
        || escape
        || draft
            .last_frame
            .is_some_and(|last| frame > last.saturating_add(1))
    {
        draft.value = None;
        draft.cancelled_until_release = pointer_down;
    }
    draft.owner = owner;
    draft.last_frame = Some(frame);
    let blocked = !writable || draft.cancelled_until_release;
    let mut value = draft.value.or(observed).unwrap_or(1);
    let width = ui.available_width().clamp(80.0, 240.0);
    let height = control_density::height(ui);
    let response = if observed.is_some() {
        ui.add_enabled_ui(!blocked, |ui| {
            ui.add_sized([width, height], egui::Slider::new(&mut value, 1..=255))
        })
        .inner
    } else {
        ui.add_sized(
            [width, height],
            egui::Label::new(egui::RichText::new(super::i18n::text("Unknown")).weak()),
        )
    };
    let mut request = None;
    if !blocked {
        if response.drag_stopped() {
            if draft.value.take().is_some() && Some(value) != observed {
                request = Some(value);
            }
        } else if response.changed() {
            if pointer_down {
                draft.value = Some(value);
            } else if Some(value) != observed {
                request = Some(value);
                draft.value = None;
            }
        }
    }
    if !pointer_down {
        // A hidden/detached panel or a cancelled gesture cannot commit later.
        draft.value = None;
        draft.cancelled_until_release = false;
    }
    ui.data_mut(|data| data.insert_temp(id, draft));
    egui::InnerResponse {
        inner: request,
        response,
    }
}

/// Reserve identical geometry for rest/hover/selection; only theme paint varies.
pub fn choice(
    ui: &mut egui::Ui,
    icon: &str,
    label: &str,
    selected: bool,
    enabled: bool,
) -> egui::Response {
    let height = control_density::height(ui).max(dialog::ACTION_HEIGHT);
    let border = ui.visuals().widgets.inactive.bg_stroke.width;
    ui.scope(|ui| {
        let style = ui.style_mut();
        for visuals in [
            &mut style.visuals.widgets.inactive,
            &mut style.visuals.widgets.hovered,
            &mut style.visuals.widgets.active,
            &mut style.visuals.widgets.open,
        ] {
            visuals.bg_stroke.width = border;
            visuals.expansion = 0.0;
        }
        ui.add_enabled(
            enabled,
            egui::Button::new(format!("{icon}  {}", super::i18n::text(label)))
                .selected(selected)
                .min_size(egui::vec2(0.0, height)),
        )
    })
    .inner
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn backend_control_is_observation_only_without_an_explicit_choice() {
        for (pending, active, advertised) in [(false, "gpio-static", true), (true, "gpio-static", true), (false, "rmt-dma", false)] {
            let status: crate::DeviceStatus = serde_json::from_value(serde_json::json!({
                "led_output":{"backend_saved":true,"backend_settings_error":"", "driver":{
                    "backend":active,"requested_backend":"rmt-dma","switch_pending":pending,"switch_error":0,"ready":true}},
                "capabilities":{"board_kind":"table","acknowledged_features":["leds"],"led_output":{
                    "backend":active,"rmt":active=="rmt-dma","dma":active=="rmt-dma",
                    "runtime_switch":advertised,"available_backends":["gpio-static","rmt-dma"]}}
            })).unwrap();
            let context = egui::Context::default();
            let mut frame = context.run_ui(Default::default(), |ui| {
                assert!(backend_picker(ui, &status).is_none());
            });
            frame.textures_delta.clear();
        }
    }
    fn brightness_owner(route: crate::Transport, epoch: u64, boot: char) -> BrightnessOwner {
        BrightnessOwner {
            route,
            epoch,
            peer: crate::peer_http::Pin::from_identity(
                "keycontroller",
                &boot.to_string().repeat(32),
            )
            .unwrap(),
        }
    }
    fn brightness_frame(
        ctx: &egui::Context,
        owner: Option<BrightnessOwner>,
        observed: Option<u8>,
        events: Vec<egui::Event>,
    ) -> (egui::Response, Option<u8>, Option<u8>) {
        let mut result = None;
        let mut output = ctx.run_ui(
            egui::RawInput {
                screen_rect: Some(egui::Rect::from_min_size(
                    egui::Pos2::ZERO,
                    egui::vec2(400.0, 100.0),
                )),
                events,
                ..Default::default()
            },
            |ui| {
                let response = brightness(ui, owner.clone(), observed);
                let draft = ui
                    .data(|data| {
                        data.get_temp::<BrightnessDraft>(ui.make_persistent_id("brightness-draft"))
                    })
                    .unwrap();
                result = Some((response.response, response.inner, draft.value));
            },
        );
        output.textures_delta.clear();
        result.unwrap()
    }
    fn pointer(point: egui::Pos2, pressed: bool) -> Vec<egui::Event> {
        vec![
            egui::Event::PointerMoved(point),
            egui::Event::PointerButton {
                pos: point,
                button: egui::PointerButton::Primary,
                pressed,
                modifiers: egui::Modifiers::NONE,
            },
        ]
    }
    #[test]
    fn brightness_drag_retains_draft_across_observations_and_commits_once_on_both_routes() {
        for route in [crate::Transport::WiFi, crate::Transport::Serial] {
            let ctx = egui::Context::default();
            let owner = Some(brightness_owner(route, 1, 'a'));
            let (response, request, draft) =
                brightness_frame(&ctx, owner.clone(), Some(80), vec![]);
            assert_eq!((request, draft), (None, None));
            let start = egui::pos2(response.rect.left() + 24.0, response.rect.center().y);
            let end = egui::pos2(response.rect.left() + 160.0, response.rect.center().y);
            assert_eq!(
                brightness_frame(&ctx, owner.clone(), Some(80), pointer(start, true)).1,
                None
            );
            let (_, request, draft) = brightness_frame(
                &ctx,
                owner.clone(),
                Some(50),
                vec![egui::Event::PointerMoved(end)],
            );
            assert!(draft.is_some());
            assert_eq!(request, None);
            let desired = draft.unwrap();
            assert_eq!(
                brightness_frame(&ctx, owner.clone(), Some(30), vec![]).2,
                Some(desired)
            );
            let (_, request, draft) =
                brightness_frame(&ctx, owner.clone(), Some(30), pointer(end, false));
            assert_eq!(request, Some(desired));
            assert_eq!(draft, None);
            assert_eq!(
                brightness_frame(&ctx, owner.clone(), Some(desired), vec![]).1,
                None
            );
            // A later peer-observed brightness takes over without a local draft.
            let (_, request, draft) = brightness_frame(&ctx, owner, Some(125), vec![]);
            assert_eq!((request, draft), (None, None));
        }
    }
    #[test]
    fn brightness_cancel_disconnect_and_owner_changes_never_commit_old_gestures() {
        for replacement in [
            None,
            Some(brightness_owner(crate::Transport::Serial, 1, 'a')),
            Some(brightness_owner(crate::Transport::WiFi, 2, 'a')),
            Some(brightness_owner(crate::Transport::WiFi, 1, 'b')),
        ] {
            let ctx = egui::Context::default();
            let owner = Some(brightness_owner(crate::Transport::WiFi, 1, 'a'));
            let (response, _, _) = brightness_frame(&ctx, owner.clone(), Some(80), vec![]);
            let point = egui::pos2(response.rect.left() + 75.0, response.rect.center().y);
            assert!(
                brightness_frame(&ctx, owner, Some(80), pointer(point, true))
                    .2
                    .is_some()
            );
            assert_eq!(
                brightness_frame(&ctx, replacement.clone(), Some(25), vec![]).1,
                None
            );
            let (_, request, draft) =
                brightness_frame(&ctx, replacement, Some(25), pointer(point, false));
            assert_eq!((request, draft), (None, None));
        }
        let ctx = egui::Context::default();
        let owner = Some(brightness_owner(crate::Transport::WiFi, 1, 'a'));
        let (response, _, _) = brightness_frame(&ctx, owner.clone(), Some(80), vec![]);
        let point = egui::pos2(response.rect.left() + 75.0, response.rect.center().y);
        brightness_frame(&ctx, owner.clone(), Some(80), pointer(point, true));
        let (_, request, draft) = brightness_frame(
            &ctx,
            owner.clone(),
            Some(80),
            vec![egui::Event::Key {
                key: egui::Key::Escape,
                physical_key: None,
                pressed: true,
                repeat: false,
                modifiers: egui::Modifiers::NONE,
            }],
        );
        assert_eq!((request, draft), (None, None));
        assert_eq!(
            brightness_frame(&ctx, owner, Some(80), pointer(point, false)).1,
            None
        );
    }
    #[test]
    fn brightness_keyboard_commits_once_and_unknown_observation_remains_non_mutating() {
        let ctx = egui::Context::default();
        let owner = Some(brightness_owner(crate::Transport::Serial, 1, 'a'));
        let (response, _, _) = brightness_frame(&ctx, owner.clone(), Some(80), vec![]);
        response.request_focus();
        let (_, request, draft) = brightness_frame(
            &ctx,
            owner.clone(),
            Some(80),
            vec![egui::Event::Key {
                key: egui::Key::ArrowRight,
                physical_key: None,
                pressed: true,
                repeat: false,
                modifiers: egui::Modifiers::NONE,
            }],
        );
        assert!(request.is_some_and(|value| value > 80));
        assert_eq!(draft, None);
        assert_eq!(
            brightness_frame(&ctx, owner.clone(), request, vec![]).1,
            None
        );
        assert_eq!(brightness_frame(&ctx, owner, None, vec![]).1, None);
    }
    #[test]
    fn game_press_requires_explicit_applied_mask_and_live_admission() {
        let mut status: crate::DeviceStatus = serde_json::from_value(serde_json::json!({
            "avr_online": true,
            "capabilities": {"board_kind":"keycontroller", "role":"first-wins",
                "acknowledged_features":["game-press"]}
        }))
        .unwrap();
        assert_eq!(status.key_mask_applied, None);
        assert!(!game_press_available(&status));
        for applied in [false, true] {
            let mut observed = serde_json::to_value(&status).unwrap();
            observed["key_mask_applied"] = applied.into();
            status = serde_json::from_value(observed).unwrap();
            assert_eq!(status.key_mask_applied, Some(applied));
            assert_eq!(game_press_available(&status), applied);
        }
        status.programming_latch = true;
        assert!(!game_press_available(&status));
        status.programming_latch = false;
        status.avr_online = false;
        assert!(!game_press_available(&status));
        status.avr_online = true;
        status
            .capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .clear();
        assert!(!game_press_available(&status));
        status
            .capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .push("game-press".into());
        status.capabilities.as_mut().unwrap().role = "table-selection".into();
        assert!(!game_press_available(&status));
    }
    fn mode(id: &str) -> Mode {
        Mode {
            id: id.into(),
            label: id.into(),
            ..Default::default()
        }
    }
    #[test]
    fn modes_are_unique_and_hidden_host_only_games_never_create_a_card() {
        assert!(visible_modes([mode("snake"), mode("")]).is_empty());
        assert_eq!(
            visible_modes([mode("cell"), mode("cell"), mode("snake")]).len(),
            1
        );
        assert_eq!(visible_modes([mode("cell"), mode("first-wins")]).len(), 2);
    }
    #[test]
    fn fixed_role_board_allocates_no_mode_card_or_spacer() {
        for modes in [vec![], vec![mode("cell")], vec![mode("first-wins")]] {
            let context = egui::Context::default();
            let mut frame = context.run_ui(Default::default(), |ui| {
                let before = ui.cursor();
                assert!(mode_picker(ui, &modes, None, &[]).is_none());
                assert_eq!(ui.cursor(), before);
            });
            frame.textures_delta.clear();
        }
    }
    #[test]
    fn reset_remains_available_for_a_single_first_wins_mode_only_when_live() {
        let mut status = crate::DeviceStatus {
            avr_online: true,
            capabilities: Some(crate::capabilities::Manifest {
                board_kind: "keycontroller".into(),
                role: "first-wins".into(),
                acknowledged_features: vec!["leds".into(), "inputs".into(), "outputs".into()],
                modes: vec![Mode {
                    id: "first-wins".into(),
                    code: 2,
                    kind: "game".into(),
                    ..Default::default()
                }],
                ..Default::default()
            }),
            ..Default::default()
        };
        assert!(reset_available(&status));
        status.programming_latch = true;
        assert!(!reset_available(&status));
        status.programming_latch = false;
        status.avr_online = false;
        assert!(!reset_available(&status));
        status.avr_online = true;
        status.capabilities.as_mut().unwrap().role = "table-selection".into();
        assert!(!reset_available(&status));
    }
    #[test]
    fn choices_keep_equal_geometry_when_hovered_selected_or_disabled() {
        for dark in [false, true] {
            let context = egui::Context::default();
            context.set_visuals(if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            });
            context.all_styles_mut(|style| {
                control_density::configure(style, crate::config::UiControlDensity::default())
            });
            let mut baseline = None;
            for state in 0..4 {
                let mut frame = context.run_ui(
                    egui::RawInput {
                        events: if state == 1 {
                            vec![egui::Event::PointerMoved(egui::pos2(20.0, 12.0))]
                        } else {
                            vec![]
                        },
                        ..Default::default()
                    },
                    |ui| {
                        let response =
                            choice(ui, icons::SPARKLE, "Rainbow", state == 2, state != 3);
                        if let Some(rect) = baseline {
                            assert_eq!(response.rect, rect);
                        } else {
                            baseline = Some(response.rect);
                        }
                    },
                );
                frame.textures_delta.clear();
            }
        }
    }
}
