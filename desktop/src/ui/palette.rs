//! Adapted directly from Pealayer's shared native/Web palette contract.
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
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imported_palettes_have_every_required_role() {
        for palette in [Palette::Studio, Palette::Native] {
            for dark in [true, false] {
                assert_eq!(colors(palette, dark).len(), 14);
            }
        }
    }
}
