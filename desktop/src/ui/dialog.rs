#![allow(dead_code)]
use eframe::egui;

/// Shared geometry for every single-line navigation row and footer action.
///
/// Keep these values here instead of allowing individual dialogs to invent
/// their own button metrics. This is deliberately public within the crate so
/// regression tests can enforce a single dialog design system.
pub const NAVIGATION_HEIGHT: f32 = 32.0;
pub const ACTION_HEIGHT: f32 = 28.0;
pub const NAVIGATION_DETAIL_HEIGHT: f32 = 44.0;
pub(crate) const ACTION_BUTTON_WIDTH: f32 = 104.0;
const CONTROL_CORNER_RADIUS: f32 = 7.0;
const CONTROL_TEXT_SIZE: f32 = 13.0;

/// Single-line input text stays centered when its row supplies extra height.
/// Multiline/wrapped editors intentionally keep TextEdit's top alignment.
pub fn singleline_text_edit<'a>(
    ui: &egui::Ui,
    text: &'a mut dyn egui::TextBuffer,
) -> egui::TextEdit<'a> {
    let align = crate::ui::i18n::input_alignment(crate::ui::i18n::rtl(), text.as_str());
    egui::TextEdit::singleline(text)
        .horizontal_align(align)
        .vertical_align(egui::Align::Center)
        .font(egui::TextStyle::Button)
        .margin(egui::Margin::symmetric(4, 0))
        .min_size(egui::vec2(0.0, crate::ui::control_density::height(ui)))
}

/// Workspace overlays paint immediately above their panel, never above dialogs.
/// Raising individual controls to Foreground lets them escape modal backdrops.
pub fn workspace_overlay_layer(
    ui: &egui::Ui,
    id: impl std::hash::Hash + std::fmt::Debug,
) -> egui::LayerId {
    let parent = ui.layer_id();
    let child = egui::LayerId::new(parent.order, parent.id.with(id));
    ui.ctx().set_sublayer(parent, child);
    child
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DialogHost {
    Embedded,
    Native,
}

/// Resolve the presentation host independently from a dialog's contents.
/// Dialog implementations can therefore keep one body and add a native host
/// without cloning their controls or behavior.
pub const fn preferred_host(native_requested: bool, native_supported: bool) -> DialogHost {
    if native_requested && native_supported {
        DialogHost::Native
    } else {
        DialogHost::Embedded
    }
}

/// Center a dialog the first time it opens without pinning it there. Unlike
/// `Window::anchor`, this leaves the remembered position free to follow title-
/// bar drags on subsequent frames.
pub fn centered_default_rect(bounds: egui::Rect, desired_size: egui::Vec2) -> egui::Rect {
    egui::Rect::from_center_size(bounds.center(), desired_size.min(bounds.size()))
}

/// Finite, internally consistent geometry for a resizable in-app dialog.
///
/// `egui::Window` persists its last requested size. During an interactive
/// resize it also runs a sizing pass, so feeding it an inverted constraint or
/// a minimum larger than its current viewport can poison the remembered
/// rectangle and terminate the render loop. Derive every constraint from the
/// same clamped bounds instead of recomputing unrelated min/max values in each
/// dialog.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct DialogGeometry {
    pub bounds: egui::Rect,
    pub default_rect: egui::Rect,
    pub min_size: egui::Vec2,
    pub max_size: egui::Vec2,
}

pub fn bounded_geometry(
    content_rect: egui::Rect,
    margin: f32,
    desired_size: egui::Vec2,
    requested_min: egui::Vec2,
    requested_max: egui::Vec2,
) -> DialogGeometry {
    let content_rect =
        if content_rect.is_finite() && content_rect.width() > 0.0 && content_rect.height() > 0.0 {
            content_rect
        } else {
            egui::Rect::from_min_size(egui::Pos2::ZERO, egui::Vec2::splat(1.0))
        };
    let maximum_inset = ((content_rect.width().min(content_rect.height()) - 1.0) * 0.5).max(0.0);
    let safe_margin = margin.max(0.0).min(maximum_inset);
    let bounds = content_rect.shrink(safe_margin);
    let available = egui::vec2(bounds.width().max(1.0), bounds.height().max(1.0));
    let max_size = egui::vec2(
        requested_max.x.max(1.0).min(available.x),
        requested_max.y.max(1.0).min(available.y),
    );
    let min_size = egui::vec2(
        requested_min.x.max(1.0).min(max_size.x),
        requested_min.y.max(1.0).min(max_size.y),
    );
    let default_size = egui::vec2(
        desired_size.x.clamp(min_size.x, max_size.x),
        desired_size.y.clamp(min_size.y, max_size.y),
    );
    DialogGeometry {
        bounds,
        default_rect: centered_default_rect(bounds, default_size),
        min_size,
        max_size,
    }
}

/// Build a window frame whose body is always fully opaque.
///
/// The application's Mica/DWM theme may deliberately give `window_fill` an
/// alpha channel. That looks appropriate for the main viewport, but applying
/// it to a floating editor lets controls from the workspace show through the
/// dialog and makes both surfaces unreadable. Dialogs use the same theme RGB
/// while explicitly discarding that alpha component.
pub fn opaque_window_frame(ui: &egui::Ui) -> egui::Frame {
    egui::Frame::window(ui.style()).fill(opaque_color(ui.visuals().window_fill()))
}

pub fn opaque_window_frame_from_context(ctx: &egui::Context) -> egui::Frame {
    let style = ctx.global_style();
    egui::Frame::window(&style).fill(opaque_color(style.visuals.window_fill()))
}

fn opaque_color(fill: egui::Color32) -> egui::Color32 {
    let [red, green, blue, _alpha] = fill.to_srgba_unmultiplied();
    egui::Color32::from_rgb(red, green, blue)
}

/// A consistent, keyboard-focusable dialog action with a Phosphor icon.
pub fn action_button(ui: &mut egui::Ui, icon: &str, label: &str) -> egui::Response {
    action_button_with_kind(ui, icon, label, false)
}

/// The visually emphasized variant of [`action_button`]. Native and embedded
/// dialog hosts use this same control, so dialog contents do not fork merely
/// to obtain platform-appropriate action hierarchy.
pub fn primary_action_button(ui: &mut egui::Ui, icon: &str, label: &str) -> egui::Response {
    action_button_with_kind(ui, icon, label, true)
}

fn action_button_with_kind(
    ui: &mut egui::Ui,
    icon: &str,
    label: &str,
    primary: bool,
) -> egui::Response {
    let selection = ui.visuals().selection;
    // Primary actions are distinguished by color and stroke, not by changing
    // their geometry. Equal widths keep paired actions such as Open / Cancel
    // visually balanced in every dialog host.
    let minimum_width = ACTION_BUTTON_WIDTH;
    // Exact Pealayer 5b3d5c1 contract: colored RichText keeps the button font
    // and a semantic border recolors the existing stroke, never its width.
    let border_width = ui.visuals().widgets.inactive.bg_stroke.width;
    let text = egui::RichText::new(format!("{icon}  {}", crate::ui::i18n::text(label)))
        .font(egui::TextStyle::Button.resolve(ui.style()));
    ui.scope(|ui| {
        // Adapt Pealayer's contextual_transport_style: semantic ink/fill
        // belongs to the enabled action, including hover and press, while
        // disabled controls retain the host's actual disabled appearance.
        if primary && ui.is_enabled() {
            let style = ui.style_mut();
            style.visuals.override_text_color = None;
            let fill = selection.bg_fill;
            let lifted = |amount: u16| {
                egui::Color32::from_rgb(
                    ((u16::from(fill.r()) * (100 - amount) + 255 * amount) / 100) as u8,
                    ((u16::from(fill.g()) * (100 - amount) + 255 * amount) / 100) as u8,
                    ((u16::from(fill.b()) * (100 - amount) + 255 * amount) / 100) as u8,
                )
            };
            let pressed = egui::Color32::from_rgb(
                (u16::from(fill.r()) * 88 / 100) as u8,
                (u16::from(fill.g()) * 88 / 100) as u8,
                (u16::from(fill.b()) * 88 / 100) as u8,
            );
            for (visuals, fill) in [
                (&mut style.visuals.widgets.inactive, fill),
                (&mut style.visuals.widgets.hovered, lifted(8)),
                (&mut style.visuals.widgets.active, pressed),
                (&mut style.visuals.widgets.open, lifted(8)),
            ] {
                visuals.fg_stroke.color = selection.stroke.color;
                visuals.bg_fill = fill;
                visuals.weak_bg_fill = fill;
                visuals.bg_stroke = egui::Stroke::new(border_width, fill);
                visuals.expansion = 0.0;
            }
        }
        ui.add(
            egui::Button::new(text)
                .min_size(egui::vec2(minimum_width, ACTION_HEIGHT))
                .corner_radius(CONTROL_CORNER_RADIUS),
        )
    })
    .inner
}

/// A shared, left-aligned navigation row for modal/dialog side rails.
pub fn navigation_button(
    ui: &mut egui::Ui,
    selected: bool,
    icon: &str,
    label: &str,
    width: f32,
) -> egui::Response {
    navigation_control(ui, selected, icon, label, None, width, NAVIGATION_HEIGHT)
}

/// A two-line navigation row for modal/dialog lists that need compact metadata.
pub fn navigation_detail_button(
    ui: &mut egui::Ui,
    selected: bool,
    icon: &str,
    label: &str,
    metadata: &str,
    width: f32,
) -> egui::Response {
    navigation_control(
        ui,
        selected,
        icon,
        label,
        Some(metadata),
        width,
        NAVIGATION_DETAIL_HEIGHT,
    )
}

fn navigation_control(
    ui: &mut egui::Ui,
    selected: bool,
    icon: &str,
    label: &str,
    metadata: Option<&str>,
    width: f32,
    height: f32,
) -> egui::Response {
    let (rect, response) =
        ui.allocate_exact_size(egui::vec2(width.max(80.0), height), egui::Sense::click());
    if ui.is_rect_visible(rect) {
        let visuals = ui.style().interact_selectable(&response, selected);
        let selection = ui.visuals().selection;
        let fill = if selected {
            selection.bg_fill.gamma_multiply(0.22)
        } else {
            visuals.weak_bg_fill
        };
        let stroke = if response.has_focus() {
            egui::Stroke::new(1.5_f32, selection.bg_fill.gamma_multiply(1.25))
        } else if selected {
            egui::Stroke::new(1.0_f32, selection.bg_fill.gamma_multiply(1.18))
        } else {
            visuals.bg_stroke
        };
        let painter = ui.painter().with_clip_rect(rect);
        painter.rect(
            rect,
            CONTROL_CORNER_RADIUS,
            fill,
            stroke,
            egui::StrokeKind::Inside,
        );

        let rtl = crate::ui::i18n::rtl();
        let icon_x = if rtl {
            rect.right() - 17.0
        } else {
            rect.left() + 17.0
        };
        painter.text(
            egui::pos2(icon_x, rect.center().y),
            egui::Align2::CENTER_CENTER,
            icon,
            egui::FontId::proportional(15.0),
            if selected {
                selection.bg_fill
            } else {
                visuals.fg_stroke.color
            },
        );
        let text_x = if rtl {
            rect.right() - 33.0
        } else {
            rect.left() + 33.0
        };
        let title_y = if metadata.is_some() {
            rect.center().y - 7.0
        } else {
            rect.center().y
        };
        painter.text(
            egui::pos2(text_x, title_y),
            if rtl {
                egui::Align2::RIGHT_CENTER
            } else {
                egui::Align2::LEFT_CENTER
            },
            crate::ui::i18n::text(label),
            egui::FontId::proportional(CONTROL_TEXT_SIZE),
            if selected {
                ui.visuals().widgets.hovered.fg_stroke.color
            } else {
                ui.visuals().text_color()
            },
        );
        if let Some(metadata) = metadata {
            painter.text(
                egui::pos2(text_x, rect.center().y + 9.0),
                if rtl {
                    egui::Align2::RIGHT_CENTER
                } else {
                    egui::Align2::LEFT_CENTER
                },
                metadata,
                egui::FontId::proportional(10.5),
                ui.visuals().weak_text_color(),
            );
        }
    }
    response
}

/// Align dialog actions to the conventional trailing edge while respecting
/// the reading direction. Add the primary action first, then secondary ones.
pub fn action_row(ui: &mut egui::Ui, rtl: bool, body: impl FnOnce(&mut egui::Ui)) {
    let layout = if rtl {
        egui::Layout::left_to_right(egui::Align::Center)
    } else {
        egui::Layout::right_to_left(egui::Align::Center)
    };
    ui.allocate_ui_with_layout(
        egui::vec2(ui.available_width(), ACTION_HEIGHT),
        layout,
        body,
    );
}

/// A full-width footer with a utility action at the leading edge and primary /
/// cancel actions at the trailing edge.
pub fn action_bar(
    ui: &mut egui::Ui,
    rtl: bool,
    leading: impl FnOnce(&mut egui::Ui),
    trailing: impl FnOnce(&mut egui::Ui),
) {
    let outer = if rtl {
        egui::Layout::right_to_left(egui::Align::Center)
    } else {
        egui::Layout::left_to_right(egui::Align::Center)
    };
    let inner = if rtl {
        egui::Layout::left_to_right(egui::Align::Center)
    } else {
        egui::Layout::right_to_left(egui::Align::Center)
    };
    ui.allocate_ui_with_layout(
        egui::vec2(ui.available_width(), ACTION_HEIGHT),
        outer,
        |ui| {
            leading(ui);
            ui.allocate_ui_with_layout(
                egui::vec2(ui.available_width(), ACTION_HEIGHT),
                inner,
                trailing,
            );
        },
    );
}

pub fn escape_pressed(ctx: &egui::Context) -> bool {
    ctx.input(|input| input.key_pressed(egui::Key::Escape))
}

/// A visually consistent, width-bounded section used inside modal dialogs.
pub fn section(ui: &mut egui::Ui, icon: &str, title: &str, body: impl FnOnce(&mut egui::Ui)) {
    crate::ui::sync_elegance_theme(ui.ctx());
    elegance::Card::new()
        .padding(egui::Margin::same(12))
        .corner_radius(9.0)
        .show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.vertical(|ui| {
                ui.label(
                    egui::RichText::new(format!("{icon}  {}", crate::ui::i18n::text(title)))
                        .strong()
                        .color(ui.visuals().widgets.hovered.fg_stroke.color),
                );
                ui.add_space(6.0);
                body(ui);
            });
        });
}

/// A compact horizontal row that cannot consume the remaining dialog height.
pub fn compact_row(ui: &mut egui::Ui, rtl: bool, body: impl FnOnce(&mut egui::Ui)) {
    let height = ui.spacing().interact_size.y;
    ui.allocate_ui_with_layout(
        egui::vec2(ui.available_width(), height),
        crate::ui::i18n::layout(rtl, egui::Align::Center),
        body,
    );
}

/// A responsive settings row shared by compact media dialogs. Labels retain
/// their natural reading order, supporting copy sits directly below them, and
/// the interactive control remains aligned at the trailing edge. At narrow
/// widths the control wraps beneath the label instead of clipping either side.
pub fn setting_row(
    ui: &mut egui::Ui,
    icon: &str,
    label: &str,
    description: Option<&str>,
    body: impl FnOnce(&mut egui::Ui),
) {
    let available = ui.available_width();
    if available < 330.0 {
        ui.vertical(|ui| {
            ui.horizontal(|ui| {
                ui.label(egui::RichText::new(icon).color(ui.visuals().selection.bg_fill));
                ui.label(egui::RichText::new(crate::ui::i18n::text(label)).strong());
            });
            if let Some(description) = description {
                ui.label(
                    egui::RichText::new(crate::ui::i18n::text(description))
                        .small()
                        .weak(),
                );
            }
            ui.add_space(3.0);
            body(ui);
        });
    } else {
        let label_width = (available * 0.44).clamp(145.0, 210.0);
        ui.horizontal(|ui| {
            ui.allocate_ui_with_layout(
                egui::vec2(label_width, 0.0),
                egui::Layout::top_down(egui::Align::Min),
                |ui| {
                    ui.horizontal(|ui| {
                        ui.label(egui::RichText::new(icon).color(ui.visuals().selection.bg_fill));
                        ui.label(egui::RichText::new(crate::ui::i18n::text(label)).strong());
                    });
                    if let Some(description) = description {
                        ui.label(
                            egui::RichText::new(crate::ui::i18n::text(description))
                                .small()
                                .weak(),
                        );
                    }
                },
            );
            ui.allocate_ui_with_layout(
                egui::vec2(ui.available_width(), 0.0),
                egui::Layout::right_to_left(egui::Align::Center),
                body,
            );
        });
    }
}

/// A compact numeric input flanked by explicit decrement/increment actions.
/// This is intentionally one shared control so Subtitle and Audio settings do
/// not drift in sizing, keyboard input, bounds, or icon treatment.
pub fn numeric_stepper(
    ui: &mut egui::Ui,
    key: &str,
    value: &mut f64,
    range: std::ops::RangeInclusive<f64>,
    step: f64,
    reset: f64,
    decimals: usize,
    suffix: &str,
    steps: &mut std::collections::BTreeMap<String, crate::config::NumericInputSteps>,
    language: crate::config::AppLanguage,
) -> bool {
    let min = *range.start();
    let max = *range.end();
    let mut changed = false;
    let adjustment = steps
        .get(key)
        .copied()
        .filter(|s| s.is_valid())
        .unwrap_or_else(|| crate::config::NumericInputSteps::for_step(step));
    let effective_step = numeric_adjustment(adjustment, ui.input(|i| i.modifiers));
    ui.push_id(key, |ui| {
        ui.horizontal(|ui| {
            changed |= receive_numeric_paste(ui, key, value, &range, suffix);
            let decrement = ui
                .add_sized(
                    [28.0, 26.0],
                    egui::Button::new(crate::ui::icons::MINUS).corner_radius(6.0),
                )
                .on_hover_text(format!(
                    "Decrease by {effective_step} (Ctrl: fine; Shift: coarse)"
                ));
            if decrement.clicked() {
                changed |= adjust_numeric(value, -effective_step, min, max);
            }

            let response = ui.add_sized(
                [86.0, 26.0],
                egui::DragValue::new(value)
                    .speed(effective_step)
                    .range(range.clone())
                    .min_decimals(decimals)
                    .max_decimals(decimals.max(9))
                    .suffix(suffix),
            );
            changed |= response.changed();
            changed |= numeric_context_menu(
                ui, &response, key, value, range, step, reset, suffix, steps, language, 1e-9,
            );

            let increment = ui
                .add_sized(
                    [28.0, 26.0],
                    egui::Button::new(crate::ui::icons::PLUS).corner_radius(6.0),
                )
                .on_hover_text(format!(
                    "Increase by {effective_step} (Ctrl: fine; Shift: coarse)"
                ));
            if increment.clicked() {
                changed |= adjust_numeric(value, effective_step, min, max);
            }
        });
    });
    changed
}

/// Ctrl wins over Shift when both are held. Command provides the same fine
/// adjustment on macOS; integer-valued callers clamp their steps to one.
pub fn numeric_adjustment(
    steps: crate::config::NumericInputSteps,
    modifiers: egui::Modifiers,
) -> f64 {
    if modifiers.ctrl || modifiers.command {
        steps.fine
    } else if modifiers.shift {
        steps.coarse
    } else {
        steps.normal
    }
}

fn adjust_numeric(value: &mut f64, delta: f64, min: f64, max: f64) -> bool {
    let next = (*value + delta).clamp(min, max);
    let changed = next != *value;
    *value = next;
    changed
}

/// Hovered sliders own their wheel gesture, including egui's smoothed tail,
/// so adjusting a value cannot also scroll the containing settings dialog.
/// Raw events are used once; replaying smooth deltas would multiply changes.
pub fn numeric_slider_wheel(
    ui: &egui::Ui,
    response: &egui::Response,
    value: &mut f64,
    range: std::ops::RangeInclusive<f64>,
    steps: crate::config::NumericInputSteps,
) -> bool {
    if !response.enabled() || !response.hovered() || egui::Popup::is_any_open(ui.ctx()) {
        return false;
    }
    let delta = ui.ctx().input_mut(|input| {
        let mut adjustment = 0.0;
        let mut owns_wheel = false;
        for event in &input.events {
            if let egui::Event::MouseWheel {
                delta, modifiers, ..
            } = event
            {
                let axis = if delta.y.abs() >= delta.x.abs() {
                    delta.y
                } else {
                    delta.x
                };
                if axis.is_finite() && axis != 0.0 {
                    owns_wheel = true;
                    adjustment += f64::from(axis.signum()) * numeric_adjustment(steps, *modifiers);
                }
            }
        }
        if owns_wheel || input.smooth_scroll_delta != egui::Vec2::ZERO {
            input.smooth_scroll_delta = egui::Vec2::ZERO;
        }
        adjustment
    });
    delta != 0.0 && adjust_numeric(value, delta, *range.start(), *range.end())
}

fn parse_numeric_paste(
    text: &str,
    suffix: &str,
    range: &std::ops::RangeInclusive<f64>,
) -> Option<f64> {
    let text = text.trim();
    let text = if suffix.trim().is_empty() {
        text
    } else {
        text.strip_suffix(suffix.trim()).unwrap_or(text).trim()
    };
    text.parse::<f64>()
        .ok()
        .filter(|v| v.is_finite())
        .map(|v| v.clamp(*range.start(), *range.end()))
}

/// Consume only the requested field's OS-delivered paste event. No clipboard
/// polling or synthetic focus IDs (which previously broke Windows AccessKit).
pub fn receive_numeric_paste(
    ui: &mut egui::Ui,
    key: &str,
    value: &mut f64,
    range: &std::ops::RangeInclusive<f64>,
    suffix: &str,
) -> bool {
    let id = ui.make_persistent_id(("numeric-paste", key));
    if !ui
        .ctx()
        .data(|data| data.get_temp::<bool>(id).unwrap_or(false))
    {
        return false;
    }
    let pasted = ui.input_mut(|input| {
        let index = input
            .events
            .iter()
            .position(|event| matches!(event, egui::Event::Paste(_)))?;
        match input.events.remove(index) {
            egui::Event::Paste(text) => Some(text),
            _ => None,
        }
    });
    if let Some(text) = pasted {
        ui.ctx().data_mut(|data| data.remove::<bool>(id));
        if let Some(next) = parse_numeric_paste(&text, suffix, range) {
            let changed = *value != next;
            *value = next;
            return changed;
        }
    } else if ui.input(|input| input.pointer.any_pressed() || input.key_pressed(egui::Key::Escape))
    {
        ui.ctx().data_mut(|data| data.remove::<bool>(id));
    }
    false
}

/// Shared value menu for steppers and slider inputs. A true model default is
/// supplied by the caller; step edits use the same persisted config contract.
pub fn numeric_context_menu(
    ui: &mut egui::Ui,
    response: &egui::Response,
    key: &str,
    value: &mut f64,
    range: std::ops::RangeInclusive<f64>,
    default_step: f64,
    reset: f64,
    suffix: &str,
    steps: &mut std::collections::BTreeMap<String, crate::config::NumericInputSteps>,
    language: crate::config::AppLanguage,
    minimum_step: f64,
) -> bool {
    use crate::ui::icons;
    let tr = |text| crate::ui::i18n::tr(language, text);
    let mut changed = false;
    let paste_id = ui.make_persistent_id(("numeric-paste", key));
    let mut adjustment = steps
        .get(key)
        .copied()
        .filter(|s| s.is_valid())
        .unwrap_or_else(|| crate::config::NumericInputSteps::for_step(default_step));
    adjustment.normal = adjustment.normal.max(minimum_step);
    adjustment.fine = adjustment.fine.max(minimum_step);
    adjustment.coarse = adjustment.coarse.max(minimum_step);
    let previous = adjustment;
    response.context_menu(|ui| {
        if ui
            .button(format!(
                "{} {}",
                icons::ARROW_COUNTER_CLOCKWISE,
                tr("Reset")
            ))
            .clicked()
        {
            let next = reset.clamp(*range.start(), *range.end());
            changed |= *value != next;
            *value = next;
            ui.close();
        }
        if ui
            .button(format!("{} {}", icons::COPY, tr("Copy")))
            .clicked()
        {
            ui.ctx().copy_text(format!("{value}"));
            ui.close();
        }
        if ui
            .button(format!("{} {}", icons::CLIPBOARD, tr("Paste")))
            .clicked()
        {
            // Clear the old edit focus so the requested paste cannot also land
            // in an unrelated text field or append to the numeric editor.
            ui.ctx().memory_mut(|memory| {
                if let Some(id) = memory.focused() {
                    memory.surrender_focus(id);
                }
            });
            ui.ctx().data_mut(|data| data.insert_temp(paste_id, true));
            ui.ctx()
                .send_viewport_cmd(egui::ViewportCommand::RequestPaste);
            ui.close();
        }
        ui.separator();
        let step = numeric_adjustment(adjustment, ui.input(|input| input.modifiers));
        for (icon, label, delta) in [
            (icons::PLUS, "Increase", step),
            (icons::MINUS, "Decrease", -step),
        ] {
            let next = (*value + delta).clamp(*range.start(), *range.end());
            if ui
                .add_enabled(
                    next != *value,
                    egui::Button::new(format!("{icon} {} ({step}{suffix})", tr(label))),
                )
                .clicked()
            {
                changed |= adjust_numeric(value, delta, *range.start(), *range.end());
                ui.close();
            }
        }
        ui.menu_button(
            format!("{} {}", icons::SLIDERS_HORIZONTAL, tr("Adjustment steps")),
            |ui| {
                egui::Grid::new(("numeric-steps", key))
                    .num_columns(2)
                    .show(ui, |ui| {
                        for (label, value) in [
                            ("Normal", &mut adjustment.normal),
                            ("Fine (Ctrl)", &mut adjustment.fine),
                            ("Coarse (Shift)", &mut adjustment.coarse),
                        ] {
                            ui.label(tr(label));
                            ui.add(
                                egui::DragValue::new(value)
                                    .range(minimum_step..=1e12)
                                    .speed(default_step / 10.0)
                                    .max_decimals(9)
                                    .suffix(suffix),
                            );
                            ui.end_row();
                        }
                    });
                if ui
                    .button(format!(
                        "{} {}",
                        icons::ARROW_COUNTER_CLOCKWISE,
                        tr("Reset steps")
                    ))
                    .clicked()
                {
                    adjustment = crate::config::NumericInputSteps::for_step(default_step);
                    adjustment.normal = adjustment.normal.max(minimum_step);
                    adjustment.fine = adjustment.fine.max(minimum_step);
                    adjustment.coarse = adjustment.coarse.max(minimum_step);
                }
            },
        );
    });
    if adjustment != previous && adjustment.is_valid() {
        steps.insert(key.to_string(), adjustment);
        changed = true;
    }
    changed
}

/// Force scroll content into one viewport-width vertical column. Scroll areas
/// otherwise inherit a surrounding horizontal layout and can grow sideways.
pub fn scroll_column(
    ui: &mut egui::Ui,
    id: impl std::hash::Hash + std::fmt::Debug,
    max_height: Option<f32>,
    body: impl FnOnce(&mut egui::Ui),
) {
    scroll_column_impl(ui, id, max_height, false, body);
}

/// A vertically content-sized scroll column for compact dialogs. It behaves
/// like [`scroll_column`], but does not stretch a short body to fill the whole
/// window. A height limit still lets the body scroll when the user makes the
/// dialog smaller than its contents.
pub fn fit_scroll(
    ui: &mut egui::Ui,
    id: impl std::hash::Hash + std::fmt::Debug,
    max_height: Option<f32>,
    body: impl FnOnce(&mut egui::Ui),
) {
    scroll_column_impl(ui, id, max_height, true, body);
}

fn scroll_column_impl(
    ui: &mut egui::Ui,
    id: impl std::hash::Hash + std::fmt::Debug,
    max_height: Option<f32>,
    shrink_height_to_content: bool,
    body: impl FnOnce(&mut egui::Ui),
) {
    let width = ui.available_width();
    let mut scroll = egui::ScrollArea::vertical()
        .id_salt(id)
        .auto_shrink([false, shrink_height_to_content]);
    if let Some(max_height) = max_height {
        scroll = scroll.max_height(max_height);
    }
    scroll.show(ui, |ui| {
        ui.allocate_ui_with_layout(
            egui::vec2(width, 0.0),
            egui::Layout::top_down(egui::Align::Min),
            |ui| {
                ui.set_width(width);
                body(ui);
            },
        );
    });
}
