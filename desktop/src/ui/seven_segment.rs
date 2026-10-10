//! Live TM1637 preview adapted from Pealayer's SevenSegmentDisplay.tsx and
//! PCController's validated SevenSegmentPreview component.
use eframe::egui;

const SEGMENTS: [(f32, f32, f32, f32); 7] = [
    (8.0, 5.0, 28.0, 5.0),
    (31.0, 8.0, 31.0, 27.0),
    (31.0, 32.0, 31.0, 51.0),
    (8.0, 54.0, 28.0, 54.0),
    (5.0, 32.0, 5.0, 51.0),
    (5.0, 8.0, 5.0, 27.0),
    (8.0, 29.5, 28.0, 29.5),
];

pub fn draw(ui: &mut egui::Ui, masks: &[u8], brightness: u8, active: bool) -> egui::Response {
    let size = egui::vec2(4.0 * 40.0, 74.0);
    let (rect, response) = ui.allocate_exact_size(size, egui::Sense::hover());
    let intensity = if active {
        (f32::from(brightness) / 7.0).clamp(0.2, 1.0)
    } else {
        0.08
    };
    let lit = egui::Color32::from_rgba_unmultiplied(255, 74, 58, (255.0 * intensity) as u8);
    let unlit = egui::Color32::from_rgba_unmultiplied(92, 30, 27, 70);
    ui.painter()
        .rect_filled(rect, 8.0, egui::Color32::from_rgb(19, 12, 13));
    for digit in 0..4 {
        let mask = masks.get(digit).copied().unwrap_or_default();
        let origin = rect.min + egui::vec2(2.0 + digit as f32 * 39.0, 6.0);
        for (bit, (x1, y1, x2, y2)) in SEGMENTS.iter().copied().enumerate() {
            let color = if mask & (1 << bit) != 0 { lit } else { unlit };
            ui.painter().line_segment(
                [origin + egui::vec2(x1, y1), origin + egui::vec2(x2, y2)],
                egui::Stroke::new(4.2, color),
            );
        }
        let dot = origin + egui::vec2(32.0, 54.0);
        ui.painter()
            .circle_filled(dot, 2.4, if mask & 0x80 != 0 { lit } else { unlit });
    }
    response.on_hover_text("Live host-observed TM1637 segment frame")
}
