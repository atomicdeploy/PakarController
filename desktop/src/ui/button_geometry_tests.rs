//! Interaction geometry regression imported from Pealayer's real
//! `5b3d5c150d714a3f951378867791b06855155094` hardware-action fix.
//! A Button stroke override is applied after egui computes its frame margins;
//! paint-only states must not move either the hit target or the caption ink.
use eframe::egui;

const CAPTIONS: [&str; 5] = ["Ordinary", "Selectable", "Apply", "Primary", "Arrange boxes"];

#[derive(Debug)]
struct FrameGeometry {
    rects: Vec<egui::Rect>,
    text_origins: Vec<(String, egui::Pos2)>,
    ids: Vec<egui::Id>,
    hovered: Vec<bool>,
    pressed: Vec<bool>,
    focused: Vec<bool>,
    enabled: Vec<bool>,
}

fn text_origins(shape: &egui::Shape, result: &mut Vec<(String, egui::Pos2)>) {
    match shape {
        egui::Shape::Text(text) => {
            if CAPTIONS
                .iter()
                .any(|caption| text.galley.job.text.contains(caption))
            {
                result.push((text.galley.job.text.clone(), text.pos));
            }
        }
        egui::Shape::Vec(shapes) => {
            for shape in shapes {
                text_origins(shape, result);
            }
        }
        _ => {}
    }
}

fn draw_frame(
    context: &egui::Context,
    events: Vec<egui::Event>,
    selected: bool,
    enabled: bool,
    menu_scope: bool,
) -> FrameGeometry {
    let mut geometry = FrameGeometry {
        rects: Vec::new(),
        text_origins: Vec::new(),
        ids: Vec::new(),
        hovered: Vec::new(),
        pressed: Vec::new(),
        focused: Vec::new(),
        enabled: Vec::new(),
    };
    let mut output = context.run_ui(
        egui::RawInput {
            screen_rect: Some(egui::Rect::from_min_size(
                egui::Pos2::ZERO,
                egui::vec2(1200.0, 400.0),
            )),
            events,
            ..Default::default()
        },
        |ui| {
            if menu_scope {
                // A real menu scope replaces the palette's 1px frame with a
                // zero-width stroke. This must remain an inherited contract,
                // not be silently replaced by a semantic action's fixed 1px.
                egui::containers::menu::menu_style(ui.style_mut());
                assert_eq!(ui.visuals().widgets.inactive.bg_stroke.width, 0.0);
            }
            ui.horizontal(|ui| {
                for index in 0..CAPTIONS.len() {
                    let response = ui
                        .add_enabled_ui(enabled, |ui| match index {
                            0 | 1 => {
                                let mut button = if menu_scope {
                                    egui::Button::new(
                                        egui::RichText::new(CAPTIONS[index])
                                            .font(egui::TextStyle::Button.resolve(ui.style())),
                                    )
                                    .min_size(egui::vec2(
                                        super::dialog::ACTION_BUTTON_WIDTH,
                                        super::dialog::ACTION_HEIGHT,
                                    ))
                                } else {
                                    egui::Button::new(CAPTIONS[index])
                                };
                                if index == 1 {
                                    button = button.selected(selected);
                                }
                                ui.add(button)
                            }
                            2 => super::dialog::action_button(ui, "+", CAPTIONS[index]),
                            3 => super::dialog::primary_action_button(ui, "+", CAPTIONS[index]),
                            _ => super::key_boxes::arrange_control(ui, selected),
                        })
                        .inner;
                    geometry.rects.push(response.rect);
                    geometry.ids.push(response.id);
                    geometry.hovered.push(response.hovered());
                    geometry.pressed.push(response.is_pointer_button_down_on());
                    geometry.focused.push(response.has_focus());
                    geometry.enabled.push(response.enabled());
                }
            });
        },
    );
    // Consume font uploads: an unconsumed delta must not obscure an assertion
    // with the fixture cleanup panic identified in the donor regression.
    output.textures_delta.clear();
    for shape in &output.shapes {
        text_origins(&shape.shape, &mut geometry.text_origins);
    }
    assert_eq!(geometry.text_origins.len(), CAPTIONS.len());
    if menu_scope {
        let ordinary = geometry.rects[0];
        for action in &geometry.rects[1..4] {
            assert_eq!(
                action.height(),
                ordinary.height(),
                "semantic actions must retain the inherited zero-stroke row height"
            );
            assert_eq!(
                action.center().y,
                ordinary.center().y,
                "semantic actions must stay centered on the ordinary button row"
            );
        }
    }
    geometry
}

fn assert_geometry(actual: &FrameGeometry, baseline: &FrameGeometry, context: &str) {
    assert_eq!(
        actual.rects, baseline.rects,
        "hit slots/sibling layout changed: {context}"
    );
    assert_eq!(
        actual.text_origins, baseline.text_origins,
        "caption ink moved: {context}"
    );
}

#[test]
fn button_hit_slots_and_caption_ink_survive_every_interaction_state() {
    exercise_button_states(false);
}

#[test]
fn semantic_actions_preserve_inherited_zero_stroke_menu_geometry() {
    exercise_button_states(true);
}

fn exercise_button_states(menu_scope: bool) {
    for palette in [
        super::palette::Palette::Studio,
        super::palette::Palette::Native,
    ] {
        for dark in [false, true] {
            for scale in [1.0, 1.25, 1.5, 2.0] {
                for density in crate::config::UiControlDensity::ALL {
                    let context = egui::Context::default();
                    context.set_pixels_per_point(scale);
                    let mut visuals = if dark {
                        egui::Visuals::dark()
                    } else {
                        egui::Visuals::light()
                    };
                    super::palette::apply(&mut visuals, palette);
                    context.set_visuals(visuals);
                    context.all_styles_mut(|style| {
                        super::control_density::configure(style, density);
                        // Distinct font roles expose accidentally losing Button
                        // font inheritance in a colored RichText branch.
                        style
                            .text_styles
                            .insert(egui::TextStyle::Body, egui::FontId::proportional(15.0));
                        style
                            .text_styles
                            .insert(egui::TextStyle::Button, egui::FontId::proportional(13.0));
                    });
                    // egui buttons choose frame styling from their preceding
                    // response, so settle layout before recording the baseline.
                    for _ in 0..3 {
                        draw_frame(&context, vec![], false, true, menu_scope);
                    }
                    let baseline = draw_frame(&context, vec![], false, true, menu_scope);
                    let case = format!(
                        "{palette:?}, dark={dark}, scale={scale}, {density:?}, menu={menu_scope}"
                    );
                    for index in 0..CAPTIONS.len() {
                        let point = baseline.rects[index].center();
                        for selected in [false, true] {
                            for id in &baseline.ids {
                                context.memory_mut(|memory| memory.surrender_focus(*id));
                            }
                            for _ in 0..3 {
                                let frame = draw_frame(
                                    &context,
                                    vec![egui::Event::PointerMoved(point)],
                                    selected,
                                    true,
                                    menu_scope,
                                );
                                assert_geometry(
                                    &frame,
                                    &baseline,
                                    &format!("hover {index}, selected={selected}, {case}"),
                                );
                                assert!(frame.hovered[index]);
                            }
                            for pass in 0..3 {
                                let events = if pass == 0 {
                                    vec![egui::Event::PointerButton {
                                        pos: point,
                                        button: egui::PointerButton::Primary,
                                        pressed: true,
                                        modifiers: egui::Modifiers::NONE,
                                    }]
                                } else {
                                    vec![]
                                };
                                let frame =
                                    draw_frame(&context, events, selected, true, menu_scope);
                                assert_geometry(
                                    &frame,
                                    &baseline,
                                    &format!("pressed {index}, selected={selected}, {case}"),
                                );
                                assert!(frame.pressed[index]);
                            }
                            let frame = draw_frame(
                                &context,
                                vec![
                                    egui::Event::PointerButton {
                                        pos: point,
                                        button: egui::PointerButton::Primary,
                                        pressed: false,
                                        modifiers: egui::Modifiers::NONE,
                                    },
                                    egui::Event::PointerGone,
                                ],
                                selected,
                                true,
                                menu_scope,
                            );
                            assert_geometry(&frame, &baseline, &format!("release {index}, {case}"));
                            context.memory_mut(|memory| memory.request_focus(baseline.ids[index]));
                            for _ in 0..3 {
                                let frame =
                                    draw_frame(&context, vec![], selected, true, menu_scope);
                                assert_geometry(
                                    &frame,
                                    &baseline,
                                    &format!("keyboard focus {index}, {case}"),
                                );
                                assert!(frame.focused[index]);
                                assert!(!frame.hovered[index]);
                            }
                            for _ in 0..3 {
                                let frame =
                                    draw_frame(&context, vec![], selected, false, menu_scope);
                                assert_geometry(
                                    &frame,
                                    &baseline,
                                    &format!("disabled {index}, {case}"),
                                );
                                assert!(frame.enabled.iter().all(|enabled| !enabled));
                            }
                            for id in &baseline.ids {
                                context.memory_mut(|memory| memory.surrender_focus(*id));
                            }
                            for _ in 0..3 {
                                let frame =
                                    draw_frame(&context, vec![], selected, true, menu_scope);
                                assert_geometry(
                                    &frame,
                                    &baseline,
                                    &format!("idle again {index}, {case}"),
                                );
                            }
                        }
                    }
                }
            }
        }
    }
}
