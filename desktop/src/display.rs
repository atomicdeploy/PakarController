//! PCController scheduled-segment contract, shared by native, CLI and TUI.
use serde::{Deserialize, Serialize};

/// Last complete frame ACKed by the configured LCD backpack, not optical proof.
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct LcdObservation {
    pub configured: bool,
    pub started: bool,
    pub detection_known: bool,
    pub backpack_ack: bool,
    pub initialized: bool,
    pub available: bool,
    pub columns: u8,
    pub rows: u8,
    pub address: Option<u8>,
    pub lines: Option<Vec<String>>,
    pub pending: bool,
    pub requested_revision: u32,
    pub presented_revision: u32,
}
pub fn optional_lcd<'de, D: serde::Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<LcdObservation>, D::Error> {
    // A malformed optional display must hide only that display, not create
    // a spurious disconnect for the otherwise valid current board.
    let raw = serde_json::Value::deserialize(deserializer)?;
    Ok(serde_json::from_value(raw).ok())
}
impl LcdObservation {
    pub fn confirmed_lines(&self) -> Option<&[String]> {
        let lines = self.lines.as_ref()?;
        (self.configured
            && self.started
            && self.detection_known
            && self.backpack_ack
            && self.initialized
            && self.available
            && self.columns == 16
            && self.rows == 2
            && matches!(self.address, Some(0x27 | 0x3f))
            && lines.len() == 2
            && lines
                .iter()
                .all(|s| s.len() == 16 && s.bytes().all(|b| (0x20..=0x7e).contains(&b))))
        .then_some(lines.as_slice())
    }
}
#[cfg(test)]
mod lcd_tests {
    use super::*;
    #[test]
    fn lcd_requires_backpack_initialization_and_complete_current_frame() {
        let raw = serde_json::json!({"configured":true,"started":true,"detection_known":true,
            "backpack_ack":true,"initialized":true,"available":true,"columns":16,"rows":2,
            "address":39,"lines":["ledboard        ","Cell 1 white    "]});
        let observed: LcdObservation = serde_json::from_value(raw.clone()).unwrap();
        assert!(observed.confirmed_lines().is_some());
        for key in [
            "configured",
            "started",
            "detection_known",
            "backpack_ack",
            "initialized",
            "available",
        ] {
            let mut invalid = raw.clone();
            invalid[key] = false.into();
            assert!(
                serde_json::from_value::<LcdObservation>(invalid)
                    .unwrap()
                    .confirmed_lines()
                    .is_none()
            );
        }
        let mut invalid = raw;
        invalid["lines"] = serde_json::json!(["stale"]);
        assert!(
            serde_json::from_value::<LcdObservation>(invalid)
                .unwrap()
                .confirmed_lines()
                .is_none()
        );
        #[derive(Deserialize)]
        struct Envelope {
            #[serde(default, deserialize_with = "optional_lcd")]
            lcd: Option<LcdObservation>,
            hostname: String,
        }
        let valid: Envelope = serde_json::from_value(
            serde_json::json!({"hostname":"ledboard","lcd":{"lines":false}}),
        )
        .unwrap();
        assert_eq!(valid.hostname, "ledboard");
        assert!(valid.lcd.is_none());
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Repeat {
    #[default]
    Once,
    Loop,
    Interval,
}
impl Repeat {
    pub fn label(self) -> &'static str {
        match self {
            Self::Once => "Once",
            Self::Loop => "Loop",
            Self::Interval => "Interval",
        }
    }
    pub fn wire_name(self) -> &'static str {
        match self {
            Self::Once => "once",
            Self::Loop => "loop",
            Self::Interval => "interval",
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub speed_ms: u16,
    pub hold_ms: u16,
    pub repeat: Repeat,
    pub interval_seconds: u8,
    pub force_scroll: bool,
}
impl Default for Settings {
    fn default() -> Self {
        Self {
            speed_ms: 260,
            hold_ms: 1500,
            repeat: Repeat::Once,
            interval_seconds: 5,
            force_scroll: false,
        }
    }
}
impl Settings {
    pub fn payload(&self, text: &str) -> Result<Vec<u8>, String> {
        if text.len() > 40 || !text.bytes().all(|b| (0x20..=0x7e).contains(&b)) {
            return Err("Display text must be at most 40 printable ASCII characters".into());
        }
        if !(80..=5000).contains(&self.speed_ms)
            || self.hold_ms > 60000
            || (self.repeat == Repeat::Interval && self.interval_seconds == 0)
        {
            return Err("Invalid display timing".into());
        }
        let repeat = match self.repeat {
            Repeat::Once => 0,
            Repeat::Loop => 1,
            Repeat::Interval => 2,
        };
        let mut bytes = vec![
            5,
            self.speed_ms as u8,
            (self.speed_ms >> 8) as u8,
            text.len() as u8,
            repeat | if self.force_scroll { 0x80 } else { 0 },
            self.hold_ms as u8,
            (self.hold_ms >> 8) as u8,
            self.interval_seconds,
        ];
        bytes.extend_from_slice(text.as_bytes());
        Ok(bytes)
    }
    pub fn form(&self, text: &str) -> Vec<(String, String)> {
        vec![
            ("text".into(), text.into()),
            ("speed_ms".into(), self.speed_ms.to_string()),
            ("hold_ms".into(), self.hold_ms.to_string()),
            ("repeat".into(), self.repeat.wire_name().into()),
            ("force_scroll".into(), self.force_scroll.to_string()),
            ("interval_seconds".into(), self.interval_seconds.to_string()),
        ]
    }
}

pub const RELEASE: [u8; 4] = [4, 0, 0, 0];

pub fn parse_command(words: &[&str]) -> Result<(Settings, String), String> {
    let mut settings = Settings::default();
    let mut i = 0;
    while i < words.len() {
        let option = words[i];
        if option == "--" {
            let text = words[i + 1..].join(" ");
            settings.payload(&text)?;
            return Ok((settings, text));
        }
        if option == "--scroll" {
            settings.force_scroll = true;
            i += 1;
            continue;
        }
        let value = words.get(i + 1).ok_or("Missing display option value")?;
        match option {
            "--speed-ms" => settings.speed_ms = value.parse().map_err(|_| "Invalid speed")?,
            "--duration-ms" | "--hold-ms" => {
                settings.hold_ms = value.parse().map_err(|_| "Invalid hold")?
            }
            "--repeat" => {
                settings.repeat = match *value {
                    "once" => Repeat::Once,
                    "loop" => Repeat::Loop,
                    "interval" => Repeat::Interval,
                    _ => return Err("Repeat must be once, loop or interval".into()),
                }
            }
            "--interval-ms" => {
                let ms: u32 = value.parse().map_err(|_| "Invalid interval")?;
                if !(1000..=255000).contains(&ms) || ms % 1000 != 0 {
                    return Err("Interval must be whole seconds, 1000..255000 ms".into());
                }
                settings.interval_seconds = (ms / 1000) as u8;
            }
            _ => return Err(format!("Unknown display option: {option}")),
        }
        i += 2;
    }
    Err("Use display segments [options] -- TEXT".into())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn default_is_one_shot_even_for_long_text() {
        let s = Settings::default();
        assert_eq!(s.payload("PAKAR CONTROLLER").unwrap()[4], 0);
        assert_eq!(s.form("text")[3].1, "once");
    }
    #[test]
    fn explicit_repeat_and_scroll_are_wire_compatible() {
        let (s, text) = parse_command(&[
            "--repeat",
            "interval",
            "--interval-ms",
            "30000",
            "--scroll",
            "--",
            "HELLO",
            "WORLD",
        ])
        .unwrap();
        let p = s.payload(&text).unwrap();
        assert_eq!(p[4], 0x82);
        assert_eq!(p[7], 30);
        assert_eq!(&p[8..], b"HELLO WORLD");
    }
    #[test]
    fn invalid_requests_are_rejected() {
        for words in [
            vec!["--repeat", "oops", "--", "HI"],
            vec!["--speed-ms", "0", "--", "HI"],
            vec!["--interval-ms", "1500", "--", "HI"],
            vec!["--repeat"],
        ] {
            assert!(parse_command(&words).is_err());
        }
        assert!(Settings::default().payload("é").is_err());
        assert!(Settings::default().payload(&"a".repeat(41)).is_err());
    }
    #[test]
    fn old_preferences_get_safe_defaults() {
        let s: Settings = serde_json::from_str("{}").unwrap();
        assert_eq!(s.repeat, Repeat::Once);
        assert!(!s.force_scroll);
        assert_eq!(RELEASE, [4, 0, 0, 0]);
    }
}
