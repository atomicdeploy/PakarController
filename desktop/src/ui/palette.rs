//! Native palette adapter using Pealayer's actual catalog and role mapping.
//! Embedded Web UI also consumes the catalog; host preference synchronization remains pending.
use std::{collections::BTreeMap, sync::OnceLock};

type PaletteCatalog = BTreeMap<String, BTreeMap<String, BTreeMap<String, String>>>;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Palette {
    #[default]
    Studio,
    Native,
}

fn colors(palette: Palette, dark: bool) -> &'static BTreeMap<String, String> {
    static CATALOG: OnceLock<PaletteCatalog> = OnceLock::new();
    let catalog = CATALOG.get_or_init(|| {
        serde_json::from_str(include_str!("../../../assets/themes/palettes.json"))
            .expect("bundled theme palettes must be valid")
    });
    &catalog[match palette {
        Palette::Studio => "studio",
        Palette::Native => "native",
    }][if dark { "dark" } else { "light" }]
}

fn parse_rgb(value: &str) -> eframe::egui::Color32 {
    let value = value.trim_start_matches('#');
    let byte = |at| u8::from_str_radix(&value[at..at + 2], 16).expect("RGB palette value");
    eframe::egui::Color32::from_rgb(byte(0), byte(2), byte(4))
}

pub fn color(palette: Palette, dark: bool, role: &str) -> eframe::egui::Color32 {
    parse_rgb(&colors(palette, dark)[role])
}

pub fn apply(visuals: &mut eframe::egui::Visuals, palette: Palette) {
    use eframe::egui::Stroke;
    let dark = visuals.dark_mode;
    let rgb = |role| color(palette, dark, role);
    let border = Stroke::new(1.0, rgb("line"));
    let hover_border = Stroke::new(1.0, rgb("line-strong"));
    visuals.warn_fg_color = rgb("amber");
    visuals.error_fg_color = rgb("red");
    visuals.panel_fill = rgb("surface-0");
    visuals.window_fill = rgb("surface-1");
    visuals.window_stroke = border;
    visuals.extreme_bg_color = rgb("canvas");
    visuals.text_edit_bg_color = Some(rgb("surface-0"));
    visuals.code_bg_color = rgb("surface-2");
    visuals.faint_bg_color = rgb("surface-2");
    visuals.weak_text_color = Some(rgb("muted"));
    visuals.widgets.noninteractive.fg_stroke = Stroke::new(1.0, rgb("text"));
    visuals.widgets.noninteractive.bg_stroke = border;
    visuals.widgets.noninteractive.bg_fill = rgb("surface-1");
    visuals.widgets.noninteractive.weak_bg_fill = rgb("surface-1");
    visuals.widgets.inactive.fg_stroke = Stroke::new(1.0, rgb("text"));
    visuals.widgets.inactive.bg_stroke = border;
    visuals.widgets.inactive.bg_fill = rgb("surface-2");
    visuals.widgets.inactive.weak_bg_fill = rgb("surface-2");
    visuals.widgets.hovered.fg_stroke = Stroke::new(1.0, rgb("text"));
    visuals.widgets.hovered.bg_stroke = hover_border;
    visuals.widgets.hovered.bg_fill = rgb("surface-3");
    visuals.widgets.hovered.weak_bg_fill = rgb("surface-3");
    // PCController's fixed border / color-only hover contract, adapted to
    // egui. Never leave pressed/open controls on egui's stock white 2px edge.
    visuals.widgets.active.fg_stroke = Stroke::new(1.0, rgb("text"));
    visuals.widgets.active.bg_stroke = hover_border;
    visuals.widgets.active.bg_fill = rgb("surface-1");
    visuals.widgets.active.weak_bg_fill = rgb("surface-1");
    visuals.widgets.open.fg_stroke = Stroke::new(1.0, rgb("text"));
    visuals.widgets.open.bg_stroke = hover_border;
    visuals.widgets.open.bg_fill = rgb("surface-3");
    visuals.widgets.open.weak_bg_fill = rgb("surface-3");
    for widget in [
        &mut visuals.widgets.noninteractive,
        &mut visuals.widgets.inactive,
        &mut visuals.widgets.hovered,
        &mut visuals.widgets.active,
        &mut visuals.widgets.open,
    ] {
        widget.corner_radius = eframe::egui::CornerRadius::same(6);
        widget.expansion = 0.0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imported_palettes_have_every_required_role() {
        let roles = [
            "canvas",
            "surface-0",
            "surface-1",
            "surface-2",
            "surface-3",
            "line",
            "line-strong",
            "text",
            "muted",
            "subtle",
            "green",
            "blue",
            "red",
            "amber",
        ];
        for palette in [Palette::Studio, Palette::Native] {
            for dark in [true, false] {
                assert_eq!(colors(palette, dark).len(), 14);
                for role in roles {
                    assert!(crate::config::parse_rgb_hex(&colors(palette, dark)[role]).is_some());
                }
            }
        }
        assert_eq!(colors(Palette::Studio, true)["surface-1"], "#11161e");
        assert_eq!(colors(Palette::Studio, false)["surface-0"], "#f8fafc");
    }

    #[test]
    fn donor_surface_text_roles_and_geometry_are_preserved_in_both_themes() {
        for palette in [Palette::Studio, Palette::Native] {
            for dark in [false, true] {
                let mut visuals = if dark {
                    eframe::egui::Visuals::dark()
                } else {
                    eframe::egui::Visuals::light()
                };
                let radius = visuals.window_corner_radius;
                apply(&mut visuals, palette);
                assert_eq!(visuals.window_fill, color(palette, dark, "surface-1"));
                assert_eq!(
                    visuals.widgets.inactive.fg_stroke.color,
                    color(palette, dark, "text")
                );
                assert_eq!(visuals.window_corner_radius, radius);
            }
        }
    }
}
