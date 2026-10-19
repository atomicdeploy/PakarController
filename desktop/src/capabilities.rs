//! Adapts PCController peripheral-availability.ts's ready AND advertised rule.
//! Like Pealayer board_info.rs, only the authoritative peer owns available UI.
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct Manifest {
    pub board_kind: String,
    pub name: String,
    pub hostname: String,
    pub application: Option<crate::identity::Application>,
    pub acknowledged_features: Vec<String>,
    pub modes: Vec<Mode>,
    pub table: Option<Table>,
    #[serde(deserialize_with = "read_led_output")]
    pub led_output: Option<LedOutput>,
}
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct LedOutput {
    pub backend: String,
    pub rmt: bool,
    pub dma: bool,
}
impl LedOutput {
    fn valid(&self) -> bool {
        matches!(
            (self.backend.as_str(), self.rmt, self.dma),
            ("gpio-static" | "fastled", false, false) | ("rmt-dma", true, true)
        )
    }
    pub fn label(&self) -> &'static str {
        match self.backend.as_str() {
            "rmt-dma" => "RMT + DMA",
            "gpio-static" => "GPIO · no RMT/DMA",
            _ => "FastLED · no RMT/DMA",
        }
    }
}
fn read_led_output<'de, D: serde::Deserializer<'de>>(d: D) -> Result<Option<LedOutput>, D::Error> {
    // Malformed/unknown optional metadata must not reject the entire board.
    let value = serde_json::Value::deserialize(d)?;
    Ok(serde_json::from_value::<LedOutput>(value)
        .ok()
        .filter(LedOutput::valid))
}
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct Mode {
    pub id: String,
    pub code: u8,
    pub label: String,
    pub kind: String,
}
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct Table {
    pub columns: u8,
    pub rows: u8,
    pub pixels_per_cell: u8,
}
/// Stable, session-scoped presentation only. This is never a live manifest or
/// command admission source. Adapted from Pealayer workspace_profiles.rs's
/// stable ID ownership; live controls retain PCController's ready AND advertised
/// rule below. A brief lease/individual peripheral ACK loss must not destroy the
/// user's dock, selection or scroll state.
#[derive(Debug, Clone, Default, Serialize)]
pub struct Presentation {
    route: Option<String>,
    board: Option<String>,
    boot: Option<String>,
    manifest: Option<Manifest>,
    #[serde(skip)]
    panel_heights: std::collections::HashMap<String, PanelExtent>,
}
#[derive(Debug, Clone, Copy)]
struct PanelExtent { width: f32, height: f32 }
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PresentationChange {
    pub peer_changed: bool,
    pub boot_changed: bool,
}
impl Presentation {
    pub fn context(&self) -> Option<&Manifest> { self.manifest.as_ref() }
    pub fn workspace_key(&self) -> Option<String> {
        self.manifest.as_ref()?;
        self.board.as_ref().map(|board| format!("board:{board}"))
    }
    /// Measured layout extent, never device data. During a brief observation
    /// loss, keep the scroll content tall enough that Unknown rows cannot clamp
    /// away the user's offset. A resize discards the old wrapping measurement.
    pub fn panel_min_height(&mut self, panel: &str, width: f32, measured: f32, live: bool) -> f32 {
        if self.board.is_none() || panel.len() > 64 || !width.is_finite() || width <= 0.0
            || !measured.is_finite() || measured < 0.0 { return 0.0; }
        if !self.panel_heights.contains_key(panel) && self.panel_heights.len() >= 8 { return 0.0; }
        let extent = self.panel_heights.entry(panel.to_owned())
            .or_insert(PanelExtent { width, height: 0.0 });
        if (extent.width - width).abs() >= 0.5 { *extent = PanelExtent { width, height: 0.0 }; }
        if live {
            extent.height = measured.min(4096.0);
            0.0 // Live content owns its actual size, including deliberate collapse.
        } else { extent.height }
    }
    pub fn update(&mut self, route: Option<&str>, status: Option<&crate::DeviceStatus>) -> PresentationChange {
        let mut change = PresentationChange::default();
        if route != self.route.as_deref() {
            change.peer_changed = self.board.is_some();
            *self = Self { route: route.map(str::to_owned), ..Default::default() };
        }
        if route.is_none() { return change; }
        let Some(status) = status else { return change; };
        // A received but invalid identity must not inherit a prior board's UI.
        if crate::peer_http::Pin::from_status(status).is_none() {
            change.peer_changed |= self.board.is_some();
            self.board = None; self.boot = None; self.manifest = None;
            self.panel_heights.clear();
            return change;
        }
        let same_board = self.board.as_deref() == Some(status.board_id.as_str());
        let same_boot = same_board && self.boot.as_deref() == Some(status.boot_id.as_str());
        change.peer_changed |= !same_board;
        change.boot_changed = same_board && !same_boot;
        if !same_boot { self.manifest = None; }
        if !same_board { self.panel_heights.clear(); }
        self.board = Some(status.board_id.clone());
        self.boot = Some(status.boot_id.clone());
        if let Some(current) = status.capabilities.as_ref().filter(|m| m.known()) {
            let mut shown = current.clone();
            if let Some(previous) = self.manifest.as_ref().filter(|m| m.board_kind == current.board_kind) {
                // Only capabilities actually ACKed on this selected route/boot
                // may retain a slot. They do not grant a fresh command lease.
                for feature in &previous.acknowledged_features {
                    if !shown.acknowledged_features.contains(feature) {
                        shown.acknowledged_features.push(feature.clone());
                    }
                }
                for mode in &previous.modes {
                    if !shown.modes.iter().any(|m| m.id == mode.id) { shown.modes.push(mode.clone()); }
                }
                if shown.table.is_none() { shown.table = previous.table.clone(); }
            }
            self.manifest = Some(shown);
        }
        change
    }
}
impl Manifest {
    pub fn led_output(&self) -> Option<&LedOutput> {
        self.led_output
            .as_ref()
            .filter(|v| self.has("leds") && v.valid())
    }
    pub fn known(&self) -> bool {
        !self.board_kind.is_empty()
            && self.board_kind.len() <= 64
            && !self.board_kind.chars().any(char::is_control)
    }
    pub fn application(&self) -> Option<&crate::identity::Application> {
        if self.known() {
            self.application.as_ref().filter(|a| a.valid())
        } else {
            None
        }
    }
    pub fn has(&self, feature: &str) -> bool {
        self.known() && self.acknowledged_features.iter().any(|v| v == feature)
    }
    pub fn table(&self) -> bool {
        self.has("table")
            && self.board_kind == "table"
            && self
                .table
                .as_ref()
                .is_some_and(|t| t.columns == 10 && t.rows == 8 && t.pixels_per_cell == 6)
    }
    pub fn modes(&self) -> impl Iterator<Item = &Mode> {
        self.modes.iter().filter(|m| {
            self.table()
                && match (m.id.as_str(), m.code, m.kind.as_str()) {
                    ("cell", 1, "presentation") => true,
                    ("first-wins", 2, "game") => self.has("inputs") && self.has("outputs"),
                    _ => false,
                }
        })
    }
    pub fn title(&self) -> String {
        if !self.known() {
            return "PakarController · Discovering capabilities".into();
        }
        let label = if self.name.trim().is_empty() {
            "Connected board"
        } else {
            self.name.trim()
        };
        let title = self
            .application()
            .map(|a| a.title.as_str())
            .unwrap_or("PakarController");
        if self.hostname.is_empty() {
            format!("{title} · {label}")
        } else {
            format!("{title} · {label} · {}.local", self.hostname)
        }
    }
    pub fn allows(&self, path: &str, form: &[(String, String)]) -> bool {
        let feature = match path {
            "/api/cells" => return self.table() && self.has("cell-batch"),
            "/api/cell" | "/api/cells/mapping" | "/api/game/reset" | "/api/snake/direction" => {
                "table"
            }
            "/api/mode" => {
                return self
                    .modes()
                    .any(|m| form.iter().any(|(k, v)| k == "value" && v == &m.id));
            }
            "/api/display" => {
                if form.iter().any(|(k, _)| k == "text") {
                    "text"
                } else {
                    "display"
                }
            }
            "/api/audio" | "/api/buzzer" => "buzzer",
            "/api/outputs" => "outputs",
            "/api/brightness" | "/api/effect" | "/api/led/output" => "leds",
            "/api/device/name" => "identity",
            "/api/status-rgb" => "status-rgb",
            "/api/network" | "/api/network/ethernet" | "/api/cloud" | "/api/cloud/reconnect" | "/api/auth" => "network",
            "/api/clients/report" => "telemetry",
            _ if path.starts_with("/api/avr/") => "isp",
            _ if path.starts_with("/api/firmware/") => "ota",
            _ => return false,
        };
        if feature == "table" {
            self.table()
        } else {
            self.has(feature)
        }
    }
    pub fn from_usb(payload: &[u8]) -> Option<Self> {
        if payload.len() < 38 || payload[29] != 0 {
            return None;
        }
        let mask = u16::from_le_bytes([payload[30], payload[31]]);
        let names = [
            "network",
            "telemetry",
            "leds",
            "streaming",
            "table",
            "display",
            "text",
            "buzzer",
            "inputs",
            "outputs",
            "settings",
            "isp",
            "ota",
            "identity",
            "cell-batch",
            "lcd",
        ];
        let mut value = Self {
            board_kind: if payload[33] == 1 {
                "table"
            } else {
                "controller"
            }
            .into(),
            name: "USB controller".into(),
            acknowledged_features: names
                .iter()
                .enumerate()
                .filter(|(bit, _)| mask & (1 << bit) != 0)
                .map(|(_, v)| (*v).into())
                .collect(),
            ..Default::default()
        };
        if payload[33] == 1 {
            value.table = Some(Table {
                columns: payload[34],
                rows: payload[35],
                pixels_per_cell: payload[36],
            });
        }
        for (code, id, label, kind) in [
            (1, "cell", "Cell selection", "presentation"),
            (2, "first-wins", "First wins", "game"),
            (3, "snake", "Snake", "presentation"),
        ] {
            if payload[32] & (1 << code) != 0 {
                value.modes.push(Mode {
                    id: id.into(),
                    code,
                    label: label.into(),
                    kind: kind.into(),
                });
            }
        }
        Some(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn presentation_status(board: &str, boot: &str, features: &[&str]) -> crate::DeviceStatus {
        crate::DeviceStatus { board_id: board.into(), boot_id: boot.repeat(32), http_peer_fencing: true,
            capabilities: Some(Manifest { board_kind: "table".into(), name: "First name".into(),
                acknowledged_features: features.iter().map(|v| (*v).into()).collect(),
                table: Some(Table { columns: 10, rows: 8, pixels_per_cell: 6 }), ..Default::default() }),
            ..Default::default() }
    }
    #[test]
    fn presentation_retains_only_prior_acks_without_reviving_live_authority() {
        let mut display = Presentation::default();
        assert!(display.context().is_none());
        let mut status = presentation_status("board-a", "a", &["table", "display"]);
        display.update(Some("USB:COM22"), Some(&status));
        let key = display.workspace_key();
        assert!(display.context().unwrap().table());
        assert!(!display.context().unwrap().has("buzzer"));
        assert_eq!(display.update(Some("USB:COM22"), None), PresentationChange::default());
        assert_eq!(display.workspace_key(), key);
        status.capabilities.as_mut().unwrap().name = "Renamed board".into();
        status.capabilities.as_mut().unwrap().acknowledged_features.clear();
        display.update(Some("USB:COM22"), Some(&status));
        assert_eq!(display.workspace_key(), key);
        assert!(display.context().unwrap().has("display"));
        assert!(!status.capabilities.as_ref().unwrap().has("display")); // Live command admission stays closed.
    }
    #[test]
    fn presentation_revokes_acks_on_boot_peer_route_or_explicit_disconnect() {
        let mut display = Presentation::default();
        let status = presentation_status("board-a", "a", &["table", "display"]);
        display.update(Some("USB:COM22"), Some(&status));
        let key = display.workspace_key();
        let reboot = presentation_status("board-a", "b", &["table"]);
        assert!(display.update(Some("USB:COM22"), Some(&reboot)).boot_changed);
        assert_eq!(display.workspace_key(), key); // Same board, same user workspace.
        assert!(!display.context().unwrap().has("display"));
        display.update(Some("Wi-Fi:http://ledboard.local"), None);
        assert!(display.context().is_none()); // Another route needs its own actual ACK.
        display.update(Some("Wi-Fi:http://ledboard.local"), Some(&status));
        assert_eq!(display.workspace_key(), key);
        let other = presentation_status("board-b", "a", &["buzzer"]);
        display.update(Some("Wi-Fi:http://ledboard.local"), Some(&other));
        assert_ne!(display.workspace_key(), key);
        assert!(!display.context().unwrap().has("display"));
        display.update(None, None);
        assert!(display.context().is_none() && display.workspace_key().is_none());
    }
    #[test]
    fn invalid_identity_never_inherits_or_creates_presentation_context() {
        let mut display = Presentation::default();
        let mut status = presentation_status("board-a", "a", &["table"]);
        display.update(Some("USB:COM22"), Some(&status));
        status.boot_id.clear();
        display.update(Some("USB:COM22"), Some(&status));
        assert!(display.context().is_none());
        display.update(Some("USB:COM22"), None);
        assert!(display.context().is_none());
    }
    #[test]
    fn measured_panel_height_preserves_scroll_only_for_same_board_panel_and_width() {
        let mut display = Presentation::default();
        let status = presentation_status("board-a", "a", &["table"]);
        display.update(Some("USB:COM22"), Some(&status));
        assert_eq!(display.panel_min_height("Table", 480.0, 1100.0, true), 0.0);
        assert_eq!(display.panel_min_height("Table", 480.0, 20.0, false), 1100.0);
        assert_eq!(display.panel_min_height("Hardware", 480.0, 20.0, false), 0.0);
        assert_eq!(display.panel_min_height("Table", 481.0, 20.0, false), 0.0);
        display.panel_min_height("Table", 481.0, 700.0, true);
        display.panel_min_height("Table", 481.0, 80.0, true); // Deliberate live collapse.
        assert_eq!(display.panel_min_height("Table", 481.0, 20.0, false), 80.0);
        let other = presentation_status("board-b", "a", &["table"]);
        display.update(Some("USB:COM22"), Some(&other));
        assert_eq!(display.panel_min_height("Table", 481.0, 20.0, false), 0.0);
        display.panel_min_height("Table", 481.0, 700.0, true);
        display.update(None, None);
        assert_eq!(display.panel_min_height("Table", 481.0, 20.0, false), 0.0);
    }
    #[test]
    fn measured_panel_height_is_finite_bounded_and_not_an_unbounded_cache() {
        let mut display = Presentation::default();
        let status = presentation_status("board-a", "a", &["table"]);
        display.update(Some("USB:COM22"), Some(&status));
        display.panel_min_height("Table", 480.0, 9000.0, true);
        assert_eq!(display.panel_min_height("Table", 480.0, 20.0, false), 4096.0);
        for width in [f32::NAN, f32::INFINITY, 0.0, -1.0] {
            assert_eq!(display.panel_min_height("Table", width, 20.0, false), 0.0);
        }
        assert_eq!(display.panel_min_height("Table", 480.0, f32::NAN, true), 0.0);
        for index in 0..32 { display.panel_min_height(&format!("panel-{index}"), 480.0, 800.0, true); }
        assert_eq!(display.panel_heights.len(), 8);
    }
    #[test]
    fn led_backend_capability_is_explicit_optional_and_survives_relay() {
        for (backend, rmt, dma) in [
            ("gpio-static", false, false),
            ("rmt-dma", true, true),
            ("fastled", false, false),
        ] {
            let json = serde_json::json!({"board_kind":"table","acknowledged_features":["leds"],
                "led_output":{"backend":backend,"rmt":rmt,"dma":dma,"future_optional":123}});
            let m: Manifest = serde_json::from_value(json).unwrap();
            let output = m.led_output().unwrap();
            assert_eq!(
                (output.backend.as_str(), output.rmt, output.dma),
                (backend, rmt, dma)
            );
            let relayed: Manifest =
                serde_json::from_value(serde_json::to_value(m).unwrap()).unwrap();
            assert_eq!(relayed.led_output().unwrap().dma, dma);
        }
        for raw in [
            serde_json::Value::Null,
            serde_json::json!({"backend":"rmt-dma"}),
            serde_json::json!({"backend":"rmt-dma","rmt":false,"dma":true}),
            serde_json::json!({"backend":"gpio-static","rmt":false,"dma":"false"}),
        ] {
            let m: Manifest = serde_json::from_value(serde_json::json!({"board_kind":"table",
                "acknowledged_features":["leds"],"led_output":raw}))
            .unwrap();
            assert!(m.known() && m.led_output().is_none());
        }
        assert!(Manifest::default().led_output().is_none());
        let old: Manifest = serde_json::from_value(
            serde_json::json!({"board_kind":"table","acknowledged_features":["leds"]}),
        )
        .unwrap();
        assert!(old.led_output().is_none());
    }
    #[test]
    fn no_profile_or_individual_ack_means_no_controls() {
        let mut m = Manifest::default();
        assert!(!m.has("display"));
        assert!(!m.table());
        assert_eq!(m.modes().count(), 0);
        m.board_kind = "controller".into();
        m.acknowledged_features = vec!["buzzer".into()];
        assert!(m.has("buzzer"));
        assert!(!m.has("display"));
        assert!(!m.has("text"));
        m.modes.push(Mode {
            id: "snake".into(),
            code: 3,
            kind: "presentation".into(),
            ..Default::default()
        });
        assert_eq!(m.modes().count(), 0);
    }
    #[test]
    fn old_serial_status_cannot_invent_a_table() {
        assert!(Manifest::from_usb(&[0; 29]).is_none());
    }
    #[test]
    fn living_manifest_tolerates_optional_fields_but_cannot_invent_capabilities() {
        let m: Manifest = serde_json::from_value(serde_json::json!({
            "board_kind":"controller", "acknowledged_features":["buzzer","future.optional"],
            "extra_optional":{"value":true}
        }))
        .unwrap();
        assert!(m.known() && m.has("buzzer"));
        assert!(!m.has("display") && !m.table());
        assert!(m.allows("/api/buzzer", &[]));
        assert!(!m.allows("/api/not-advertised", &[]));
        assert!(serde_json::from_value::<Manifest>(serde_json::json!({
            "board_kind":"controller","acknowledged_features":false
        }))
        .is_err());
    }
    #[test]
    fn serial_manifest_is_individual_and_table_modes_are_profile_scoped() {
        let mut payload = [0; 38];
        payload[29] = 0; // Reserved, never a selectable contract generation.
        payload[30] = 1 << 7; // Buzzer only, not Display or Text.
        payload[32] = 15;
        let m = Manifest::from_usb(&payload).unwrap();
        assert!(m.has("buzzer"));
        assert!(!m.has("display"));
        assert_eq!(m.modes().count(), 0);
        payload[30] |= 1 << 4;
        payload[33] = 1;
        payload[34..37].copy_from_slice(&[10, 8, 6]);
        let m = Manifest::from_usb(&payload).unwrap();
        assert!(m.table());
        assert!(!m.allows("/api/cells", &[]));
        payload[31] |= 1 << 6; // CellBatch bit14, individually acknowledged.
        assert!(Manifest::from_usb(&payload)
            .unwrap()
            .allows("/api/cells", &[]));
        assert_eq!(m.modes().count(), 1); // Snake is hidden; no input/output ACK: no First wins.
        payload[31] = 3;
        assert_eq!(Manifest::from_usb(&payload).unwrap().modes().count(), 2);
        payload[29] = 2;
        assert!(Manifest::from_usb(&payload).is_none());
    }
}
