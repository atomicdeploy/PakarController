//! First-press workspace. Layout gestures never issue game commands.
//!
//! Grab-point capture and fixed source geometry adapt Pealayer's actual
//! `remember_drag_offset_on_press` / `drag_translation` in src/ui/layout.rs:
//! e47f7739 (stable source rect), d71c88d6 (capture before drag promotion).
//! This is a scoped reuse, not a claim of full Designer/timeline parity.
use super::i18n::text;
use eframe::egui::{self, Color32, Pos2, Rect, Stroke, Vec2};

const GRID: f32 = 16.0;
const CARD: Vec2 = egui::vec2(128.0, 112.0);
const GAP: f32 = 8.0;
const MIN_WIDTH: f32 = 448.0;
const HEIGHT: f32 = 416.0;

#[derive(Clone)]
pub struct BoxView {
    pub enabled: bool,
    pub pressed: bool,
    pub winner: bool,
    pub round_active: bool,
    pub color: Color32,
    /// Actual acknowledged output only; absent telemetry is not synthesized.
    pub groups: [Option<Vec<Color32>>; 2],
}

impl BoxView {
    fn status(&self) -> &'static str {
        if !self.enabled {
            "Disabled"
        } else if self.pressed {
            "Pressed"
        } else if self.winner {
            "Winner"
        } else if self.round_active {
            "Lost"
        } else {
            "Ready"
        }
    }
}

#[derive(Default)]
pub struct Output {
    pub trigger: Option<u8>,
    pub layout_changed: bool,
}

#[derive(Clone, Copy)]
struct Drag {
    index: usize,
    grab_offset: Vec2,
    last_frame: u64,
}

fn snap(value: f32, maximum: f32) -> f32 {
    (value / GRID).round().clamp(0.0, (maximum / GRID).floor()) * GRID
}

fn normalized(min: Pos2, inner: Rect) -> [f32; 2] {
    let travel = (inner.size() - CARD).max(Vec2::splat(1.0));
    [
        (min.x - inner.left()) / travel.x,
        (min.y - inner.top()) / travel.y,
    ]
}

fn snapped_min(min: Pos2, inner: Rect) -> Pos2 {
    let travel = (inner.size() - CARD).max(Vec2::ZERO);
    inner.min
        + egui::vec2(
            snap(min.x - inner.left(), travel.x),
            snap(min.y - inner.top(), travel.y),
        )
}

fn can_place(candidate: Rect, occupied: impl Iterator<Item = Rect>) -> bool {
    occupied.into_iter().all(|other| {
        !candidate
            .expand(GAP / 2.0)
            .intersects(other.expand(GAP / 2.0))
    })
}

/// Resolve old free-placement layouts without overlapping hit targets. This
/// display projection does not overwrite a board's saved layout on resize.
fn layout_rects(inner: Rect, positions: &[[f32; 2]; 9]) -> [Rect; 9] {
    let travel = (inner.size() - CARD).max(Vec2::ZERO);
    let mut placed = [Rect::NOTHING; 9];
    for (index, position) in positions.iter().enumerate() {
        let x = if position[0].is_finite() {
            position[0].clamp(0.0, 1.0)
        } else {
            (index % 3) as f32 / 2.0
        };
        let y = if position[1].is_finite() {
            position[1].clamp(0.0, 1.0)
        } else {
            (index / 3) as f32 / 2.0
        };
        let wanted = snapped_min(inner.min + egui::vec2(x * travel.x, y * travel.y), inner);
        let candidate = Rect::from_min_size(wanted, CARD);
        if can_place(candidate, placed[..index].iter().copied()) {
            placed[index] = candidate;
            continue;
        }
        let mut nearest: Option<(f32, Rect)> = None;
        for row in 0..=(travel.y / GRID) as usize {
            for column in 0..=(travel.x / GRID) as usize {
                let min = inner.min + egui::vec2(column as f32 * GRID, row as f32 * GRID);
                let rect = Rect::from_min_size(min, CARD);
                let distance = wanted.distance_sq(min);
                if can_place(rect, placed[..index].iter().copied())
                    && nearest.is_none_or(|(best, _)| distance < best)
                {
                    nearest = Some((distance, rect));
                }
            }
        }
        if let Some((_, rect)) = nearest {
            placed[index] = rect;
        } else {
            // An arbitrary imported layout can fragment all available room.
            // A deterministic three-column projection keeps every box usable.
            return std::array::from_fn(|i| {
                Rect::from_min_size(
                    inner.min + egui::vec2((i % 3) as f32 * 144.0, (i / 3) as f32 * 128.0),
                    CARD,
                )
            });
        }
    }
    placed
}

fn commit_layout(
    positions: &mut [[f32; 2]; 9],
    rects: &[Rect; 9],
    inner: Rect,
    index: usize,
    target: Rect,
) {
    // Legacy coordinates may have needed collision-safe projection. Commit the
    // complete visible layout once, so moving one card cannot rearrange peers
    // when their original overlapping coordinates are projected next frame.
    *positions = std::array::from_fn(|i| normalized(rects[i].min, inner));
    positions[index] = normalized(target.min, inner);
}

fn pointer_intensity(dot: Pos2, pointer: Option<Pos2>) -> f32 {
    let Some(pointer) = pointer else {
        return 0.0;
    };
    let proximity = (1.0 - dot.distance(pointer) / 144.0).clamp(0.0, 1.0);
    proximity * proximity
}

fn mix(base: Color32, tint: Color32, amount: f32) -> Color32 {
    let t = amount.clamp(0.0, 1.0);
    Color32::from_rgb(
        (f32::from(base.r()) * (1.0 - t) + f32::from(tint.r()) * t).round() as u8,
        (f32::from(base.g()) * (1.0 - t) + f32::from(tint.g()) * t).round() as u8,
        (f32::from(base.b()) * (1.0 - t) + f32::from(tint.b()) * t).round() as u8,
    )
}

fn paint_card(
    ui: &egui::Ui,
    rect: Rect,
    key: u8,
    model: &BoxView,
    hover: f32,
    focus: bool,
    editing: bool,
    dragging: bool,
) {
    let painter = ui.painter();
    let accent = ui.visuals().selection.bg_fill;
    let surface = ui.visuals().widgets.noninteractive.bg_fill;
    let semantic = if model.enabled {
        model.color
    } else {
        ui.visuals().weak_text_color()
    };
    let fill = mix(
        surface,
        semantic,
        if model.enabled {
            0.08 + hover * 0.05
        } else {
            0.015
        },
    );
    let edge = if dragging || focus {
        accent
    } else {
        mix(
            surface,
            semantic,
            if model.enabled {
                0.22 + hover * 0.20
            } else {
                0.09
            },
        )
    };
    // Border always occupies the same inside pixel. Hover/focus do not alter
    // allocation, padding, typography, or card geometry.
    painter.rect_filled(rect, 14.0, fill);
    painter.rect_stroke(rect, 14.0, Stroke::new(1.0, edge), egui::StrokeKind::Inside);
    let opacity = if model.enabled { 1.0 } else { 0.42 };
    let ink = ui.visuals().text_color().gamma_multiply(opacity);
    let weak = ui.visuals().weak_text_color().gamma_multiply(opacity);
    painter.text(
        rect.min + egui::vec2(12.0, 12.0),
        egui::Align2::LEFT_TOP,
        format!("{} {key}", text("Box")),
        egui::FontId::proportional(15.0),
        ink,
    );
    painter.text(
        rect.min + egui::vec2(12.0, 33.0),
        egui::Align2::LEFT_TOP,
        text(model.status()),
        egui::FontId::proportional(11.0),
        weak,
    );
    let glyph = if editing {
        super::icons::DOTS_SIX_VERTICAL
    } else if model.winner {
        super::icons::CHECK
    } else {
        super::icons::HAND_TAP
    };
    painter.text(
        rect.right_top() + egui::vec2(-12.0, 13.0),
        egui::Align2::RIGHT_TOP,
        glyph,
        egui::FontId::proportional(16.0),
        if model.winner && model.enabled {
            semantic
        } else {
            weak
        },
    );
    let row_width = rect.width() - 24.0;
    for group in 0..2 {
        let y = rect.top() + 65.0 + group as f32 * 24.0;
        painter.text(
            egui::pos2(rect.left() + 12.0, y - 12.0),
            egui::Align2::LEFT_TOP,
            text(if group == 0 { "Button" } else { "Indicator" }),
            egui::FontId::proportional(9.0),
            weak,
        );
        let colors = model.groups[group].as_ref();
        let count = colors.map_or(1, |values| values.len().max(1));
        let gap = if count > 24 { 0.0 } else { 2.0 };
        let segment = (row_width - (count-1) as f32 * gap) / count as f32;
        for pixel in 0..count {
            let pixel_rect = Rect::from_min_size(
                egui::pos2(rect.left() + 12.0 + pixel as f32 * (segment + gap), y),
                egui::vec2(segment, 8.0),
            );
            let color = colors.and_then(|values| values.get(pixel)).copied();
            let fill = color
                .map(|color| {
                    if model.enabled {
                        color
                    } else {
                        mix(surface, color, 0.18)
                    }
                })
                .unwrap_or_else(|| mix(surface, weak, 0.12));
            painter.rect_filled(pixel_rect, 2.0, fill);
        }
    }
}

pub fn show(
    ui: &mut egui::Ui,
    board_id: &str,
    positions: &mut [[f32; 2]; 9],
    models: &[BoxView; 9],
    editing: bool,
    can_trigger: bool,
    animate: bool,
) -> Output {
    // A horizontal ScrollArea gives its child unbounded width; size against the
    // viewport before entering it, never against the content Ui.
    let canvas_width = ui.available_width().max(MIN_WIDTH);
    egui::ScrollArea::horizontal()
        .id_salt(("key-box-scroll", board_id))
        .show(ui, |ui| {
            let (canvas, _) =
                ui.allocate_exact_size(egui::vec2(canvas_width, HEIGHT), egui::Sense::hover());
            ui.set_clip_rect(ui.clip_rect().intersect(canvas));
            let inner = canvas.shrink(16.0);
            let rects = layout_rects(inner, positions);
            let state_id = ui.make_persistent_id(("key-box-drag", board_id));
            let mut drag = ui.data_mut(|data| data.get_temp::<Drag>(state_id));
            let mut out = Output::default();
            let current_frame = ui.ctx().cumulative_frame_nr();
            let released =
                ui.input(|input| input.pointer.button_released(egui::PointerButton::Primary));
            let pointer = ui
                .ctx()
                .pointer_hover_pos()
                .or_else(|| released.then(|| ui.ctx().pointer_interact_pos()).flatten());
            if !editing
                || !ui.input(|input| input.focused)
                || (drag.is_some()
                    && ui.input_mut(|input| {
                        input.consume_key(egui::Modifiers::NONE, egui::Key::Escape)
                    }))
                || pointer.is_none()
                || drag.is_some_and(|active| current_frame > active.last_frame.saturating_add(1))
            {
                drag = None;
            }
            let accent = ui.visuals().selection.bg_fill;
            let base = ui.visuals().extreme_bg_color;
            ui.painter().rect_filled(canvas, 16.0, base);
            ui.painter().rect_stroke(
                canvas,
                16.0,
                Stroke::new(1.0, mix(base, accent, if editing { 0.3 } else { 0.12 })),
                egui::StrokeKind::Inside,
            );
            let grid_pointer = pointer.filter(|p| canvas.contains(*p));
            for row in 0..=(inner.height() / GRID) as usize {
                for column in 0..=(inner.width() / GRID) as usize {
                    let dot = inner.min + egui::vec2(column as f32 * GRID, row as f32 * GRID);
                    let intensity = pointer_intensity(dot, grid_pointer);
                    let drag_accent = if drag.is_some() { 0.12 } else { 0.0 };
                    let color = mix(
                        base,
                        if editing || drag.is_some() {
                            accent
                        } else {
                            ui.visuals().weak_text_color()
                        },
                        0.16 + intensity * (0.40 + drag_accent),
                    );
                    ui.painter()
                        .circle_filled(dot, 1.0 + intensity * 0.5, color);
                }
            }
            let mut hover = [0.0; 9];
            let mut focus = [false; 9];
            for index in 0..9 {
                let key = (index + 1) as u8;
                let id = ui.make_persistent_id(("key-box", board_id, key));
                let offset_id = id.with("pointer-offset");
                let rect = rects[index];
                // Imported press-origin capture, not current pointer delta. Preserve
                // the source rect until drop so the card never jumps under the cursor.
                if editing && drag.is_none() {
                    let press = ui.input(|input| {
                        input
                            .pointer
                            .primary_down()
                            .then(|| input.pointer.press_origin())
                            .flatten()
                            .filter(|p| rect.contains(*p))
                    });
                    if let Some(press) = press {
                        ui.data_mut(|data| data.insert_temp(offset_id, press - rect.min));
                    }
                }
                let response = ui.interact(
                    rect,
                    id,
                    if editing {
                        egui::Sense::click_and_drag()
                    } else {
                        egui::Sense::click()
                    },
                );
                response.widget_info(|| {
                    egui::WidgetInfo::labeled(
                        egui::WidgetType::Button,
                        editing || (can_trigger && models[index].enabled),
                        format!(
                            "{} {key}, {}, {} {}, {} {}",
                            text("Box"),
                            text(models[index].status()),
                            text("Column"),
                            ((rect.left() - inner.left()) / GRID).round() as usize + 1,
                            text("Row"),
                            ((rect.top() - inner.top()) / GRID).round() as usize + 1
                        ),
                    )
                });
                focus[index] = response.has_focus();
                hover[index] = if animate {
                    ui.ctx()
                        .animate_bool_with_time(id.with("hover"), response.hovered(), 0.12)
                } else {
                    if response.hovered() { 1.0 } else { 0.0 }
                };
                if editing {
                    if response.clicked() {
                        response.request_focus();
                    }
                    if response.drag_started_by(egui::PointerButton::Primary) {
                        let grab_offset = ui
                            .data_mut(|data| data.get_temp::<Vec2>(offset_id))
                            .unwrap_or(CARD * 0.5);
                        drag = Some(Drag {
                            index,
                            grab_offset,
                            last_frame: current_frame,
                        });
                        response.request_focus();
                    }
                    if response.has_focus() && drag.is_none() {
                        let direction = ui.input_mut(|input| {
                            egui::vec2(
                                u8::from(
                                    input.consume_key(egui::Modifiers::NONE, egui::Key::ArrowRight),
                                ) as f32
                                    - u8::from(
                                        input.consume_key(
                                            egui::Modifiers::NONE,
                                            egui::Key::ArrowLeft,
                                        ),
                                    ) as f32,
                                u8::from(
                                    input.consume_key(egui::Modifiers::NONE, egui::Key::ArrowDown),
                                ) as f32
                                    - u8::from(
                                        input
                                            .consume_key(egui::Modifiers::NONE, egui::Key::ArrowUp),
                                    ) as f32,
                            )
                        });
                        if direction != Vec2::ZERO {
                            let candidate = Rect::from_min_size(
                                snapped_min(rect.min + direction * GRID, inner),
                                CARD,
                            );
                            if candidate != rect
                                && can_place(
                                    candidate,
                                    rects
                                        .iter()
                                        .enumerate()
                                        .filter(|(other, _)| *other != index)
                                        .map(|(_, rect)| *rect),
                                )
                            {
                                commit_layout(positions, &rects, inner, index, candidate);
                                out.layout_changed = true;
                            }
                        }
                    }
                } else if response.clicked() && can_trigger && models[index].enabled {
                    out.trigger = Some(key);
                }
                response
                    .on_hover_cursor(if editing {
                        if drag.is_some_and(|value| value.index == index) {
                            egui::CursorIcon::Grabbing
                        } else {
                            egui::CursorIcon::Grab
                        }
                    } else if can_trigger && models[index].enabled {
                        egui::CursorIcon::PointingHand
                    } else {
                        egui::CursorIcon::Default
                    })
                    .on_hover_text(if editing {
                        format!(
                            "{} {key} · {}",
                            text("Box"),
                            text("Drag or use arrow keys; Esc cancels")
                        )
                    } else if !models[index].enabled {
                        format!("{} {key} · {}", text("Box"), text("Disabled"))
                    } else if models[index].groups.iter().all(Option::is_none) {
                        format!(
                            "{} {key} · {}",
                            text("Box"),
                            text("Live pixel preview not received")
                        )
                    } else {
                        format!(
                            "{} {key} · {} / {}",
                            text("Box"),
                            text("Button"),
                            text("Indicator")
                        )
                    });
            }
            for index in 0..9 {
                if drag.is_some_and(|value| value.index == index) {
                    ui.painter().rect_stroke(
                        rects[index],
                        14.0,
                        Stroke::new(1.0, mix(base, accent, 0.25)),
                        egui::StrokeKind::Inside,
                    );
                } else {
                    paint_card(
                        ui,
                        rects[index],
                        (index + 1) as u8,
                        &models[index],
                        hover[index],
                        focus[index],
                        editing,
                        false,
                    );
                }
            }
            if let (Some(active), Some(pointer)) = (drag, pointer) {
                let raw_min = pointer - active.grab_offset;
                let target = Rect::from_min_size(snapped_min(raw_min, inner), CARD);
                let valid = canvas.contains(pointer)
                    && can_place(
                        target,
                        rects
                            .iter()
                            .enumerate()
                            .filter(|(index, _)| *index != active.index)
                            .map(|(_, rect)| *rect),
                    );
                let signal = if valid {
                    accent
                } else {
                    ui.visuals().error_fg_color
                };
                ui.painter()
                    .rect_filled(target.expand(3.0), 16.0, signal.gamma_multiply(0.08));
                ui.painter().rect_stroke(
                    target,
                    14.0,
                    Stroke::new(1.0, signal.gamma_multiply(0.7)),
                    egui::StrokeKind::Inside,
                );
                let preview = Rect::from_min_size(raw_min, CARD);
                paint_card(
                    ui,
                    preview,
                    (active.index + 1) as u8,
                    &models[active.index],
                    1.0,
                    false,
                    true,
                    true,
                );
                if ui.input(|input| input.pointer.button_released(egui::PointerButton::Primary)) {
                    if valid {
                        commit_layout(positions, &rects, inner, active.index, target);
                        out.layout_changed = true;
                    }
                    drag = None;
                } else if !ui.input(|input| input.pointer.primary_down()) {
                    drag = None;
                }
                ui.ctx().request_repaint();
            }
            ui.data_mut(|data| {
                if let Some(mut drag) = drag {
                    drag.last_frame = current_frame;
                    data.insert_temp(state_id, drag);
                } else {
                    data.remove::<Drag>(state_id);
                }
            });
            out
        })
        .inner
}

#[cfg(test)]
mod tests {
    use super::*;

    fn inner(width: f32) -> Rect {
        Rect::from_min_size(Pos2::ZERO, egui::vec2(width, HEIGHT)).shrink(16.0)
    }

    #[test]
    fn default_and_colliding_imports_always_have_grid_aligned_disjoint_cards() {
        for width in [MIN_WIDTH, 720.0, 1200.0] {
            for positions in [
                crate::native_workspace::default_key_positions(),
                [[0.5, 0.5]; 9],
            ] {
                let area = inner(width);
                let rects = layout_rects(area, &positions);
                for (i, rect) in rects.iter().enumerate() {
                    assert!(area.contains_rect(*rect));
                    assert_eq!(rect.size(), CARD);
                    assert_eq!((rect.min.x - area.min.x) % GRID, 0.0);
                    assert_eq!((rect.min.y - area.min.y) % GRID, 0.0);
                    assert!(can_place(
                        *rect,
                        rects
                            .iter()
                            .enumerate()
                            .filter(|(j, _)| *j != i)
                            .map(|(_, r)| *r)
                    ));
                }
            }
        }
    }

    #[test]
    fn grab_offset_is_preserved_and_drop_projection_roundtrips() {
        let area = inner(720.0);
        let source = area.min + egui::vec2(160.0, 128.0);
        let grab = egui::vec2(12.0, 21.0);
        let pointer = source + grab + egui::vec2(32.0, 16.0);
        let target = snapped_min(pointer - grab, area);
        assert_eq!(target, source + egui::vec2(32.0, 16.0));
        let saved = normalized(target, area);
        let travel = area.size() - CARD;
        assert_eq!(
            snapped_min(
                area.min + egui::vec2(saved[0] * travel.x, saved[1] * travel.y),
                area
            ),
            target
        );
    }

    #[test]
    fn collision_rejects_overlap_and_preserves_a_real_gutter() {
        let fixed = Rect::from_min_size(Pos2::ZERO, CARD);
        assert!(!can_place(
            fixed.translate(egui::vec2(16.0, 0.0)),
            [fixed].into_iter()
        ));
        assert!(can_place(
            fixed.translate(egui::vec2(144.0, 0.0)),
            [fixed].into_iter()
        ));
    }

    #[test]
    fn grid_light_falls_off_smoothly_and_never_exceeds_bounds() {
        let center = Pos2::ZERO;
        assert_eq!(pointer_intensity(center, Some(center)), 1.0);
        assert_eq!(pointer_intensity(center, None), 0.0);
        assert!(
            pointer_intensity(center, Some(egui::pos2(32.0, 0.0)))
                > pointer_intensity(center, Some(egui::pos2(96.0, 0.0)))
        );
        assert_eq!(pointer_intensity(center, Some(egui::pos2(144.0, 0.0))), 0.0);
    }

    fn models(enabled: bool) -> [BoxView; 9] {
        std::array::from_fn(|_| BoxView {
            enabled,
            pressed: false,
            winner: false,
            round_active: false,
            color: Color32::WHITE,
            groups: [None, None],
        })
    }

    fn frame(
        context: &egui::Context,
        positions: &mut [[f32; 2]; 9],
        models: &[BoxView; 9],
        editing: bool,
        allowed: bool,
        events: Vec<egui::Event>,
    ) -> (Output, Rect) {
        let mut result = Output::default();
        let mut bounds = Rect::NOTHING;
        let mut rendered = context.run_ui(
            egui::RawInput {
                screen_rect: Some(Rect::from_min_size(Pos2::ZERO, egui::vec2(800.0, 600.0))),
                events,
                focused: true,
                ..Default::default()
            },
            |ui| {
                ui.set_width(720.0);
                let origin = ui.cursor().min;
                result = show(
                    ui,
                    "fixture-board",
                    positions,
                    models,
                    editing,
                    allowed,
                    false,
                );
                bounds = Rect::from_min_size(origin, egui::vec2(720.0, HEIGHT));
            },
        );
        rendered.textures_delta.clear();
        (result, bounds)
    }

    fn pointer_button(pos: Pos2, pressed: bool) -> egui::Event {
        egui::Event::PointerButton {
            pos,
            button: egui::PointerButton::Primary,
            pressed,
            modifiers: Default::default(),
        }
    }

    #[test]
    fn clicks_trigger_only_enabled_authorized_game_cards_never_editing() {
        for (editing, allowed, enabled, expected) in [
            (false, true, true, Some(1)),
            (true, true, true, None),
            (false, false, true, None),
            (false, true, false, None),
        ] {
            let context = egui::Context::default();
            let mut positions = crate::native_workspace::default_key_positions();
            let initial = positions;
            let models = models(enabled);
            let (_, bounds) = frame(&context, &mut positions, &models, editing, allowed, vec![]);
            let point = bounds.min + egui::vec2(40.0, 40.0);
            frame(
                &context,
                &mut positions,
                &models,
                editing,
                allowed,
                vec![egui::Event::PointerMoved(point)],
            );
            frame(
                &context,
                &mut positions,
                &models,
                editing,
                allowed,
                vec![pointer_button(point, true)],
            );
            let (result, _) = frame(
                &context,
                &mut positions,
                &models,
                editing,
                allowed,
                vec![pointer_button(point, false)],
            );
            assert_eq!(
                result.trigger, expected,
                "editing={editing} allowed={allowed} enabled={enabled}"
            );
            assert!(!result.layout_changed);
            assert_eq!(positions, initial);
        }
    }

    #[test]
    fn drag_commits_once_on_release_and_never_sends_a_game_command() {
        let context = egui::Context::default();
        let mut positions = crate::native_workspace::default_key_positions();
        let initial = positions;
        let models = models(true);
        let (_, bounds) = frame(&context, &mut positions, &models, true, true, vec![]);
        let point = bounds.min + egui::vec2(40.0, 40.0);
        frame(
            &context,
            &mut positions,
            &models,
            true,
            true,
            vec![egui::Event::PointerMoved(point)],
        );
        frame(
            &context,
            &mut positions,
            &models,
            true,
            true,
            vec![pointer_button(point, true)],
        );
        let target = point + egui::vec2(32.0, 16.0);
        let (dragging, _) = frame(
            &context,
            &mut positions,
            &models,
            true,
            true,
            vec![egui::Event::PointerMoved(target)],
        );
        assert!(!dragging.layout_changed);
        assert!(dragging.trigger.is_none());
        assert_eq!(positions, initial);
        let (dropped, _) = frame(
            &context,
            &mut positions,
            &models,
            true,
            true,
            vec![pointer_button(target, false)],
        );
        assert!(dropped.layout_changed);
        assert!(dropped.trigger.is_none());
        assert_ne!(positions[0], initial[0]);
        let area = bounds.shrink(16.0);
        let original_rects = layout_rects(area, &initial);
        let committed_rects = layout_rects(area, &positions);
        assert_eq!(&committed_rects[1..], &original_rects[1..]);
        let (settled, _) = frame(&context, &mut positions, &models, true, true, vec![]);
        assert!(!settled.layout_changed);
    }

    #[test]
    fn committing_a_repaired_import_never_repositions_untouched_cards() {
        let area = inner(720.0);
        let mut positions = [[0.5, 0.5]; 9];
        let visible = layout_rects(area, &positions);
        commit_layout(&mut positions, &visible, area, 0, visible[0]);
        assert_eq!(layout_rects(area, &positions), visible);
    }

    #[test]
    fn escape_and_occupied_drop_cancel_without_persisting_or_triggering() {
        for escape in [false, true] {
            let context = egui::Context::default();
            let mut positions = crate::native_workspace::default_key_positions();
            let initial = positions;
            let models = models(true);
            let (_, bounds) = frame(&context, &mut positions, &models, true, true, vec![]);
            let rects = layout_rects(bounds.shrink(16.0), &positions);
            let grab = egui::vec2(24.0, 24.0);
            let point = rects[0].min + grab;
            frame(
                &context,
                &mut positions,
                &models,
                true,
                true,
                vec![egui::Event::PointerMoved(point)],
            );
            frame(
                &context,
                &mut positions,
                &models,
                true,
                true,
                vec![pointer_button(point, true)],
            );
            let target = if escape {
                point + egui::vec2(32.0, 16.0)
            } else {
                rects[1].min + grab
            };
            frame(
                &context,
                &mut positions,
                &models,
                true,
                true,
                vec![egui::Event::PointerMoved(target)],
            );
            if escape {
                let (result, _) = frame(
                    &context,
                    &mut positions,
                    &models,
                    true,
                    true,
                    vec![egui::Event::Key {
                        key: egui::Key::Escape,
                        physical_key: None,
                        pressed: true,
                        repeat: false,
                        modifiers: Default::default(),
                    }],
                );
                assert!(!result.layout_changed && result.trigger.is_none());
            }
            let (result, _) = frame(
                &context,
                &mut positions,
                &models,
                true,
                true,
                vec![pointer_button(target, false)],
            );
            assert!(!result.layout_changed && result.trigger.is_none());
            assert_eq!(positions, initial, "escape={escape}");
        }
    }
}
