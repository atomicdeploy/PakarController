//! Selective Elegance widgets, driven by Pealayer rather than a stock theme.
use crate::config::ColorPalette;
use eframe::egui::{Color32, Context, Id, Style, TextStyle};
use std::sync::Arc;

#[derive(Clone)]
struct SourceStyle {
    style: Arc<Style>,
    palette: ColorPalette,
}

fn source_id() -> Id {
    Id::new("pealayer-elegance-source-palette")
}

pub(crate) fn set_source_palette(ctx: &Context, palette: ColorPalette) {
    ctx.data_mut(|data| data.insert_temp(source_id(), palette));
}

/// Use the resolved host style, including native OS theme/accent changes.
/// `Theme::install` is the public widget-theme API; restore its global style
/// side effects immediately. Never call this from a Context transaction.
pub fn sync_elegance_theme(ctx: &Context) {
    let style = ctx.global_style();
    let source = ctx
        .data(|data| data.get_temp(source_id()))
        .unwrap_or_default();
    // Many cards share this adapter in one frame. Reuse the resolved theme
    // until the host style/palette changes; do not recompute contrast per row.
    let cache_id = source_id().with("resolved-style");
    let unchanged = ctx
        .data(|data| data.get_temp::<SourceStyle>(cache_id))
        .is_some_and(|cached| cached.palette == source && Arc::ptr_eq(&cached.style, &style));
    if unchanged {
        return;
    }
    let theme = from_style(&style, source);
    if elegance::Theme::current(ctx) != theme {
        theme.install(ctx);
        ctx.set_global_style(style.clone());
    }
    ctx.data_mut(|data| {
        data.insert_temp(
            cache_id,
            SourceStyle {
                style,
                palette: source,
            },
        )
    });
}

fn opaque(color: Color32) -> Color32 {
    let [r, g, b, _] = color.to_srgba_unmultiplied();
    Color32::from_rgb(r, g, b)
}

fn shade(color: Color32, factor: f32) -> Color32 {
    Color32::from_rgb(
        (f32::from(color.r()) * factor).floor() as u8,
        (f32::from(color.g()) * factor).floor() as u8,
        (f32::from(color.b()) * factor).floor() as u8,
    )
}

fn luminance(color: Color32) -> f64 {
    let linear = |channel: u8| {
        let value = f64::from(channel) / 255.0;
        if value <= 0.04045 {
            value / 12.92
        } else {
            ((value + 0.055) / 1.055).powf(2.4)
        }
    };
    0.2126 * linear(color.r()) + 0.7152 * linear(color.g()) + 0.0722 * linear(color.b())
}

/// Elegance 0.16.1 paints white labels on filled buttons. Keep hue but deepen
/// only their fills until white has 4.5:1 contrast. Structural focus and status
/// colors remain exact host colors. Native primary actions keep adaptive ink.
fn white_label_fill(color: Color32) -> Color32 {
    let color = opaque(color);
    if 1.05 / (luminance(color) + 0.05) >= 4.5 {
        return color;
    }
    let (mut low, mut high) = (0.0, 1.0);
    for _ in 0..16 {
        let factor = (low + high) * 0.5;
        if 1.05 / (luminance(shade(color, factor)) + 0.05) >= 4.5 {
            low = factor;
        } else {
            high = factor;
        }
    }
    shade(color, low)
}

fn from_style(style: &Style, source: ColorPalette) -> elegance::Theme {
    let v = &style.visuals;
    let role = |name| super::palette::color(source, v.dark_mode, name);
    let blue = white_label_fill(v.selection.bg_fill);
    let green = white_label_fill(role("green"));
    let red = white_label_fill(v.error_fg_color);
    let amber = white_label_fill(v.warn_fg_color);
    let typography = elegance::Typography {
        body: TextStyle::Body.resolve(style).size,
        button: TextStyle::Button.resolve(style).size,
        label: TextStyle::Body.resolve(style).size,
        small: TextStyle::Small.resolve(style).size,
        heading: TextStyle::Heading.resolve(style).size,
        monospace: TextStyle::Monospace.resolve(style).size,
    };
    elegance::Theme {
        palette: elegance::Palette {
            is_dark: v.dark_mode,
            bg: opaque(v.panel_fill),
            card: opaque(v.window_fill),
            input_bg: opaque(v.text_edit_bg_color.unwrap_or(v.extreme_bg_color)),
            border: v.window_stroke.color,
            text: v.text_color(),
            text_muted: v.weak_text_color(),
            text_faint: role("subtle"),
            blue,
            blue_hover: shade(blue, 0.88),
            green,
            green_hover: shade(green, 0.88),
            red,
            red_hover: shade(red, 0.88),
            // Pealayer has no separate purple action role: neutral-positive
            // actions follow the user's accent, not a second stock brand.
            purple: blue,
            purple_hover: shade(blue, 0.88),
            amber,
            amber_hover: shade(amber, 0.88),
            focus: v.selection.bg_fill,
            success: role("green"),
            danger: v.error_fg_color,
            warning: v.warn_fg_color,
        },
        control_radius: f32::from(v.widgets.inactive.corner_radius.nw),
        card_radius: f32::from(v.window_corner_radius.nw),
        card_padding: 12.0,
        control_padding_y: ((super::dialog::ACTION_HEIGHT - typography.button * 1.2) * 0.5)
            .max(style.spacing.button_padding.y),
        control_padding_x: style.spacing.button_padding.x.max(8.0),
        typography,
    }
}
