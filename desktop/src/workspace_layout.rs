//! Keep the user's dock separate from its capability-filtered projection.
//! Adapt Pealayer's stable workspace ownership and canonical tab restoration.
use crate::ui::layout::{self, Panel};
use egui_dock::DockState;

pub struct WorkspaceLayout {
    full: DockState<Panel>,
    hidden: Vec<Panel>,
    available: Vec<Panel>,
    rendered: Option<String>,
}
impl Default for WorkspaceLayout {
    fn default() -> Self { Self::restore("", None) }
}
fn fingerprint(dock: &DockState<Panel>) -> String {
    let mut copy = dock.clone();
    // Painted rectangles are derived from the viewport, not user layout edits.
    for (_, node) in copy.iter_all_nodes_mut() {
        node.set_rect(eframe::egui::Rect::ZERO);
        if let Some(leaf) = node.get_leaf_mut() { leaf.viewport = eframe::egui::Rect::ZERO; }
    }
    layout::serialize(&copy).unwrap_or_default()
}
impl WorkspaceLayout {
    pub fn restore(json: &str, hidden: Option<Vec<Panel>>) -> Self {
        let mut full = layout::restore(json);
        // Old snapshots conflated unavailable with closed. Only explicit new
        // visibility intent can suppress a default panel after this migration.
        for panel in Panel::ALL { layout::restore_canonical(&mut full, panel); }
        let mut unique = Vec::new();
        for panel in hidden.unwrap_or_default() { if !unique.contains(&panel) { unique.push(panel); } }
        Self { full, hidden: unique, available: Vec::new(), rendered: None }
    }
    pub fn snapshot(&self) -> (String, Vec<Panel>) {
        (layout::serialize(&self.full).unwrap_or_default(), self.hidden.clone())
    }
    pub fn status(&self) -> serde_json::Value {
        serde_json::json!({
            "retained_layout_sha256": crate::firmware_source::digest(fingerprint(&self.full).as_bytes()),
            "hidden_panels": self.hidden.iter().map(|p| p.name()).collect::<Vec<_>>(),
            "available_panels": self.available.iter().map(|p| p.name()).collect::<Vec<_>>(),
        })
    }
    fn projection(&self, available: &[Panel]) -> DockState<Panel> {
        let mut visible = self.full.clone();
        for panel in Panel::ALL {
            if !available.contains(&panel) || self.hidden.contains(&panel) {
                while let Some(path) = visible.find_tab(&panel) { visible.remove_tab(path); }
            }
        }
        visible
    }
    /// Observe user edits against the last projection, never against a new ACK
    /// inventory. Capability loss must not be interpreted as closing a panel.
    pub fn observe(&mut self, visible: &DockState<Panel>) {
        let next = fingerprint(visible);
        if self.rendered.as_ref().is_none_or(|old| *old == next) { return; }
        let before = self.projection(&self.available);
        for panel in &self.available {
            let was_open = before.find_tab(panel).is_some();
            let is_open = visible.find_tab(panel).is_some();
            if was_open && !is_open && !self.hidden.contains(panel) { self.hidden.push(*panel); }
            if is_open { self.hidden.retain(|p| p != panel); }
        }
        // A close-only edit retains even the hidden panel's original slot.
        if fingerprint(&self.projection(&self.available)) != next {
            let previous = self.full.clone();
            self.full = visible.clone();
            // During partial availability retain absent slots beside their
            // original siblings; use the donor canonical anchors otherwise.
            for panel in Panel::ALL {
                if self.full.find_tab(&panel).is_some() { continue; }
                let sibling = previous.find_tab(&panel).and_then(|path| previous.leaf(path.node_path()).ok())
                    .and_then(|leaf| leaf.tabs.iter().find_map(|sibling| self.full.find_tab(sibling)));
                if let Some(path) = sibling {
                    if let Ok(leaf) = self.full.leaf_mut(path.node_path()) { leaf.tabs.push(panel); }
                } else { layout::restore_canonical(&mut self.full, panel); }
            }
        }
        self.rendered = Some(next);
    }
    pub fn synchronize(&mut self, visible: &mut DockState<Panel>, available: &[Panel]) {
        self.observe(visible);
        if self.rendered.is_none() || self.available != available {
            *visible = self.projection(available);
            self.available = available.to_vec();
            self.rendered = Some(fingerprint(visible));
        }
    }
    pub fn show(&mut self, visible: &mut DockState<Panel>, panel: Panel) {
        self.observe(visible);
        self.hidden.retain(|p| *p != panel);
        *visible = self.projection(&self.available);
        layout::reveal(visible, panel);
        self.observe(visible);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reconnect_and_reboot_restore_splits_active_tabs_and_hidden_choices() {
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![Panel::Events]);
        layout.synchronize(&mut visible, &Panel::ALL);
        visible = DockState::new(vec![Panel::Table]);
        visible.main_surface_mut().split_right(egui_dock::NodeIndex::root(), 0.31,
            Panel::ALL.into_iter().filter(|p| *p != Panel::Table).collect());
        layout::reveal(&mut visible, Panel::Network);
        layout.observe(&visible);
        let custom = fingerprint(&visible);
        layout.synchronize(&mut visible, &[Panel::Events]);
        assert!(visible.find_tab(&Panel::Table).is_none());
        layout.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(fingerprint(&visible), custom);
        visible.remove_tab(visible.find_tab(&Panel::Streaming).unwrap());
        layout.observe(&visible);
        let (full, hidden) = layout.snapshot();
        assert_eq!(hidden, vec![Panel::Streaming]);
        let mut restarted = WorkspaceLayout::restore(&full, Some(hidden));
        restarted.synchronize(&mut visible, &[Panel::Events]);
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert!(visible.find_tab(&Panel::Table).is_some());
        assert!(visible.find_tab(&Panel::Streaming).is_none());
    }
    #[test]
    fn partial_ack_never_saves_a_missing_panel_as_hidden_or_flattens_default() {
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &[Panel::Table, Panel::Events]);
        layout.synchronize(&mut visible, &[Panel::Events]);
        let (full, hidden) = layout.snapshot();
        assert!(hidden.is_empty());
        let mut restarted = WorkspaceLayout::restore(&full, Some(hidden));
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(fingerprint(&visible), fingerprint(&layout::create_initial_layout()));
        assert!(visible.iter_leaves().count() >= 4);
    }
    #[test]
    fn painted_partial_workspace_does_not_destroy_temporarily_unavailable_slots() {
        struct Viewer;
        impl egui_dock::TabViewer for Viewer {
            type Tab = Panel;
            fn id(&mut self, tab: &mut Panel) -> eframe::egui::Id { eframe::egui::Id::new(*tab) }
            fn title(&mut self, tab: &mut Panel) -> eframe::egui::WidgetText { tab.name().into() }
            fn ui(&mut self, ui: &mut eframe::egui::Ui, tab: &mut Panel) { ui.label(tab.name()); }
        }
        let ctx = eframe::egui::Context::default();
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &Panel::ALL);
        let before = layout.status()["retained_layout_sha256"].clone();
        for available in [&[Panel::Events][..], &[Panel::Table, Panel::Events][..], &Panel::ALL[..]] {
            layout.synchronize(&mut visible, available);
            let _ = ctx.run_ui(Default::default(), |ui| {
                egui_dock::DockArea::new(&mut visible).show_inside(ui, &mut Viewer);
            });
            layout.observe(&visible);
        }
        assert_eq!(layout.status()["retained_layout_sha256"], before);
        assert!(layout.snapshot().1.is_empty());
    }
    #[test]
    fn empty_legacy_workspace_recovers_but_explicit_close_all_survives() {
        let mut layout = WorkspaceLayout::restore(&layout::serialize(&DockState::new(vec![Panel::Events])).unwrap(), None);
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(visible.iter_all_tabs().count(), Panel::ALL.len());
        for panel in Panel::ALL { visible.remove_tab(visible.find_tab(&panel).unwrap()); }
        layout.observe(&visible);
        let (full, hidden) = layout.snapshot();
        let mut restarted = WorkspaceLayout::restore(&full, Some(hidden));
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(visible.iter_all_tabs().count(), 0);
        restarted.show(&mut visible, Panel::Table);
        assert!(visible.find_tab(&Panel::Table).is_some());
        assert!(visible.find_tab(&Panel::Streaming).is_none());
    }
}
