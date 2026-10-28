//! Selected-field edits follow the existing game-presentation ACK/readback model.
use crate::ui::dropdown::DropdownUiExt;
use crate::ui::{dialog, icons};
use eframe::egui;
use serde_json::{Value, json};

pub fn parse(text: &str) -> Result<Value, String> {
    let value: Value = serde_json::from_str(text).map_err(|_| "Invalid appearance JSON")?;
    let fields = value
        .as_object()
        .filter(|v| !v.is_empty())
        .ok_or("Empty appearance")?;
    for (key, value) in fields {
        let valid = match key.as_str() {
            "enabled" | "neighbors" | "invert" | "diagonal" => value.is_boolean(),
            "fire_rotation" => value.as_u64().is_some_and(|n| matches!(n, 0 | 90 | 180 | 270)),
            "fade_ms" => value.as_u64().is_some_and(|n| n <= 10000),
            "pulse_ms" => value.as_u64().is_some_and(|n| (100..=10000).contains(&n)),
            "ring_ms" | "chase_ms" => value.as_u64().is_some_and(|n| (100..=20000).contains(&n)),
            "states" => value.as_object().is_some_and(|states| {
                !states.is_empty()
                    && states.iter().all(|(name, style)| {
                        matches!(name.as_str(), "idle" | "lost" | "won" | "off")
                            && style.as_object().is_some_and(|fields| {
                                !fields.is_empty()
                                    && fields.iter().all(|(k, v)| match k.as_str() {
                                        "rgb" => v.as_u64().is_some_and(|n| {
                                            n <= 0xffffff && (name != "off" || n == 0)
                                        }),
                                        "motion" => v
                                            .as_u64()
                                            .is_some_and(|n| n <= 3 && (name != "off" || n == 0)),
                                        _ => false,
                                    })
                            })
                    })
            }),
            _ => false,
        };
        if !valid {
            return Err(format!("Invalid appearance field: {key}"));
        }
    }
    Ok(value)
}
pub fn matches(patch: &Value, actual: &Value) -> bool {
    match patch.as_object() {
        Some(fields) => actual.is_object() && fields.iter().all(|(k, v)| matches(v, &actual[k])),
        None => patch == actual,
    }
}
pub fn controls(ui: &mut egui::Ui, current: &Value, table: bool) -> Option<Value> {
    ui.scope(|ui| {
        ui.style_mut().wrap_mode = Some(egui::TextWrapMode::Wrap);
        controls_inner(ui, current, table)
    })
    .inner
}

fn controls_inner(ui: &mut egui::Ui, current: &Value, table: bool) -> Option<Value> {
    let mut patch = None;
    // Readback or opening this menu is not an effect-selection intent.
    if let Some(saved) = current["fire_rotation"].as_u64().filter(|n| table && matches!(n, 0 | 90 | 180 | 270)) {
        let mut rotation = saved;
        dialog::setting_row(ui, icons::FIRE, "Fire direction", None, |ui| {
            crate::ui::dropdown::ComboBox::from_id_salt("fire-direction")
                .width(ui.available_width().clamp(100.0, 220.0))
                .selected_text(fire_direction(rotation))
                .show_ui(ui, |ui| {for angle in [0, 90, 180, 270] {
                    ui.dropdown_value(&mut rotation, angle, fire_direction(angle));
                }});
        });
        if rotation != saved { patch = Some(json!({"fire_rotation": rotation})); }
        ui.add_space(8.0);
    }
    let mut enabled = current["enabled"].as_bool()?;
    if ui
        .checkbox(&mut enabled, "Extended state effects")
        .changed()
    {
        patch = Some(json!({"enabled":enabled}));
    }
    ui.add_space(8.0);
    for (state, label) in [
        ("idle", "Idle"),
        ("won", "Won"),
        ("lost", "Lost"),
        ("off", "Off"),
    ] {
        let Some(rgb) = current["states"][state]["rgb"]
            .as_u64()
            .filter(|rgb| *rgb <= 0xffffff)
        else {
            continue;
        };
        let saved = [(rgb >> 16) as u8, (rgb >> 8) as u8, rgb as u8];
        let draft_id = ui.id().with(("state-color", state, rgb));
        let mut color = ui
            .data(|d| d.get_temp::<[u8; 3]>(draft_id))
            .unwrap_or(saved);
        // Match Flutter's individual state cards; the shared Pealayer density
        // and inside-stroke rules keep hover/focus from moving the row.
        egui::Frame::new().fill(ui.visuals().faint_bg_color)
            .stroke(ui.visuals().widgets.noninteractive.bg_stroke)
            .corner_radius(7.0).inner_margin(egui::Margin::same(8)).show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.horizontal_wrapped(|ui| {
                ui.strong(crate::ui::i18n::text(label));
                ui.label(egui::RichText::new(format!("#{rgb:06X}")).monospace().small().weak());
            });
            ui.add_space(4.0);
            ui.horizontal_wrapped(|ui| {
                ui.add_enabled_ui(state != "off", |ui| {
                    if ui.color_edit_button_srgb(&mut color).changed() {
                        ui.data_mut(|d|d.insert_temp(draft_id,color));
                    }
                });
                if state != "off" {
                    ui.add_enabled_ui(color!=saved, |ui| {
                        if dialog::action_button(ui, icons::CHECK, "Apply color").clicked() {
                            patch=Some(json!({"states":{state:{"rgb":((color[0] as u32)<<16)|((color[1] as u32)<<8)|color[2] as u32}}}));
                            ui.data_mut(|d|d.remove::<[u8;3]>(draft_id));
                        }
                    });
                }
            });
                // The motion selector is a complete control, not a wrapping
                // text fragment. Keep its own row instead of letting a scoped
                // combo exceed the remaining width after the color actions.
                if state != "off" && (table || state!="idle") {
                    ui.add_space(4.0);
                    if let Some(original)=current["states"][state]["motion"].as_u64().filter(|motion| *motion <= 3) {
                        let mut motion=original;
                        crate::ui::dropdown::ComboBox::from_id_salt(("state-motion",state))
                            .width(ui.available_width().clamp(100.0, 170.0))
                            .selected_text(["Solid","Pulse","Circular chase","Chase"][motion as usize])
                            .show_ui(ui,|ui| { for (i,name) in ["Solid","Pulse","Circular chase","Chase"].iter().enumerate(){ui.dropdown_value(&mut motion,i as u64,*name);} });
                        if motion!=original {patch=Some(json!({"states":{state:{"motion":motion}}}));}
                    }
                }
        });
        ui.add_space(6.0);
    }
    ui.add_space(4.0);
    for (field, label, min, max) in [
        ("fade_ms", "Transition (ms)", 0, 10000),
        ("pulse_ms", "Pulse period (ms)", 100, 10000),
        (
            "ring_ms",
            "Circular period (ms)",
            if table { 100 } else { 400 },
            if table { 20000 } else { 3000 },
        ),
        ("chase_ms", "Chase period (ms)", 100, 20000),
    ] {
        let Some(saved) = current[field].as_u64() else {
            continue;
        };
        let id = ui.id().with(("state-period", field, saved));
        let mut value = ui.data(|d| d.get_temp::<u64>(id)).unwrap_or(saved);
        dialog::setting_row(ui, icons::CLOCK, label, None, |ui| {
            ui.horizontal_wrapped(|ui| {
                if ui
                    .add_sized(
                        [64.0, crate::ui::control_density::height(ui)],
                        egui::DragValue::new(&mut value).range(min..=max),
                    )
                    .changed()
                {
                    ui.data_mut(|d| d.insert_temp(id, value));
                }
                ui.add_enabled_ui(value != saved, |ui| {
                    if dialog::action_button(ui, icons::CHECK, "Apply").clicked() {
                        patch = Some(json!({field:value}));
                        ui.data_mut(|d| d.remove::<u64>(id));
                    }
                });
            });
        });
        ui.add_space(6.0);
    }
    ui.separator();
    for (field, label) in [
        ("neighbors", "Chase affects adjacent pixels"),
        ("invert", "Bright chasing dot"),
        ("diagonal", "Diagonal lost-state ripple"),
    ] {
        if field == "diagonal" && !table {
            continue;
        }
        if let Some(mut v) = current[field].as_bool() {
            if ui.checkbox(&mut v, label).changed() {
                patch = Some(json!({field:v}));
            }
        }
    }
    patch
}
fn fire_direction(rotation: u64) -> &'static str {
    match rotation {90 => "90° · Right", 180 => "180° · Down", 270 => "270° · Left", _ => "0° · Up"}
}
pub fn preview_controls(ui: &mut egui::Ui, ids: &str) -> Option<Vec<(String, String)>> {
    ui.scope(|ui| {
        ui.style_mut().wrap_mode = Some(egui::TextWrapMode::Wrap);
        preview_controls_inner(ui, ids)
    })
    .inner
}

fn preview_controls_inner(ui: &mut egui::Ui, ids: &str) -> Option<Vec<(String, String)>> {
    let id = ui.id().with("temporary-state-color");
    let mut rgb = ui
        .data(|d| d.get_temp::<[u8; 3]>(id))
        .unwrap_or([0, 255, 0]);
    let mut request = None;
    ui.horizontal(|ui| {
        if ui.color_edit_button_srgb(&mut rgb).changed() {
            ui.data_mut(|d| d.insert_temp(id, rgb));
        }
        ui.label(
            egui::RichText::new(format!("#{:02X}{:02X}{:02X}", rgb[0], rgb[1], rgb[2]))
                .monospace()
                .weak(),
        );
    });
    crate::ui::panel_details::action_row(
        ui,
        &[
            (icons::EYE, "Preview · 5 seconds"),
            (icons::STOP, "Stop preview"),
        ],
        |ui| {
            ui.add_enabled_ui(!ids.is_empty(), |ui| {
                if dialog::primary_action_button(ui, icons::EYE, "Preview · 5 seconds").clicked() {
                    let color = ((rgb[0] as u32) << 16) | ((rgb[1] as u32) << 8) | rgb[2] as u32;
                    request = Some(vec![
                        ("ids".into(), ids.into()),
                        ("rgb".into(), color.to_string()),
                        ("duration_ms".into(), "5000".into()),
                    ]);
                }
            });
            if dialog::action_button(ui, icons::STOP, "Stop preview").clicked() {
                request = Some(vec![("cancel".into(), "true".into())]);
            }
        },
    );
    request
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn unavailable_connection_disables_appearance_and_preview_without_hiding_them() {
        for writable in [false, true] {
            for target in [
                "Extended state effects",
                "Preview · 5 seconds",
                "Stop preview",
            ] {
                let context = egui::Context::default();
                let mut patches = Vec::new();
                let mut requests = Vec::new();
                let current = json!({"enabled":true,"states":{}});
                let mut frame = |events| {
                    context.run_ui(
                        egui::RawInput {
                            screen_rect: Some(egui::Rect::from_min_size(
                                egui::Pos2::ZERO,
                                egui::vec2(400.0, 600.0),
                            )),
                            events,
                            ..Default::default()
                        },
                        |ui| {
                            ui.add_enabled_ui(writable, |ui| {
                                if let Some(patch) = controls(ui, &current, true) {
                                    patches.push(patch);
                                }
                                if let Some(request) = preview_controls(ui, "1-4") {
                                    requests.push(request);
                                }
                            });
                        },
                    )
                };
                let mut output = frame(vec![]);
                let point = output
                    .shapes
                    .iter()
                    .find_map(|shape| match &shape.shape {
                        egui::Shape::Text(text) if text.galley.text().ends_with(target) => {
                            Some(text.pos + text.galley.size() * 0.5)
                        }
                        _ => None,
                    })
                    .expect("observed control remains visible on either transport");
                output.textures_delta.clear();
                for pressed in [true, false] {
                    let mut output = frame(vec![
                        egui::Event::PointerMoved(point),
                        egui::Event::PointerButton {
                            pos: point,
                            button: egui::PointerButton::Primary,
                            pressed,
                            modifiers: egui::Modifiers::NONE,
                        },
                    ]);
                    output.textures_delta.clear();
                }
                drop(frame);
                if !writable {
                    assert!(patches.is_empty(), "unavailable control emitted a patch");
                    assert!(requests.is_empty(), "unavailable control emitted a preview");
                } else if target == "Extended state effects" {
                    assert_eq!(patches, [json!({"enabled":false})]);
                    assert!(requests.is_empty());
                } else if target == "Stop preview" {
                    assert!(patches.is_empty());
                    assert_eq!(requests, [vec![("cancel".into(), "true".into())]]);
                } else {
                    assert!(patches.is_empty());
                    assert_eq!(
                        requests,
                        [vec![
                            ("ids".into(), "1-4".into()),
                            ("rgb".into(), "65280".into()),
                            ("duration_ms".into(), "5000".into())
                        ]]
                    );
                }
            }
        }
    }
    #[test]
    fn appearance_cards_wrap_within_docked_panels_without_sending_on_render() {
        let current = json!({"enabled":true,"states":{
            "idle":{"rgb":16777215,"motion":0},"won":{"rgb":65280,"motion":2},
            "lost":{"rgb":16711680,"motion":1},"off":{"rgb":0,"motion":0}},
            "fade_ms":250,"pulse_ms":900,"ring_ms":900,"chase_ms":1800,
            "neighbors":true,"invert":false,"diagonal":true,"fire_rotation":180});
        for width in [220.0, 280.0, 380.0, 640.0] {
            for table in [false, true] {
                let context = egui::Context::default();
                context.all_styles_mut(|style| {
                    crate::ui::control_density::configure(
                        style,
                        crate::config::UiControlDensity::Compact,
                    )
                });
                for _ in 0..2 {
                    let mut output = context.run_ui(
                        egui::RawInput {
                            screen_rect: Some(egui::Rect::from_min_size(
                                egui::Pos2::ZERO,
                                egui::vec2(width + 40.0, 1600.0),
                            )),
                            ..Default::default()
                        },
                        |ui| {
                            ui.set_width(width);
                            let left = ui.min_rect().left();
                            assert!(controls(ui, &current, table).is_none());
                            assert!(preview_controls(ui, "1-4").is_none());
                            assert!(
                                ui.min_rect().right() <= left + width + 1.0,
                                "appearance overflow at {width}: {:?}",
                                ui.min_rect()
                            );
                        },
                    );
                    output.textures_delta.clear();
                }
            }
        }
    }
    #[test]
    fn validates_and_compares_only_requested_fields() {
        for angle in [0, 90, 180, 270] {
            let patch = parse(&format!(r#"{{"fire_rotation":{angle}}}"#)).unwrap();
            assert!(matches(&patch, &json!({"fire_rotation":angle,"enabled":false})));
            assert!(!matches(&patch, &json!({"fire_rotation":(angle+90)%360})));
        }
        let p = parse(r#"{"states":{"lost":{"rgb":255}},"enabled":true}"#).unwrap();
        assert!(matches(
            &p,
            &json!({"states":{"lost":{"rgb":255,"motion":2}},"enabled":true,"ring_ms":900})
        ));
        assert!(!matches(
            &p,
            &json!({"states":{"lost":{"rgb":16711680}},"enabled":true})
        ));
        for invalid in [
            r#"{"states":{"off":{"rgb":1}}}"#,
            r#"{"ring_ms":0}"#,
            r#"{"enabled":1}"#,
            r#"{"states":{"lost":{"motion":4}}}"#,
            r#"{"fire_rotation":45}"#,
            r#"{"fire_rotation":360}"#,
            r#"{"fire_rotation":"90"}"#,
        ] {
            assert!(parse(invalid).is_err());
        }
    }
}
