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
impl Manifest {
    pub fn known(&self) -> bool {
        !self.board_kind.is_empty() && self.board_kind.len() <= 64
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
                    ("cell", 1, "presentation")
                    | ("snake", 3, "presentation") => true,
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
            "/api/cell"
            | "/api/cells/mapping"
            | "/api/game/reset"
            | "/api/snake/direction" => "table",
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
            "/api/network" | "/api/cloud" | "/api/cloud/reconnect" | "/api/auth" => "network",
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
        })).unwrap();
        assert!(m.known() && m.has("buzzer"));
        assert!(!m.has("display") && !m.table());
        assert!(m.allows("/api/buzzer", &[]));
        assert!(!m.allows("/api/not-advertised", &[]));
        assert!(serde_json::from_value::<Manifest>(serde_json::json!({
            "board_kind":"controller","acknowledged_features":false
        })).is_err());
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
        assert_eq!(m.modes().count(), 2); // No Idle or input/output ACK: no First wins.
        payload[31] = 3;
        assert_eq!(Manifest::from_usb(&payload).unwrap().modes().count(), 3);
        payload[29] = 2;
        assert!(Manifest::from_usb(&payload).is_none());
    }
}
