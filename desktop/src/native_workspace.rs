//! Durable presentation only, adapted from Pealayer capture/apply_workspace_profile.
//! No status, boot lease, credentials, pending command or operation is serialized.
use crate::{PakarControllerApp, ui};
use eframe::egui;
use serde::{Deserialize, Serialize};
use std::time::{Duration, Instant};

const LIMIT: usize = 16;
const MEMORY_LIMIT: usize = 128 * 1024;
const LAYOUT_LIMIT: usize = 64 * 1024;

pub(crate) fn default_key_positions() -> [[f32; 2]; 9] {
    std::array::from_fn(|index| [(index % 3) as f32 / 2.0, (index / 3) as f32 / 2.0])
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Identity {
    board: String,
    application: String,
}
impl Identity {
    fn observed(status: &crate::DeviceStatus) -> Option<Self> {
        crate::peer_http::Pin::from_status(status)?;
        let manifest = status.capabilities.as_ref().filter(|m| m.known())?;
        Some(Self {
            board: status.board_id.clone(),
            application: manifest
                .application()
                .map(|app| app.id.clone())
                .unwrap_or_else(|| format!("board-kind:{}", manifest.board_kind)),
        })
    }
    fn valid(&self) -> bool {
        !self.board.is_empty()
            && self.board.len() <= 64
            && self
                .board
                .bytes()
                .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'-' || c == b'_')
            && !self.application.trim().is_empty()
            && self.application.len() <= 128
            && !self.application.chars().any(char::is_control)
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Snapshot {
    pub layout: String,
    pub layout_revision: u8,
    pub hidden_panels: Option<Vec<ui::layout::Panel>>,
    pub memory: Option<String>,
    pub selection: Vec<u8>,
    pub selected_cell: u8,
    pub find: String,
    pub cell_effect: String,
    pub display_text: String,
    pub display_value: i16,
    pub buzzer_frequency: u16,
    pub buzzer_duration: u16,
    pub key_positions: [[f32; 2]; 9],
    pub settings_page: usize,
    pub settings_search: String,
    pub pixel_probe: crate::pixel_diagnostic::Probe,
}
impl Default for Snapshot {
    fn default() -> Self {
        Self {
            layout: String::new(),
            layout_revision: 0,
            hidden_panels: None,
            memory: None,
            selection: Vec::new(),
            selected_cell: 1,
            find: String::new(),
            cell_effect: "fade".into(),
            display_text: "PAKAR CONTROLLER".into(),
            display_value: 1,
            buzzer_frequency: 1032,
            buzzer_duration: 100,
            key_positions: default_key_positions(),
            settings_page: 0,
            settings_search: String::new(),
            pixel_probe: Default::default(),
        }
    }
}
impl Snapshot {
    fn normalize(&mut self) {
        if !self.pixel_probe.valid() {
            self.pixel_probe = Default::default();
        }
        if let Some(hidden) = &mut self.hidden_panels {
            *hidden = ui::layout::Panel::ALL
                .into_iter()
                .filter(|p| hidden.contains(p))
                .collect();
        }
        self.selection.retain(|id| (1..=80).contains(id));
        self.selection.sort_unstable();
        self.selection.dedup();
        self.selected_cell = self.selected_cell.clamp(1, 80);
        // Find is one input adapter for the same selection. Preserve an invalid
        // in-progress query, but never restore a second, conflicting highlight.
        if ui::cell_search::parse(&self.find).is_ok() {
            self.find =
                ui::cell_search::format_selection(&self.selection.iter().copied().collect());
        }
        if self.find.len() > 256 {
            self.find.clear();
        }
        if !["immediate", "fade", "pulse"].contains(&self.cell_effect.as_str()) {
            self.cell_effect = "fade".into();
        }
        if self.display_text.len() > 256 {
            self.display_text = self.display_text.chars().take(128).collect();
        }
        if self.settings_search.len() > 256 {
            self.settings_search.clear();
        }
        self.settings_page = self.settings_page.min(31);
        if self.layout.len() > LAYOUT_LIMIT {
            self.layout.clear();
        }
        if self
            .memory
            .as_ref()
            .is_some_and(|value| value.len() > MEMORY_LIMIT)
        {
            self.memory = None;
        }
        self.buzzer_frequency = self.buzzer_frequency.clamp(20, 20000);
        self.buzzer_duration = self.buzzer_duration.clamp(1, 60000);
        for (index, position) in self.key_positions.iter_mut().enumerate() {
            if !position[0].is_finite() || !position[1].is_finite() {
                *position = default_key_positions()[index];
            } else {
                position[0] = position[0].clamp(0.0, 1.0);
                position[1] = position[1].clamp(0.0, 1.0);
            }
        }
    }
}
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct Entry {
    identity: Identity,
    snapshot: Snapshot,
}
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Store {
    entries: Vec<Entry>,
}
impl Store {
    pub fn normalize(&mut self) {
        let mut normalized = Self::default();
        for entry in std::mem::take(&mut self.entries) {
            normalized.remember(entry.identity, entry.snapshot);
        }
        *self = normalized;
    }
    fn get(&self, identity: &Identity) -> Option<&Snapshot> {
        self.entries
            .iter()
            .find(|entry| &entry.identity == identity)
            .map(|entry| &entry.snapshot)
    }
    fn remember(&mut self, identity: Identity, mut snapshot: Snapshot) -> bool {
        if !identity.valid() {
            return false;
        }
        snapshot.normalize();
        if self.get(&identity) == Some(&snapshot) {
            return false;
        }
        self.entries.retain(|entry| entry.identity != identity);
        if self.entries.len() >= LIMIT {
            self.entries.remove(0);
        }
        self.entries.push(Entry { identity, snapshot });
        true
    }
}

#[derive(Default)]
pub struct Runtime {
    pub layout: crate::workspace_layout::WorkspaceLayout,
    pub errors: std::cell::RefCell<std::collections::BTreeSet<String>>,
    pub events_policy: Option<(bool, Vec<String>)>,
    active: Option<Identity>,
    context: Option<egui::Context>,
    checked: Option<Instant>,
    dirty_since: Option<Instant>,
    written: Option<Instant>,
}
impl Runtime {
    fn changed(&mut self, now: Instant) {
        self.dirty_since.get_or_insert(now);
    }
    fn write_due(&self, now: Instant) -> bool {
        self.dirty_since
            .is_some_and(|at| now.saturating_duration_since(at) >= Duration::from_secs(1))
            && self
                .written
                .is_none_or(|at| now.saturating_duration_since(at) >= Duration::from_secs(5))
    }
    pub fn saved(&mut self) {
        self.dirty_since = None;
        self.written = Some(Instant::now());
    }
}

impl PakarControllerApp {
    fn capture_ui_workspace(&self, ctx: &egui::Context) -> Snapshot {
        let (layout, hidden) = self.workspace.layout.snapshot();
        Snapshot {
            // Persist the full logical workspace, never its offline/partial-ACK projection.
            layout,
            layout_revision: 1,
            hidden_panels: Some(hidden),
            memory: ctx
                .memory(|memory| serde_json::to_string(memory).ok())
                .filter(|value| value.len() <= MEMORY_LIMIT),
            selection: self.table.selection.cells(),
            selected_cell: self.selected_cell,
            find: self.cell_search.clone(),
            cell_effect: self.cell_effect.clone(),
            display_text: self.display_text.clone(),
            display_value: self.display_value,
            buzzer_frequency: self.buzzer_frequency,
            buzzer_duration: self.buzzer_duration,
            key_positions: self.key_positions,
            settings_page: self.device_settings.page,
            settings_search: self.device_settings.search.clone(),
            pixel_probe: self.pixel_diagnostic.probe(),
        }
    }
    pub(crate) fn remember_ui_workspace(&mut self) {
        let Some(identity) = self.workspace.active.clone() else {
            return;
        };
        let Some(ctx) = self.workspace.context.clone() else {
            return;
        };
        self.workspace.layout.observe(&self.dock);
        let snapshot = self.capture_ui_workspace(&ctx);
        if self.config.native_workspaces.remember(identity, snapshot) {
            self.workspace.changed(Instant::now());
        }
    }
    /// Called before changing the displayed dock, after actual fresh identity
    /// admission. Neither hostname, route, boot nor a cache can choose a session.
    pub(crate) fn prepare_ui_workspace(
        &mut self,
        ctx: &egui::Context,
        status: Option<&crate::DeviceStatus>,
    ) -> Option<Snapshot> {
        self.workspace.context = Some(ctx.clone());
        let next = status.and_then(Identity::observed)?;
        if self.workspace.active.as_ref() == Some(&next) {
            return None;
        }
        self.remember_ui_workspace();
        let snapshot = self
            .config
            .native_workspaces
            .get(&next)
            .cloned()
            .unwrap_or_default();
        self.workspace.errors.borrow_mut().clear();
        self.workspace.active = Some(next);
        Some(snapshot)
    }
    pub(crate) fn apply_ui_workspace(&mut self, ctx: &egui::Context, mut snapshot: Snapshot) {
        snapshot.normalize();
        let keys_hidden = snapshot
            .hidden_panels
            .as_ref()
            .is_some_and(|hidden| hidden.contains(&ui::layout::Panel::Keys));
        self.workspace.layout = crate::workspace_layout::WorkspaceLayout::restore(
            &snapshot.layout,
            snapshot.hidden_panels,
        );
        if snapshot.layout_revision == 0 {
            self.workspace.layout.migrate_legacy_slots();
        }
        let available: Vec<_> = ui::layout::Panel::ALL
            .into_iter()
            .filter(|p| p.available(self.presentation_context()))
            .collect();
        self.workspace
            .layout
            .synchronize(&mut self.dock, &available);
        // Focus controls without moving the user's saved splits. Keys already
        // has a canonical primary slot in new layouts, before projection.
        if self
            .presentation_context()
            .is_some_and(|c| c.role == "first-wins")
            && !keys_hidden
        {
            ui::layout::reveal(&mut self.dock, ui::layout::Panel::Keys);
        }
        if self.presentation_context().is_some_and(|c| c.table())
            && self.dock.find_tab(&ui::layout::Panel::Table).is_some()
        {
            ui::layout::reveal(&mut self.dock, ui::layout::Panel::Table);
        }
        self.workspace.events_policy = None;
        self.table.selection.group(snapshot.selection, false);
        self.selected_cell = snapshot.selected_cell;
        self.sync_cell_selection_search();
        self.cell_search = snapshot.find;
        self.cell_effect = snapshot.cell_effect;
        self.display_text = snapshot.display_text;
        self.display_value = snapshot.display_value;
        self.buzzer_frequency = snapshot.buzzer_frequency;
        self.buzzer_duration = snapshot.buzzer_duration;
        self.key_positions = snapshot.key_positions;
        self.device_settings.page = snapshot.settings_page;
        self.device_settings.search = snapshot.settings_search;
        self.pixel_diagnostic.set_probe(snapshot.pixel_probe);
        if let Some(observation) = self
            .pixel_diagnostic
            .observed
            .clone()
            .filter(|o| o.active && !o.off)
        {
            self.pixel_diagnostic.set_probe(observation.probe);
        }
        let memory = snapshot
            .memory
            .and_then(|text| serde_json::from_str::<egui::Memory>(&text).ok())
            .unwrap_or_default();
        ctx.memory_mut(|current| {
            // Current global preferences win; the stored memory contributes
            // only positions, scroll/collapse and superficial widget state.
            // A new peer with no snapshot must not inherit another peer's scroll.
            let options = current.options.clone();
            *current = memory;
            current.options = options;
        });
        ctx.request_repaint();
    }
    pub(crate) fn checkpoint_ui_workspace(&mut self, ctx: &egui::Context) {
        self.workspace.context = Some(ctx.clone());
        let now = Instant::now();
        if self
            .workspace
            .checked
            .is_none_or(|at| now.saturating_duration_since(at) >= Duration::from_secs(1))
        {
            self.workspace.checked = Some(now);
            self.remember_ui_workspace();
        }
        if self.workspace.write_due(now) {
            match self.config.save() {
                Ok(()) => self.workspace.saved(),
                Err(error) => {
                    self.workspace.written = Some(now);
                    self.message = error;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn status(board: &str, boot: char, app: &str) -> crate::DeviceStatus {
        crate::DeviceStatus {
            board_id: board.into(),
            boot_id: boot.to_string().repeat(32),
            http_peer_fencing: true,
            capabilities: Some(crate::capabilities::Manifest {
                board_kind: "table".into(),
                name: "Renamable caption".into(),
                acknowledged_features: vec!["table".into()],
                table: Some(crate::capabilities::Table {
                    columns: 10,
                    rows: 8,
                    pixels_per_cell: 6,
                }),
                application: Some(crate::identity::Application {
                    id: app.into(),
                    name: "Pakar".into(),
                    title: "Pakar table".into(),
                    ..Default::default()
                }),
                ..Default::default()
            }),
            ..Default::default()
        }
    }
    #[test]
    fn durable_workspace_identity_ignores_boot_and_caption_but_not_board_or_application() {
        let a = status("board-a", 'a', "table");
        let mut reboot = status("board-a", 'b', "table");
        reboot.hostname = "renamed".into();
        reboot.capabilities.as_mut().unwrap().name = "Another caption".into();
        assert_eq!(Identity::observed(&a), Identity::observed(&reboot));
        assert_ne!(
            Identity::observed(&a),
            Identity::observed(&status("board-b", 'a', "table"))
        );
        assert_ne!(
            Identity::observed(&a),
            Identity::observed(&status("board-a", 'a', "other-app"))
        );
        reboot.boot_id.clear();
        assert!(Identity::observed(&reboot).is_none());
    }
    #[test]
    fn workspace_roundtrip_preserves_unified_selection_drafts_and_scroll_without_authority() {
        let context = egui::Context::default();
        let scroll_id = egui::Id::new("workspace-scroll-fixture");
        let mut scroll = egui::scroll_area::State::default();
        scroll.offset = egui::vec2(123.0, 456.0);
        scroll.store(&context, scroll_id);
        let mut dock = ui::layout::create_initial_layout();
        dock.remove_tab(dock.find_tab(&ui::layout::Panel::Streaming).unwrap());
        ui::layout::reveal(&mut dock, ui::layout::Panel::Network);
        let mut store = Store::default();
        let identity = Identity::observed(&status("board-a", 'a', "table")).unwrap();
        let snapshot = Snapshot {
            selection: vec![3, 2, 1, 3],
            selected_cell: 2,
            find: "1,2,3".into(),
            pixel_probe: crate::pixel_diagnostic::Probe {
                index: 474,
                group: true,
                rgb: 0x1234ab,
                ..Default::default()
            },
            display_text: "Unsaved text".into(),
            cell_effect: "pulse".into(),
            key_positions: {
                let mut positions = default_key_positions();
                positions[3] = [0.18, 0.82];
                positions
            },
            layout: ui::layout::serialize(&dock).unwrap(),
            memory: context.memory(|memory| serde_json::to_string(memory).ok()),
            ..Default::default()
        };
        assert!(store.remember(identity.clone(), snapshot));
        let bytes = serde_json::to_vec(&store).unwrap();
        let restored: Store = serde_json::from_slice(&bytes).unwrap();
        let value = restored.get(&identity).unwrap();
        assert_eq!(value.selection, vec![1, 2, 3]);
        assert_eq!(value.find, "1–3");
        assert_eq!(value.selected_cell, 2);
        assert_eq!(value.display_text, "Unsaved text");
        assert_eq!(value.key_positions[3], [0.18, 0.82]);
        assert_eq!(
            value.pixel_probe,
            crate::pixel_diagnostic::Probe {
                index: 474,
                group: true,
                rgb: 0x1234ab,
                ..Default::default()
            }
        );
        let memory: egui::Memory = serde_json::from_str(value.memory.as_ref().unwrap()).unwrap();
        let restored_context = egui::Context::default();
        restored_context.memory_mut(|current| *current = memory);
        assert_eq!(
            egui::scroll_area::State::load(&restored_context, scroll_id)
                .unwrap()
                .offset,
            egui::vec2(123.0, 456.0)
        );
        let restored_dock = ui::layout::restore(&value.layout);
        assert!(
            restored_dock
                .find_tab(&ui::layout::Panel::Streaming)
                .is_none()
        );
        assert!(
            restored_dock
                .find_tab(&ui::layout::Panel::Network)
                .is_some()
        );
        assert_eq!(value.layout, ui::layout::serialize(&restored_dock).unwrap());
        let json = String::from_utf8(bytes).unwrap();
        for forbidden in [
            "boot_id",
            "http_peer_fencing",
            "password",
            "pending",
            "commands",
            "brightness",
        ] {
            assert!(!json.contains(forbidden));
        }
        assert!(!store.remember(identity, value.clone())); // Identical state causes no write.
    }
    #[test]
    fn same_peer_reboot_restores_only_its_workspace_and_other_peers_cannot_inherit_it() {
        let mut store = Store::default();
        let a = Identity::observed(&status("board-a", 'a', "table")).unwrap();
        let b = Identity::observed(&status("board-b", 'a', "table")).unwrap();
        store.remember(
            a.clone(),
            Snapshot {
                selection: vec![1, 21],
                find: "1,21".into(),
                ..Default::default()
            },
        );
        store.remember(
            b.clone(),
            Snapshot {
                selection: vec![80],
                find: "80".into(),
                ..Default::default()
            },
        );
        let roundtrip: Store =
            serde_json::from_slice(&serde_json::to_vec(&store).unwrap()).unwrap();
        let reboot = Identity::observed(&status("board-a", 'b', "table")).unwrap();
        assert_eq!(roundtrip.get(&reboot).unwrap().selection, vec![1, 21]);
        assert_eq!(roundtrip.get(&b).unwrap().selection, vec![80]);
        assert!(
            roundtrip
                .get(&Identity::observed(&status("board-a", 'a', "other-app")).unwrap())
                .is_none()
        );
    }
    #[test]
    fn workspace_store_is_bounded_and_malformed_cached_geometry_is_not_authority() {
        let mut store = Store::default();
        for n in 0..LIMIT + 3 {
            store.remember(
                Identity {
                    board: format!("board-{n}"),
                    application: "table".into(),
                },
                Snapshot {
                    selection: vec![0, 1, 80, 255],
                    selected_cell: 255,
                    find: "draft-".into(),
                    memory: Some("x".repeat(MEMORY_LIMIT + 1)),
                    ..Default::default()
                },
            );
        }
        assert_eq!(store.entries.len(), LIMIT);
        assert_eq!(store.entries[0].identity.board, "board-3");
        let value = &store.entries[0].snapshot;
        assert_eq!(value.selection, vec![1, 80]);
        assert_eq!(value.selected_cell, 80);
        assert_eq!(value.find, "draft-");
        assert!(value.memory.is_none());
        let old: crate::config::AppConfig = serde_json::from_str("{}").unwrap();
        assert!(old.native_workspaces.entries.is_empty());
    }
    #[test]
    fn workspace_debounce_is_bounded_and_success_clears_dirty_state() {
        let now = Instant::now();
        let mut runtime = Runtime::default();
        runtime.changed(now);
        assert!(!runtime.write_due(now));
        assert!(runtime.write_due(now + Duration::from_secs(1)));
        runtime.written = Some(now);
        assert!(!runtime.write_due(now + Duration::from_secs(4)));
        assert!(runtime.write_due(now + Duration::from_secs(5)));
        runtime.saved();
        assert!(!runtime.write_due(now + Duration::from_secs(10)));
    }
}
