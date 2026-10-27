//! Keep the user's dock separate from its capability-filtered projection.
//! Adapt Pealayer's stable workspace ownership and canonical tab restoration.
use crate::ui::layout::{self, Panel};
use egui_dock::DockState;

pub struct WorkspaceLayout {
    full: DockState<Panel>,
    hidden: Vec<Panel>,
    available: Vec<Panel>,
    rendered: Option<String>,
    events_override: Option<bool>,
}
impl Default for WorkspaceLayout {
    fn default() -> Self {
        Self::restore("", None)
    }
}
fn fingerprint(dock: &DockState<Panel>) -> String {
    let mut copy = dock.clone();
    // Painted rectangles are derived from the viewport, not user layout edits.
    for (_, node) in copy.iter_all_nodes_mut() {
        node.set_rect(eframe::egui::Rect::ZERO);
        if let Some(leaf) = node.get_leaf_mut() {
            leaf.viewport = eframe::egui::Rect::ZERO;
        }
    }
    layout::serialize(&copy).unwrap_or_default()
}
/// egui_dock persists active/focused/collapsed view state beside split topology.
/// Compare those separately: merely selecting a tab cannot rebuild hidden slots.
fn topology_fingerprint(dock: &DockState<Panel>) -> serde_json::Value {
    fn omit_view_state(value: &mut serde_json::Value) {
        match value {
            serde_json::Value::Object(fields) => {
                for field in [
                    "focused_surface",
                    "focused_node",
                    "active",
                    "collapsed",
                    "fully_collapsed",
                    "collapsed_leaf_count",
                    "scroll",
                    "tab_bar_hidden",
                ] {
                    fields.remove(field);
                }
                for child in fields.values_mut() {
                    omit_view_state(child);
                }
            }
            serde_json::Value::Array(items) => {
                for child in items {
                    omit_view_state(child);
                }
            }
            _ => {}
        }
    }
    let mut value = serde_json::from_str(&fingerprint(dock)).unwrap_or_default();
    omit_view_state(&mut value);
    value
}

/// Apply view edits by stable Panel identity rather than projection node index:
/// pruning a hidden leaf can renumber the visible tree. Unchanged projected tabs
/// must not replace a retained hidden active tab merely because another leaf got
/// focus. Pealayer reveal/canonical slot ownership remains the restoration path.
fn apply_view_edits(
    full: &mut DockState<Panel>,
    before: &DockState<Panel>,
    visible: &DockState<Panel>,
) {
    for (path, leaf) in visible.iter_leaves() {
        let Ok(previous) = before.leaf(path) else {
            continue;
        };
        let Some(anchor) = leaf.tabs.first().and_then(|panel| full.find_tab(panel)) else {
            continue;
        };
        if leaf.active != previous.active {
            if let Some(active) = leaf
                .tabs
                .get(leaf.active.0)
                .and_then(|panel| full.find_tab(panel))
            {
                let _ = full.set_active_tab(active);
            }
        }
        if let Ok(retained) = full.leaf_mut(anchor.node_path()) {
            if leaf.collapsed != previous.collapsed {
                retained.collapsed = leaf.collapsed;
            }
            if leaf.scroll != previous.scroll {
                retained.scroll = leaf.scroll;
            }
            if leaf.tab_bar_hidden != previous.tab_bar_hidden {
                retained.tab_bar_hidden = leaf.tab_bar_hidden;
            }
        }
    }
    if visible.focused_leaf() != before.focused_leaf() {
        if let Some(panel) = visible
            .focused_leaf()
            .and_then(|path| visible.leaf(path).ok())
            .and_then(|leaf| leaf.tabs.get(leaf.active.0))
            .copied()
        {
            layout::reveal(full, panel);
        }
    }
}
impl WorkspaceLayout {
    pub fn restore(json: &str, hidden: Option<Vec<Panel>>) -> Self {
        let mut full = layout::restore(json);
        // Old snapshots conflated unavailable with closed. Only explicit new
        // visibility intent can suppress a default panel after this migration.
        for panel in Panel::ALL {
            layout::restore_canonical(&mut full, panel);
        }
        let mut unique = Vec::new();
        for panel in hidden.unwrap_or_default() {
            if !unique.contains(&panel) {
                unique.push(panel);
            }
        }
        Self {
            full,
            hidden: unique,
            available: Vec::new(),
            rendered: None,
            events_override: None,
        }
    }
    pub fn snapshot(&self) -> (String, Vec<Panel>) {
        (
            layout::serialize(&self.full).unwrap_or_default(),
            self.hidden.clone(),
        )
    }
    /// Only for pre-revision snapshots; never reapply to a saved user layout.
    pub fn migrate_legacy_slots(&mut self) {
        for (panel, anchor) in [
            (Panel::Network, Panel::EffectControls),
            (Panel::Keys, Panel::Table),
        ] {
            if let (Some(old), Some(events), Some(target)) = (
                self.full.find_tab(&panel),
                self.full.find_tab(&Panel::Events),
                self.full.find_tab(&anchor),
            ) {
                if old.node_path() == events.node_path() && old.node_path() != target.node_path() {
                    self.full.remove_tab(old);
                    layout::restore_canonical(&mut self.full, panel);
                }
            }
        }
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
            let hidden = if panel == Panel::Events {
                self.events_override
                    .map(|show| !show)
                    .unwrap_or_else(|| self.hidden.contains(&panel))
            } else {
                self.hidden.contains(&panel)
            };
            if !available.contains(&panel) || hidden {
                while let Some(path) = visible.find_tab(&panel) {
                    visible.remove_tab(path);
                }
            }
        }
        visible
    }
    /// Observe user edits against the last projection, never against a new ACK
    /// inventory. Capability loss must not be interpreted as closing a panel.
    pub fn observe(&mut self, visible: &DockState<Panel>) {
        let next = fingerprint(visible);
        if self.rendered.as_ref().is_none_or(|old| *old == next) {
            return;
        }
        let before = self.projection(&self.available);
        for panel in &self.available {
            if *panel == Panel::Events && self.events_override.is_some() {
                continue;
            }
            let was_open = before.find_tab(panel).is_some();
            let is_open = visible.find_tab(panel).is_some();
            if was_open && !is_open && !self.hidden.contains(panel) {
                self.hidden.push(*panel);
            }
            if is_open {
                self.hidden.retain(|p| p != panel);
            }
        }
        let current_projection = self.projection(&self.available);
        // Close-only and view-only edits retain all hidden split fractions and
        // sibling order. Only actual topology edits use the reconstruction path.
        if topology_fingerprint(&current_projection) == topology_fingerprint(visible) {
            apply_view_edits(&mut self.full, &current_projection, visible);
        } else {
            let previous = self.full.clone();
            self.full = visible.clone();
            // During partial availability retain absent slots beside their
            // original siblings; use the donor canonical anchors otherwise.
            for panel in Panel::ALL {
                if self.full.find_tab(&panel).is_some() {
                    continue;
                }
                let sibling = previous
                    .find_tab(&panel)
                    .and_then(|path| previous.leaf(path.node_path()).ok())
                    .and_then(|leaf| {
                        leaf.tabs
                            .iter()
                            .find_map(|sibling| self.full.find_tab(sibling))
                    });
                if let Some(path) = sibling {
                    if let Ok(leaf) = self.full.leaf_mut(path.node_path()) {
                        leaf.tabs.push(panel);
                    }
                } else {
                    layout::restore_canonical(&mut self.full, panel);
                }
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
        if panel == Panel::Events {
            self.events_override = None;
        }
        self.hidden.retain(|p| *p != panel);
        *visible = self.projection(&self.available);
        layout::reveal(visible, panel);
        self.observe(visible);
    }
    /// Automatic error presentation never becomes persisted user visibility.
    pub fn events_visibility(&mut self, visible: &mut DockState<Panel>, show: Option<bool>) {
        self.observe(visible);
        self.events_override = show;
        *visible = self.projection(&self.available);
        if show == Some(true) {
            if let Some(path) = visible.find_tab(&Panel::Events) {
                let _ = visible.set_active_tab(path);
                if let Ok(node) = visible.node_mut(path.node_path()) {
                    node.set_collapsed(false);
                }
            }
        }
        self.rendered = Some(fingerprint(visible));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn migration_is_explicit_and_custom_network_placement_survives_restore() {
        let mut dock = layout::create_initial_layout();
        dock.remove_tab(dock.find_tab(&Panel::Network).unwrap());
        let events = dock.find_tab(&Panel::Events).unwrap();
        dock.leaf_mut(events.node_path())
            .unwrap()
            .tabs
            .push(Panel::Network);
        let json = layout::serialize(&dock).unwrap();
        let mut retained = WorkspaceLayout::restore(&json, Some(vec![]));
        assert_eq!(fingerprint(&retained.full), fingerprint(&dock));
        retained.migrate_legacy_slots();
        assert_eq!(
            retained.full.find_tab(&Panel::Network).unwrap().node_path(),
            retained
                .full
                .find_tab(&Panel::EffectControls)
                .unwrap()
                .node_path()
        );
    }
    #[test]
    fn automatic_events_do_not_change_saved_layout_or_hidden_intent() {
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &Panel::ALL);
        let saved = layout.snapshot();
        layout.events_visibility(&mut visible, Some(false));
        layout.observe(&visible);
        assert!(visible.find_tab(&Panel::Events).is_none());
        assert_eq!(layout.snapshot(), saved);
        layout.events_visibility(&mut visible, Some(true));
        layout.observe(&visible);
        assert!(visible.find_tab(&Panel::Events).is_some());
        assert_eq!(layout.snapshot(), saved);
        layout.events_visibility(&mut visible, Some(false));
        assert_eq!(layout.snapshot(), saved);
    }
    #[test]
    fn first_wins_default_keeps_controls_above_events_and_network_in_sidebar() {
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        layout.synchronize(
            &mut visible,
            &[
                Panel::Keys,
                Panel::EffectControls,
                Panel::Network,
                Panel::Events,
            ],
        );
        assert_ne!(
            visible.find_tab(&Panel::Keys).unwrap().node_path(),
            visible.find_tab(&Panel::Events).unwrap().node_path()
        );
        assert_eq!(
            visible.find_tab(&Panel::Network).unwrap().node_path(),
            visible
                .find_tab(&Panel::EffectControls)
                .unwrap()
                .node_path()
        );
        let saved = layout.snapshot();
        let mut restored = WorkspaceLayout::restore(&saved.0, Some(saved.1));
        restored.synchronize(
            &mut visible,
            &[
                Panel::Keys,
                Panel::EffectControls,
                Panel::Network,
                Panel::Events,
            ],
        );
        assert_ne!(
            visible.find_tab(&Panel::Keys).unwrap().node_path(),
            visible.find_tab(&Panel::Events).unwrap().node_path()
        );
    }
    #[test]
    fn reconnect_and_reboot_restore_splits_active_tabs_and_hidden_choices() {
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![Panel::Events]);
        layout.synchronize(&mut visible, &Panel::ALL);
        visible = DockState::new(vec![Panel::Table]);
        visible.main_surface_mut().split_right(
            egui_dock::NodeIndex::root(),
            0.31,
            Panel::ALL
                .into_iter()
                .filter(|p| *p != Panel::Table)
                .collect(),
        );
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
        assert_eq!(
            fingerprint(&visible),
            fingerprint(&layout::create_initial_layout())
        );
        assert!(visible.iter_leaves().count() >= 4);
    }
    #[test]
    fn painted_partial_workspace_does_not_destroy_temporarily_unavailable_slots() {
        struct Viewer;
        impl egui_dock::TabViewer for Viewer {
            type Tab = Panel;
            fn id(&mut self, tab: &mut Panel) -> eframe::egui::Id {
                eframe::egui::Id::new(*tab)
            }
            fn title(&mut self, tab: &mut Panel) -> eframe::egui::WidgetText {
                tab.name().into()
            }
            fn ui(&mut self, ui: &mut eframe::egui::Ui, tab: &mut Panel) {
                ui.label(tab.name());
            }
        }
        let ctx = eframe::egui::Context::default();
        let mut layout = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &Panel::ALL);
        let before = layout.status()["retained_layout_sha256"].clone();
        for available in [
            &[Panel::Events][..],
            &[Panel::Table, Panel::Events][..],
            &Panel::ALL[..],
        ] {
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
        let mut layout = WorkspaceLayout::restore(
            &layout::serialize(&DockState::new(vec![Panel::Events])).unwrap(),
            None,
        );
        let mut visible = DockState::new(vec![]);
        layout.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(visible.iter_all_tabs().count(), Panel::ALL.len());
        for panel in Panel::ALL {
            visible.remove_tab(visible.find_tab(&panel).unwrap());
        }
        layout.observe(&visible);
        let (full, hidden) = layout.snapshot();
        let mut restarted = WorkspaceLayout::restore(&full, Some(hidden));
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(visible.iter_all_tabs().count(), 0);
        restarted.show(&mut visible, Panel::Table);
        assert!(visible.find_tab(&Panel::Table).is_some());
        assert!(visible.find_tab(&Panel::Streaming).is_none());
    }
    #[test]
    fn focusing_with_hidden_events_and_streaming_retains_custom_slots_after_restart() {
        let mut full = DockState::new(vec![Panel::Keys, Panel::Table]);
        let [main, right] = full.main_surface_mut().split_right(
            egui_dock::NodeIndex::root(),
            0.63,
            Panel::ALL
                .into_iter()
                .filter(|panel| {
                    !matches!(
                        panel,
                        Panel::Keys | Panel::Table | Panel::Events | Panel::Streaming
                    )
                })
                .collect(),
        );
        full.main_surface_mut()
            .split_below(main, 0.37, vec![Panel::Streaming, Panel::Events]);
        full.node_mut(egui_dock::NodePath::new(
            egui_dock::SurfaceIndex::main(),
            right,
        ))
        .unwrap()
        .set_collapsed(true);
        let topology = topology_fingerprint(&full);
        let events = full.find_tab(&Panel::Events).unwrap();
        let original_events_tabs = full.leaf(events.node_path()).unwrap().tabs.clone();
        let json = layout::serialize(&full).unwrap();
        let hidden = vec![Panel::Events, Panel::Streaming];
        let mut retained = WorkspaceLayout::restore(&json, Some(hidden.clone()));
        let mut visible = DockState::new(vec![]);
        retained.synchronize(&mut visible, &Panel::ALL);
        retained.show(&mut visible, Panel::Table);
        retained.observe(&visible);
        assert_eq!(topology_fingerprint(&retained.full), topology);
        assert_eq!(retained.hidden, hidden);
        let saved = retained.snapshot();
        let mut restarted = WorkspaceLayout::restore(&saved.0, Some(saved.1));
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(topology_fingerprint(&restarted.full), topology);
        let table = restarted.full.find_tab(&Panel::Table).unwrap();
        assert_eq!(
            restarted.full.leaf(table.node_path()).unwrap().active,
            table.tab
        );
        assert_eq!(
            restarted
                .full
                .leaf(restarted.full.find_tab(&Panel::Events).unwrap().node_path())
                .unwrap()
                .tabs,
            original_events_tabs
        );
        restarted.show(&mut visible, Panel::Events);
        restarted.show(&mut visible, Panel::Streaming);
        assert_eq!(topology_fingerprint(&visible), topology);
    }
    #[test]
    fn view_only_collapse_and_tab_scroll_do_not_rebuild_hidden_splits() {
        let mut retained =
            WorkspaceLayout::restore("", Some(vec![Panel::Events, Panel::Streaming]));
        let mut visible = DockState::new(vec![]);
        retained.synchronize(&mut visible, &Panel::ALL);
        let topology = topology_fingerprint(&retained.full);
        let front = visible.find_tab(&Panel::FrontPanel).unwrap().node_path();
        let leaf = visible.leaf_mut(front).unwrap();
        leaf.collapsed = true;
        leaf.scroll = 22.0;
        retained.observe(&visible);
        assert_eq!(topology_fingerprint(&retained.full), topology);
        let front = retained
            .full
            .find_tab(&Panel::FrontPanel)
            .unwrap()
            .node_path();
        assert!(retained.full.leaf(front).unwrap().collapsed);
        assert_eq!(retained.full.leaf(front).unwrap().scroll, 22.0);
    }
    #[test]
    fn actual_split_and_tab_order_edits_still_persist() {
        let mut retained = WorkspaceLayout::default();
        let mut visible = DockState::new(vec![]);
        retained.synchronize(&mut visible, &Panel::ALL);
        let effects = visible
            .find_tab(&Panel::EffectControls)
            .unwrap()
            .node_path();
        visible.leaf_mut(effects).unwrap().tabs.reverse();
        for (_, node) in visible.iter_all_nodes_mut() {
            if let egui_dock::Node::Horizontal(split) = node {
                split.fraction = 0.57;
                break;
            }
        }
        let edited = topology_fingerprint(&visible);
        retained.observe(&visible);
        assert_eq!(topology_fingerprint(&retained.full), edited);
        let saved = retained.snapshot();
        let mut restarted = WorkspaceLayout::restore(&saved.0, Some(saved.1));
        restarted.synchronize(&mut visible, &Panel::ALL);
        assert_eq!(topology_fingerprint(&visible), edited);
    }
}
