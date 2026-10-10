//! Adapts PCController peripheral-availability.ts's ready AND advertised rule.
//! Like Pealayer board_info.rs, only the authoritative peer owns available UI.
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct Manifest {
    pub schema: u8,
    pub board_kind: String,
    pub name: String,
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
    pub fn has(&self, feature: &str) -> bool {
        self.schema == 1 && self.acknowledged_features.iter().any(|v| v == feature)
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
                    ("idle", 0, "presentation")
                    | ("cell", 1, "presentation")
                    | ("snake", 3, "presentation") => true,
                    ("first-wins", 2, "game") => self.has("inputs") && self.has("outputs"),
                    _ => false,
                }
        })
    }
    pub fn title(&self) -> String {
        if self.schema != 1 {
            return "PakarController · Discovering capabilities".into();
        }
        let label = if self.name.trim().is_empty() {
            "Connected board"
        } else {
            self.name.trim()
        };
        format!("PakarController · {label}")
    }
    pub fn allows(&self, path: &str, form: &[(String, String)]) -> bool {
        let feature = match path {
            "/api/v1/cell"
            | "/api/v1/cells/mapping"
            | "/api/v1/game/reset"
            | "/api/v1/snake/direction" => "table",
            "/api/v1/mode" => {
                return self
                    .modes()
                    .any(|m| form.iter().any(|(k, v)| k == "value" && v == &m.id));
            }
            "/api/v1/display" => {
                if form.iter().any(|(k, _)| k == "text") {
                    "text"
                } else {
                    "display"
                }
            }
            "/api/v1/audio" | "/api/v1/buzzer" => "buzzer",
            "/api/v1/outputs" => "outputs",
            "/api/v1/brightness" | "/api/v1/effect" => "leds",
            _ => return self.schema == 1,
        };
        if feature == "table" {
            self.table()
        } else {
            self.has(feature)
        }
    }
    pub fn from_usb(payload: &[u8]) -> Option<Self> {
        if payload.len() < 38 || payload[29] != 1 {
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
        ];
        let mut value = Self {
            schema: 1,
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
            (0, "idle", "Idle", "presentation"),
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
        m.schema = 1;
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
    fn serial_manifest_is_individual_and_table_modes_are_profile_scoped() {
        let mut payload = [0; 38];
        payload[29] = 1;
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
        assert_eq!(m.modes().count(), 3); // No input/output ACK: no First wins.
        payload[31] = 3;
        assert_eq!(Manifest::from_usb(&payload).unwrap().modes().count(), 4);
        payload[29] = 2;
        assert!(Manifest::from_usb(&payload).is_none());
    }
}
