//! Adapt Pealayer App::logic's valid, non-minimized geometry observation.
//! A Windows restore must not turn an iconic framebuffer into the normal size.
use eframe::egui;

#[derive(Clone, Default)]
struct Geometry {
    normal: Option<egui::Vec2>,
    recovering: bool,
}
impl Geometry {
    fn observe(&mut self, viewport: &egui::ViewportInfo) -> Option<egui::Vec2> {
        if viewport.visible() != Some(true) || viewport.fullscreen == Some(true) {
            return None;
        }
        let size = viewport.inner_rect?.size();
        if !size.is_finite() { return None; }
        // Pealayer WindowGeometry::is_valid: finite and bounded, not a minimized
        // or fullscreen sample. Keep real user sizing; do not force defaults.
        if (420.0..=16_384.0).contains(&size.x) && (300.0..=16_384.0).contains(&size.y) {
            self.recovering = false;
            if viewport.maximized != Some(true) { self.normal = Some(size); }
        } else if (size.x < 320.0 || size.y < 240.0) && !self.recovering {
            self.recovering = true;
            return Some(self.normal.unwrap_or(egui::vec2(1180.0, 760.0)));
        }
        None
    }
}
pub fn reconcile(ctx: &egui::Context) {
    let viewport = ctx.input(|i| i.viewport().clone());
    let id = egui::Id::new("pakar.normal-window-geometry");
    let restore = ctx.data_mut(|data| {
        let mut geometry = data.get_temp::<Geometry>(id).unwrap_or_default();
        let restore = geometry.observe(&viewport);
        data.insert_temp(id, geometry);
        restore
    });
    if let Some(size) = restore {
        ctx.send_viewport_cmd(egui::ViewportCommand::MinInnerSize(egui::vec2(900.0, 600.0)));
        ctx.send_viewport_cmd(egui::ViewportCommand::InnerSize(size));
        ctx.request_repaint();
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn viewport(size: egui::Vec2) -> egui::ViewportInfo {
        egui::ViewportInfo { inner_rect: Some(egui::Rect::from_min_size(egui::Pos2::ZERO, size)), minimized: Some(false), occluded: Some(false), ..Default::default() }
    }
    #[test]
    fn preserves_user_size_and_never_samples_minimized_or_fullscreen_geometry() {
        let mut state = Geometry::default();
        assert_eq!(state.observe(&viewport(egui::vec2(1000.0, 700.0))), None);
        let mut tiny = viewport(egui::vec2(79.0, 101.0));
        tiny.minimized = Some(true); assert_eq!(state.observe(&tiny), None);
        tiny.minimized = Some(false); tiny.fullscreen = Some(true); assert_eq!(state.observe(&tiny), None);
        tiny.fullscreen = Some(false);
        assert_eq!(state.observe(&tiny), Some(egui::vec2(1000.0, 700.0)));
        assert_eq!(state.observe(&tiny), None); // no resize storm while awaiting OS
        assert_eq!(state.observe(&viewport(egui::vec2(1000.0, 700.0))), None);
        assert_eq!(state.observe(&tiny), Some(egui::vec2(1000.0, 700.0)));
    }
    #[test]
    fn recovery_commands_work_without_paint_and_do_not_reveal_hidden_windows() {
        let ctx = egui::Context::default();
        let mut input = egui::RawInput::default();
        *input.viewports.get_mut(&egui::ViewportId::ROOT).unwrap() = viewport(egui::vec2(79.0, 101.0));
        let result = ctx.run_logic(&input, reconcile);
        assert!(result.viewport_commands.values().flatten().any(|c| matches!(c, egui::ViewportCommand::InnerSize(s) if *s == egui::vec2(1180.0, 760.0))));
        assert!(!result.viewport_commands.values().flatten().any(|c| matches!(c, egui::ViewportCommand::Visible(_) | egui::ViewportCommand::Focus)));
        input.viewports.get_mut(&egui::ViewportId::ROOT).unwrap().minimized = Some(true);
        assert!(ctx.run_logic(&input, reconcile).viewport_commands.is_empty());
    }
}
