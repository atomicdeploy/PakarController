//! Six physical RGB sectors; never infer pixel values from cell/game state.
use eframe::egui::{self, Color32, Pos2, Rect, Stroke};
use serde::{Deserialize, Serialize};
use std::time::{Duration, Instant};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Shape {
    Rounded,
    #[default]
    Circle,
    Hexagon,
}
impl Shape {
    pub fn label(self) -> &'static str {
        match self {
            Self::Rounded => "Rounded",
            Self::Circle => "Circle",
            Self::Hexagon => "Hexagon",
        }
    }
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Frame {
    pub rgb: String,
    pub physical: Vec<u8>,
    pub revision: u32,
    pub brightness: u8,
    #[serde(default)]
    pub pending: bool,
}
impl Frame {
    pub fn structurally_valid(&self) -> bool {
        self.rgb.len() == 480 * 6
            && self.rgb.bytes().all(|b| b.is_ascii_hexdigit())
            && self.physical.len() == 80
            && (1..=80).all(|id| self.physical.iter().filter(|p| **p == id).count() == 1)
    }
    pub fn valid(&self) -> bool {
        !self.pending && self.structurally_valid()
    }
    pub fn cell(&self, id: u8) -> Option<(u16, [Color32; 6])> {
        if !(1..=80).contains(&id) || !self.valid() {
            return None;
        }
        let first = u16::from(self.physical[id as usize - 1] - 1) * 6;
        let mut colors = [Color32::TRANSPARENT; 6];
        for (sector, color) in colors.iter_mut().enumerate() {
            let at = (usize::from(first) + sector) * 6;
            let hex = &self.rgb[at..at + 6];
            *color = Color32::from_rgb(
                u8::from_str_radix(&hex[..2], 16).ok()?,
                u8::from_str_radix(&hex[2..4], 16).ok()?,
                u8::from_str_radix(&hex[4..], 16).ok()?,
            );
        }
        Some((first, colors))
    }
}
pub struct Sample {
    pub frame: Frame,
    pub at: Instant,
}
impl Sample {
    pub fn fresh(&self) -> bool {
        self.at.elapsed() < Duration::from_secs(5)
    }
    /// A transient no-frame observation is not a new RGB sample. Keep only the
    /// existing same-peer sample for its original freshness window; never renew
    /// its clock from null/invalid telemetry. Peer/map revocation clears outside.
    pub fn observe(slot: &mut Option<Self>, frame: Option<Frame>) {
        match frame {
            Some(frame) if frame.valid() => *slot = Some(Self { frame, at: Instant::now() }),
            Some(frame) if !frame.structurally_valid() => *slot = None,
            // A queued/staging frame is not a new presented RGB observation.
            // Like null, it cannot renew the previous sample's clock.
            _ if slot.as_ref().is_some_and(|sample| !sample.fresh()) => *slot = None,
            _ => {},
        }
    }
}
/// Screen Y increases downward: subtracting angle gives CCW. Sector 0 is top.
pub fn point(center: Pos2, radius: f32, sector: f32) -> Pos2 {
    let angle = -std::f32::consts::FRAC_PI_2 - sector * std::f32::consts::TAU / 6.0;
    center + egui::vec2(angle.cos(), angle.sin()) * radius
}
pub fn draw(
    ui: &egui::Ui,
    response: &egui::Response,
    shape: Shape,
    selected: bool,
    diagnostic: bool,
    sample: Option<&Frame>,
    physical: Option<u8>,
    id: u8,
    label: &str,
    unknown: Color32,
    animate: bool,
) {
    let rect = response.rect;
    let focus = response.has_focus();
    let center = rect.center();
    let radius = (rect.width().min(rect.height()) * 0.5 - 3.0).max(1.0);
    let painter = ui.painter().with_clip_rect(rect);
    let cell = sample.and_then(|frame| frame.cell(id));
    let first = cell.as_ref().map(|(first, _)| *first).or_else(|| {
        physical
            .filter(|p| (1..=80).contains(p))
            .map(|p| u16::from(p - 1) * 6)
    });
    for sector in 0..6 {
        let mut points = vec![center];
        let steps = if shape == Shape::Circle { 8 } else { 1 };
        for step in 0..=steps {
            points.push(point(
                center,
                radius,
                sector as f32 - 0.5 + step as f32 / steps as f32,
            ));
        }
        let color = cell
            .as_ref()
            .map(|(_, colors)| colors[sector])
            .unwrap_or(unknown);
        painter.add(egui::Shape::convex_polygon(
            points,
            color,
            Stroke::new(0.6, ui.visuals().widgets.noninteractive.bg_stroke.color),
        ));
        if diagnostic {
            let index = first
                .map(|first| (first + sector as u16).to_string())
                .unwrap_or_else(|| "?".into());
            let position = point(center, radius * 0.74, sector as f32);
            // Pealayer hardware-control indicators use fixed paint slots; these
            // neutral badges also keep diagnostic text independent of RGB fill.
            let badge = Rect::from_center_size(position, egui::vec2(22.0, 11.0));
            painter.rect_filled(badge, 3.0, Color32::from_black_alpha(210));
            painter.text(
                position,
                egui::Align2::CENTER_CENTER,
                index,
                egui::FontId::monospace(8.0),
                Color32::WHITE,
            );
        }
    }
    let line = if selected {
        ui.visuals().warn_fg_color
    } else if focus {
        ui.visuals().selection.stroke.color
    } else if response.hovered() || response.is_pointer_button_down_on() {
        ui.visuals().widgets.hovered.bg_stroke.color
    } else {
        ui.visuals().widgets.noninteractive.bg_stroke.color
    };
    let width = if selected || focus || response.hovered() {
        2.0
    } else {
        1.0
    };
    let width = if animate {
        ui.ctx()
            .animate_value_with_time(response.id.with("pixel-width"), width, 0.14)
    } else {
        width
    };
    let stroke = Stroke::new(
        width,
        super::cell_table::animated_color(ui, response.id.with("pixel-line"), line, animate),
    );
    // Radius/stroke stay inside the fixed hit slot, selection never recolors RGB.
    if shape == Shape::Circle {
        painter.circle_stroke(center, radius, stroke);
    } else {
        painter.add(egui::Shape::closed_line(
            (0..6)
                .map(|i| point(center, radius, i as f32 - 0.5))
                .collect(),
            stroke,
        ));
    }
    let mut job = egui::text::LayoutJob::simple(
        label.into(),
        egui::FontId::proportional(12.0),
        Color32::WHITE,
        radius * 1.1,
    );
    job.wrap.max_rows = 1;
    job.wrap.break_anywhere = true;
    let text = painter.layout_job(job);
    let badge = Rect::from_center_size(center, text.size() + egui::vec2(7.0, 4.0));
    painter.rect_filled(badge, 4.0, Color32::from_black_alpha(220));
    painter.galley(center - text.size() * 0.5, text, Color32::WHITE);
    if cell.is_none() {
        painter.text(
            center + egui::vec2(0.0, 14.0),
            egui::Align2::CENTER_CENTER,
            "RGB unavailable",
            egui::FontId::proportional(8.0),
            ui.visuals().weak_text_color(),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn transient_null_preserves_only_original_fresh_same_peer_sample() {
        let frame = Frame { rgb: "00ff00".repeat(480), physical: (1..=80).collect(), revision: 7, brightness: 255, pending:false };
        let mut slot = None;
        Sample::observe(&mut slot, Some(frame.clone()));
        let observed = slot.as_ref().unwrap().at;
        Sample::observe(&mut slot, None);
        assert_eq!(slot.as_ref().unwrap().at, observed);
        assert_eq!(slot.as_ref().unwrap().frame.revision, 7);
        slot.as_mut().unwrap().at = Instant::now() - Duration::from_secs(6);
        Sample::observe(&mut slot, None); assert!(slot.is_none());
        Sample::observe(&mut slot, Some(frame.clone()));
        let mut invalid = frame; invalid.physical[1] = 1;
        Sample::observe(&mut slot, Some(invalid)); assert!(slot.is_none());
    }
    #[test]
    fn pending_rgb_never_renews_sample_or_acknowledges_a_matching_probe() {
        let probe = crate::pixel_diagnostic::Probe { index: 474, group: true, rgb: 0x1234ab };
        let mut frame = Frame { rgb: probe.frame(false).unwrap().iter().map(|b| format!("{b:02x}")).collect(),
            physical: (1..=80).collect(), revision: 9, brightness: 255, pending: false };
        let mut status: crate::DeviceStatus = serde_json::from_value(serde_json::json!({})).unwrap();
        status.pixel_frame = Some(frame.clone());
        assert!(probe.matches(&status, false));
        let mut slot = None; Sample::observe(&mut slot, Some(frame.clone()));
        let observed = slot.as_ref().unwrap().at;
        frame.pending = true; assert!(frame.structurally_valid()); assert!(!frame.valid());
        status.pixel_frame = Some(frame.clone()); assert!(!probe.matches(&status, false));
        Sample::observe(&mut slot, Some(frame.clone()));
        assert_eq!(slot.as_ref().unwrap().at, observed);
        slot.as_mut().unwrap().at = Instant::now() - Duration::from_secs(6);
        Sample::observe(&mut slot, Some(frame.clone())); assert!(slot.is_none());
        frame.rgb.replace_range(..2, "zz"); assert!(!frame.structurally_valid());
        let mut fresh = status.pixel_frame.unwrap(); fresh.pending = false;
        Sample::observe(&mut slot, Some(fresh)); assert!(slot.is_some());
        Sample::observe(&mut slot, Some(frame)); assert!(slot.is_none());
    }
    #[test]
    fn top_origin_counter_clockwise_and_physical_mapping() {
        let contract: serde_json::Value =
            serde_json::from_str(include_str!("../../../assets/cells/pixel-contract.json"))
                .unwrap();
        let c = Pos2::ZERO;
        for (sector, center) in contract["ccw_unit_centers"]
            .as_array()
            .unwrap()
            .iter()
            .enumerate()
        {
            let p = point(c, 1.0, sector as f32);
            assert!((f64::from(p.x) - center[0].as_f64().unwrap()).abs() < 0.00001);
            assert!((f64::from(p.y) - center[1].as_f64().unwrap()).abs() < 0.00001);
        }
        assert!((point(c, 1.0, 0.0).y + 1.0).abs() < 0.001);
        assert!(point(c, 1.0, 1.0).x < 0.0);
        let mut f = Frame {
            rgb: "000000".repeat(480),
            physical: (1..=80).collect(),
            revision: 1,
            brightness: 255,
            pending: false,
        };
        f.physical.swap(0, 79);
        f.rgb.replace_range(
            474 * 6..480 * 6,
            &contract["colors"]
                .as_array()
                .unwrap()
                .iter()
                .map(|v| v.as_str().unwrap())
                .collect::<String>(),
        );
        let (first, colors) = f.cell(1).unwrap();
        assert_eq!(first, 474);
        assert_eq!(colors[0], Color32::from_rgb(18, 52, 171));
        f.physical[1] = 80;
        assert!(!f.valid());
        assert!(f.cell(1).is_none());
    }
}
