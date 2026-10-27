//! Physical RGB sectors; never infer pixel values from cell/game state.
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

#[derive(Clone, Debug, Default, Deserialize, Serialize)]
pub struct Frame {
    pub rgb: String,
    pub physical: Vec<u8>,
    pub revision: u32,
    pub brightness: u8,
    #[serde(default)]
    pub pending: bool,
    #[serde(default)]
    pub group_layout_revision: Option<u32>,
    #[serde(default)]
    pub group_counts: Vec<u8>,
    #[serde(default)]
    pub group_starts: Vec<u16>,
}
impl Frame {
    pub fn structurally_valid(&self) -> bool {
        let groups = self.physical.len();
        let pixels = match groups {
            80 => 480,
            18 => 216,
            _ => return false,
        };
        self.rgb.len() == pixels * 6
            && self.rgb.bytes().all(|b| b.is_ascii_hexdigit())
            && (1..=groups as u8).all(|id| self.physical.iter().filter(|p| **p == id).count() == 1)
            && self.spans().is_some()
    }
    fn spans(&self) -> Option<Vec<(u16, u8)>> {
        let (capacity, count) = match self.physical.len() { 80 => (480, 6), 18 => (216, 12), _ => return None };
        if self.group_layout_revision.is_none() && self.group_counts.is_empty() && self.group_starts.is_empty() {
            return Some((0..self.physical.len()).map(|g| ((g * count) as u16, count as u8)).collect());
        }
        self.group_layout_revision?;
        if self.group_counts.len() != self.physical.len() || self.group_starts.len() != self.physical.len() { return None; }
        let mut next = 0_u16;
        for (&start, &count) in self.group_starts.iter().zip(&self.group_counts) {
            if start != next || count == 0 { return None; }
            next = next.checked_add(u16::from(count))?;
            if next > capacity { return None; }
        }
        Some(self.group_starts.iter().copied().zip(self.group_counts.iter().copied()).collect())
    }
    pub fn physical_span(&self, group: u8) -> Option<(u16, u8)> {
        self.spans()?.get(usize::from(group.checked_sub(1)?)).copied()
    }
    fn colors(&self, first: u16, count: u8) -> Option<Vec<Color32>> {
        (usize::from(first)..usize::from(first) + usize::from(count)).map(|pixel| {
            let rgb = u32::from_str_radix(self.rgb.get(pixel * 6..pixel * 6 + 6)?, 16).ok()?;
            Some(Color32::from_rgb((rgb >> 16) as u8, (rgb >> 8) as u8, rgb as u8))
        }).collect()
    }
    pub fn valid(&self) -> bool {
        !self.pending && self.structurally_valid()
    }
    pub fn key_group(&self, key: u8, group: u8) -> Option<Vec<Color32>> {
        if !(1..=9).contains(&key) || group > 1 || self.physical.len() != 18 || !self.valid() {
            return None;
        }
        let (first, count) = self.physical_span(self.physical[usize::from(key - 1) * 2 + usize::from(group)])?;
        self.colors(first, count)
    }
    pub fn cell(&self, id: u8) -> Option<(u16, Vec<Color32>)> {
        if self.physical.len() != 80 || !(1..=80).contains(&id) || !self.valid() {
            return None;
        }
        let (first, count) = self.physical_span(self.physical[id as usize - 1])?;
        Some((first, self.colors(first, count)?))
    }
}
pub struct Sample {
    pub frame: Frame,
    pub at: Instant,
}
/// Pixel-only pushes never replace status or renew command authority.
pub fn websocket_sample(text: &str, peer: &crate::peer_http::Pin) -> Option<Frame> {
    let value: serde_json::Value = serde_json::from_str(text).ok()?;
    if value["type"] != "pixel-frame" || value["pixel_preview_requested"] != true
        || !peer.matches_identity(value["board_id"].as_str()?, value["boot_id"].as_str()?) {
        return None;
    }
    let frame: Frame = serde_json::from_value(value["pixel_frame"].clone()).ok()?;
    frame.valid().then_some(frame)
}
impl Sample {
    pub fn fresh(&self) -> bool {
        self.at.elapsed() < Duration::from_secs(5)
    }
    /// Last-observed RGB is presentation only. The selected peer/map owner
    /// revokes it separately; callers must use current telemetry for actuation.
    pub fn observe(slot: &mut Option<Self>, frame: Option<Frame>) {
        Self::observe_at(slot, frame, Instant::now());
    }
    /// Retain capture-start time when a slow immutable USB snapshot completes.
    /// Null/staging observations neither erase the last presented frame nor
    /// renew its freshness. Malformed data still revokes the presentation.
    pub fn observe_at(slot: &mut Option<Self>, frame: Option<Frame>, at: Instant) {
        match frame {
            Some(frame) if frame.valid() => *slot = Some(Self { frame, at }),
            Some(frame) if !frame.structurally_valid() => *slot = None,
            _ => {}
        }
    }
}
/// A new semantic output invalidates the old RGB presentation, even if its
/// optional pixel frame has not arrived yet. Missing optional observations do
/// not invent an output change. Command verification still reads DeviceStatus.
pub fn semantic_changed(
    next: &crate::DeviceStatus,
    previous: Option<&crate::DeviceStatus>,
) -> bool {
    let Some(previous) = previous else {
        return true;
    };
    crate::peer_http::Pin::from_identity(&next.board_id, &next.boot_id)
        != crate::peer_http::Pin::from_identity(&previous.board_id, &previous.boot_id)
        || (!next.cells.is_empty() && next.cells != previous.cells)
        || (!next.mode.is_empty() && next.mode != previous.mode)
        || (!next.effect.is_empty() && next.effect != previous.effect)
        || (next.brightness != 0 && next.brightness != previous.brightness)
        || next.led_output["enabled"]
            .as_bool()
            .is_some_and(|enabled| Some(enabled) != previous.led_output["enabled"].as_bool())
        || next.led_output["diagnostic"]["revision"]
            .as_u64()
            .is_some_and(|revision| {
                Some(revision) != previous.led_output["diagnostic"]["revision"].as_u64()
            })
        || next.pixel_groups["revision"].as_u64().is_some_and(|revision|
            Some(revision) != previous.pixel_groups["revision"].as_u64())
}
/// Screen Y increases downward: subtracting angle gives CCW. Sector 0 is top.
pub fn point(center: Pos2, radius: f32, sector: f32) -> Pos2 {
    sector_point(center, radius, sector, 6)
}
fn sector_point(center: Pos2, radius: f32, sector: f32, count: usize) -> Pos2 {
    let angle = -std::f32::consts::FRAC_PI_2 - sector * std::f32::consts::TAU / count.max(1) as f32;
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
    let first = cell.as_ref().map(|(first, _)| *first).or_else(||
        physical.and_then(|group| sample.and_then(|frame| frame.physical_span(group).map(|s| s.0))));
    let count = cell.as_ref().map_or(6, |(_, colors)| colors.len());
    for sector in 0..count {
        let mut points = if count == 1 { Vec::new() } else { vec![center] };
        let steps = if count < 3 || shape == Shape::Circle { (48 / count).max(2) } else { 1 };
        for step in 0..=steps {
            points.push(sector_point(
                center,
                radius,
                sector as f32 - 0.5 + step as f32 / steps as f32,
                count,
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
            let position = sector_point(center, radius * 0.74, sector as f32, count);
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
    let line = super::cell_table::cell_outline_color(
        ui.visuals(), ui.visuals().widgets.noninteractive.bg_stroke.color,
        ui.visuals().panel_fill, selected, focus,
        response.hovered() || response.is_pointer_button_down_on(),
    );
    let width = if selected || focus {
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
    fn pixel_groups_frame_preserves_physical_mapping_and_variable_sector_count() {
        let mut frame=Frame{rgb:"112233".repeat(480),physical:(1..=80).rev().collect(),revision:9,brightness:255,
            group_layout_revision:Some(4),group_counts:vec![6;80],..Default::default()};
        frame.group_counts[0]=5;frame.group_counts[79]=7;
        let mut next=0;frame.group_starts=frame.group_counts.iter().map(|&count|{let start=next;next+=u16::from(count);start}).collect();
        assert!(frame.valid());assert_eq!(frame.cell(80).unwrap().1.len(),5);
        assert_eq!(frame.cell(1).unwrap().0,473);assert_eq!(frame.cell(1).unwrap().1.len(),7);
        frame.group_starts[1]=6;assert!(!frame.valid());
        frame.group_starts[1]=5;frame.group_layout_revision=None;assert!(!frame.valid());
    }
    #[test]
    fn pixel_preview_hover_preserves_rgb_and_fixed_outline_width() {
        let frame = Frame {
            rgb: "12ab34".repeat(480), physical: (1..=80).collect(),
            revision: 1, brightness: 255, pending: false,
            ..Default::default()
        };
        for dark in [false, true] {
            let ctx = egui::Context::default();
            let mut visuals = if dark { egui::Visuals::dark() } else { egui::Visuals::light() };
            super::super::palette::apply(&mut visuals, super::super::palette::Palette::Studio);
            ctx.set_visuals(visuals);
            let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(64.0, 64.0));
            let mut edges = Vec::new();
            for hover in [false, true] {
                let mut output = ctx.run_ui(egui::RawInput {
                    events: if hover { vec![egui::Event::PointerMoved(rect.center())] } else { vec![] },
                    ..Default::default()
                }, |ui| {
                    let response = ui.interact(rect, egui::Id::new("pixel-hover"), egui::Sense::click());
                    draw(ui, &response, Shape::Circle, false, false, Some(&frame), Some(1), 1, "1", Color32::GRAY, false);
                    assert_eq!(response.rect, rect);
                });
                output.textures_delta.clear();
                let sectors: Vec<_> = output.shapes.iter().filter_map(|shape| match &shape.shape {
                    egui::Shape::Path(path) => Some(path.fill), _ => None,
                }).collect();
                assert_eq!(sectors, vec![Color32::from_rgb(0x12, 0xab, 0x34); 6]);
                let edge = output.shapes.iter().find_map(|shape| match &shape.shape {
                    egui::Shape::Circle(circle) => Some(circle.stroke), _ => None,
                }).unwrap();
                assert_eq!(edge.width, 1.0);
                edges.push(edge.color);
            }
            assert_ne!(edges[0], edges[1], "hover is visible without mutating sampled RGB");
        }
    }
    #[test]
    fn websocket_pixels_require_same_boot_and_valid_presented_frame() {
        let peer = crate::peer_http::Pin::from_identity("001122334455", &"a".repeat(32)).unwrap();
        let mut packet = serde_json::json!({"type":"pixel-frame","board_id":"001122334455","boot_id":"a".repeat(32),"pixel_preview_requested":true,
            "pixel_frame":{"rgb":"00ff00".repeat(216),"physical":(1..=18).collect::<Vec<_>>(),"revision":7,"brightness":255,"pending":false}});
        assert!(websocket_sample(&packet.to_string(), &peer).is_some());
        packet["boot_id"] = "b".repeat(32).into();
        assert!(websocket_sample(&packet.to_string(), &peer).is_none());
        packet["boot_id"] = "a".repeat(32).into();
        packet["pixel_frame"]["pending"] = true.into();
        assert!(websocket_sample(&packet.to_string(), &peer).is_none());
        packet["pixel_frame"]["pending"] = false.into();
        packet["pixel_frame"]["rgb"] = "bad".into();
        assert!(websocket_sample(&packet.to_string(), &peer).is_none());
    }
    #[test]
    fn transient_null_preserves_last_observed_sample_without_renewing_freshness() {
        let frame = Frame {
            rgb: "00ff00".repeat(480),
            physical: (1..=80).collect(),
            revision: 7,
            brightness: 255,
            pending: false,
            ..Default::default()
        };
        let mut slot = None;
        Sample::observe(&mut slot, Some(frame.clone()));
        let observed = slot.as_ref().unwrap().at;
        Sample::observe(&mut slot, None);
        assert_eq!(slot.as_ref().unwrap().at, observed);
        assert_eq!(slot.as_ref().unwrap().frame.revision, 7);
        let old = Instant::now() - Duration::from_secs(6);
        slot.as_mut().unwrap().at = old;
        Sample::observe(&mut slot, None);
        assert_eq!(slot.as_ref().unwrap().at, old);
        assert!(!slot.as_ref().unwrap().fresh());
        Sample::observe(&mut slot, Some(frame.clone()));
        let mut invalid = frame;
        invalid.physical[1] = 1;
        Sample::observe(&mut slot, Some(invalid));
        assert!(slot.is_none());
    }
    #[test]
    fn usb_capture_time_is_not_replaced_by_chunk_completion_time() {
        let frame = Frame {
            rgb: "00ff00".repeat(480),
            physical: (1..=80).collect(),
            revision: 7,
            brightness: 255,
            pending: false,
            ..Default::default()
        };
        let captured = Instant::now() - Duration::from_secs(6);
        let mut slot = None;
        Sample::observe_at(&mut slot, Some(frame), captured);
        assert_eq!(slot.as_ref().unwrap().at, captured);
        assert!(!slot.as_ref().unwrap().fresh());
        Sample::observe_at(&mut slot, None, Instant::now());
        assert_eq!(slot.as_ref().unwrap().at, captured);
    }
    #[test]
    fn semantic_changes_invalidate_rgb_but_omitted_optional_metadata_does_not() {
        let previous: crate::DeviceStatus = serde_json::from_value(serde_json::json!({
            "board_id":"board-a","boot_id":"a".repeat(32),"mode":"cell","effect":"none",
            "brightness":255,"cells":["white","red"],
            "led_output":{"enabled":true,"diagnostic":{"revision":7}}
        }))
        .unwrap();
        assert!(semantic_changed(&previous, None));
        assert!(!semantic_changed(&previous, Some(&previous)));
        for field in [
            "cells",
            "mode",
            "effect",
            "brightness",
            "enabled",
            "diagnostic",
            "boot",
            "board",
        ] {
            let mut next = previous.clone();
            match field {
                "cells" => next.cells[0] = "off".into(),
                "mode" => next.mode = "first-wins".into(),
                "effect" => next.effect = "rainbow".into(),
                "brightness" => next.brightness = 64,
                "enabled" => next.led_output["enabled"] = false.into(),
                "diagnostic" => next.led_output["diagnostic"]["revision"] = 8.into(),
                "boot" => next.boot_id = "b".repeat(32),
                "board" => next.board_id = "board-b".into(),
                _ => unreachable!(),
            }
            assert!(semantic_changed(&next, Some(&previous)), "{field}");
        }
        let mut omitted = previous.clone();
        omitted.led_output = serde_json::Value::Null;
        assert!(!semantic_changed(&omitted, Some(&previous)));
    }
    #[test]
    fn pending_rgb_never_renews_sample_or_acknowledges_a_matching_probe() {
        let probe = crate::pixel_diagnostic::Probe {
            index: 474,
            group: true,
            rgb: 0x1234ab,
            ..Default::default()
        };
        let mut frame = Frame {
            rgb: probe
                .frame(false)
                .unwrap()
                .iter()
                .map(|b| format!("{b:02x}"))
                .collect(),
            physical: (1..=80).collect(),
            revision: 9,
            brightness: 255,
            pending: false,
            ..Default::default()
        };
        let mut status: crate::DeviceStatus =
            serde_json::from_value(serde_json::json!({})).unwrap();
        status.pixel_frame = Some(frame.clone());
        assert!(probe.matches(&status, false));
        let mut slot = None;
        Sample::observe(&mut slot, Some(frame.clone()));
        let observed = slot.as_ref().unwrap().at;
        frame.pending = true;
        assert!(frame.structurally_valid());
        assert!(!frame.valid());
        status.pixel_frame = Some(frame.clone());
        assert!(!probe.matches(&status, false));
        Sample::observe(&mut slot, Some(frame.clone()));
        assert_eq!(slot.as_ref().unwrap().at, observed);
        let old = Instant::now() - Duration::from_secs(6);
        slot.as_mut().unwrap().at = old;
        Sample::observe(&mut slot, Some(frame.clone()));
        assert_eq!(slot.as_ref().unwrap().at, old);
        assert!(!slot.as_ref().unwrap().fresh());
        frame.rgb.replace_range(..2, "zz");
        assert!(!frame.structurally_valid());
        let mut fresh = status.pixel_frame.unwrap();
        fresh.pending = false;
        Sample::observe(&mut slot, Some(fresh));
        assert!(slot.is_some());
        Sample::observe(&mut slot, Some(frame));
        assert!(slot.is_none());
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
            ..Default::default()
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
