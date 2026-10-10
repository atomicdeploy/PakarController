#![allow(dead_code)]
//! Compact RGB editor shared by channel presentation and custom accents.
//! The swatch is a click target inside the text field's reserved leading margin.
use eframe::egui;

#[derive(serde::Deserialize)]
pub struct PaletteSwatch {
    pub label: String,
    pub hex: String,
}

pub fn semantic_swatches() -> &'static [PaletteSwatch] {
    static SWATCHES: std::sync::LazyLock<Vec<PaletteSwatch>> = std::sync::LazyLock::new(|| {
        serde_json::from_str(include_str!("../../../assets/themes/ui-colors.json"))
            .expect("valid shared UI swatches")
    });
    &SWATCHES
}

fn swatch_row(ui: &mut egui::Ui) -> Option<egui::Color32> {
    let mut selected = None;
    ui.horizontal_wrapped(|ui| {
        for swatch in semantic_swatches() {
            let [r, g, b] = crate::config::parse_rgb_hex(&swatch.hex).expect("valid shared swatch");
            let color = egui::Color32::from_rgb(r, g, b);
            let (rect, response) =
                ui.allocate_exact_size(egui::vec2(22.0, 22.0), egui::Sense::click());
            ui.painter().circle_filled(rect.center(), 7.0, color);
            ui.painter().circle_stroke(
                rect.center(),
                7.0,
                ui.visuals().widgets.noninteractive.bg_stroke,
            );
            if response.on_hover_text(&swatch.label).clicked() {
                selected = Some(color);
            }
        }
    });
    selected
}

/// Compact RGB-valued control with the same swatches as HEX fields.
pub fn color_button_srgba(ui: &mut egui::Ui, color: &mut egui::Color32) -> egui::Response {
    let (rect, mut response) = ui.allocate_exact_size(egui::vec2(28.0, 28.0), egui::Sense::click());
    ui.painter().circle_filled(rect.center(), 8.0, *color);
    ui.painter().circle_stroke(
        rect.center(),
        8.0,
        ui.visuals().widgets.noninteractive.bg_stroke,
    );
    egui::Popup::menu(&response)
        .close_behavior(egui::PopupCloseBehavior::CloseOnClickOutside)
        .show(|ui| {
            if let Some(selected) = swatch_row(ui) {
                *color = selected;
                response.mark_changed();
            }
            ui.separator();
            if egui::color_picker::color_picker_color32(
                ui,
                color,
                egui::color_picker::Alpha::OnlyBlend,
            ) {
                response.mark_changed();
            }
        });
    response
}

pub fn color_button_srgb(ui: &mut egui::Ui, rgb: &mut [u8; 3]) -> egui::Response {
    let mut color = egui::Color32::from_rgb(rgb[0], rgb[1], rgb[2]);
    let response = color_button_srgba(ui, &mut color);
    if response.changed() {
        *rgb = [color.r(), color.g(), color.b()];
    }
    response
}

pub fn color_field(
    ui: &mut egui::Ui,
    hex: &mut String,
    fallback: [u8; 3],
    width: f32,
) -> egui::Response {
    let id = ui.next_auto_id().with("rgb-color-field");
    let mut response = ui.add_sized(
        [width, 28.0],
        crate::ui::dialog::singleline_text_edit(hex)
            .id(id)
            .desired_width(width)
            .margin(egui::Margin {
                left: 30,
                right: 6,
                top: 5,
                bottom: 5,
            })
            .char_limit(7)
            .hint_text("#RRGGBB"),
    );
    let rgb = crate::config::parse_rgb_hex(hex).unwrap_or(fallback);
    let swatch_rect = egui::Rect::from_center_size(
        egui::pos2(response.rect.left() + 15.0, response.rect.center().y),
        egui::vec2(20.0, 20.0),
    );
    let swatch = ui
        .interact(swatch_rect, id.with("swatch"), egui::Sense::click())
        .on_hover_text("Choose color");
    ui.painter().circle_filled(
        swatch_rect.center(),
        6.0,
        egui::Color32::from_rgb(rgb[0], rgb[1], rgb[2]),
    );
    ui.painter().circle_stroke(
        swatch_rect.center(),
        6.0,
        ui.visuals().widgets.noninteractive.bg_stroke,
    );
    egui::Popup::menu(&swatch)
        .id(id.with("palette"))
        .close_behavior(egui::PopupCloseBehavior::CloseOnClickOutside)
        .show(|ui| {
            if let Some(selected) = swatch_row(ui) {
                *hex = format!(
                    "#{:02X}{:02X}{:02X}",
                    selected.r(),
                    selected.g(),
                    selected.b()
                );
                response.mark_changed();
            }
            ui.separator();
            let mut color = egui::Color32::from_rgb(rgb[0], rgb[1], rgb[2]);
            if egui::color_picker::color_picker_color32(
                ui,
                &mut color,
                egui::color_picker::Alpha::Opaque,
            ) {
                *hex = format!("#{:02X}{:02X}{:02X}", color.r(), color.g(), color.b());
                response.mark_changed();
            }
        });
    response
}
