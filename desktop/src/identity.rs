use base64::Engine;
use serde::{Deserialize, Serialize};

pub fn valid_display_name(input: &str) -> bool {
    !input.is_empty()
        && input.len() <= 63
        && input.trim_matches(' ') == input
        && !input.chars().any(char::is_control)
}
#[derive(Debug, Clone, Default, Deserialize, Serialize, Hash)]
#[serde(default)]
pub struct Application {
    pub id: String,
    pub name: String,
    pub title: String,
    pub icon_png_base64: String,
}
impl Application {
    pub fn valid(&self) -> bool {
        [(&self.id, 96), (&self.name, 96), (&self.title, 160)]
            .iter()
            .all(|(s, max)| {
                !s.trim().is_empty() && s.len() <= *max && !s.chars().any(char::is_control)
            })
    }
    pub fn icon_bytes(&self) -> Option<Vec<u8>> {
        if !self.valid() || self.icon_png_base64.len() > 4096 {
            return None;
        }
        let bytes = base64::engine::general_purpose::STANDARD
            .decode(&self.icon_png_base64)
            .ok()?;
        if bytes.len() < 33 || bytes[..8] != *b"\x89PNG\r\n\x1a\n" || &bytes[12..16] != b"IHDR" {
            return None;
        }
        let width = u32::from_be_bytes(bytes[16..20].try_into().ok()?);
        let height = u32::from_be_bytes(bytes[20..24].try_into().ok()?);
        if !(8..=256).contains(&width) || !(8..=256).contains(&height) {
            return None;
        }
        Some(bytes)
    }
}
#[derive(Default)]
pub struct UsbRead {
    pub nonce: u8,
    pub revision: Option<u32>,
    pub total: usize,
    pub bytes: Vec<u8>,
}
impl UsbRead {
    pub fn request(&self) -> [u8; 3] {
        [
            self.nonce,
            self.bytes.len() as u8,
            (self.bytes.len() >> 8) as u8,
        ]
    }
    pub fn accept(&mut self, payload: &[u8]) -> Result<Option<serde_json::Value>, String> {
        if payload.len() < 10 || payload.len() > 48 || payload[0] != self.nonce {
            return Err("Stale/invalid identity chunk".into());
        }
        let revision = u32::from_le_bytes(payload[1..5].try_into().unwrap());
        let total = u16::from_le_bytes(payload[5..7].try_into().unwrap()) as usize;
        let offset = u16::from_le_bytes(payload[7..9].try_into().unwrap()) as usize;
        if total == 0
            || total > 8192
            || offset != self.bytes.len()
            || offset + payload.len() - 9 > total
            || self
                .revision
                .is_some_and(|r| r != revision || self.total != total)
        {
            return Err("Mixed or out-of-order identity response".into());
        }
        self.revision = Some(revision);
        self.total = total;
        self.bytes.extend_from_slice(&payload[9..]);
        if self.bytes.len() != total {
            return Ok(None);
        }
        let value: serde_json::Value =
            serde_json::from_slice(&self.bytes).map_err(|_| "Malformed identity JSON")?;
        if value["revision"].as_u64() != Some(revision as u64)
            || !value["hostname"]
                .as_str()
                .is_some_and(|v| normalize_name(v).is_ok())
            || !value["display_name"]
                .as_str()
                .is_some_and(valid_display_name)
        {
            return Err("Invalid identity readback".into());
        }
        Ok(Some(value))
    }
}
pub fn usb_write(
    hostname: &str,
    label: &str,
) -> Result<std::collections::VecDeque<Vec<u8>>, String> {
    let host = normalize_name(hostname)?;
    if !valid_display_name(label) {
        return Err("Invalid board display name".into());
    }
    let mut packets = std::collections::VecDeque::from([vec![0]]);
    for (tag, text) in [(1, host.as_str()), (3, label)] {
        for chunk in text.as_bytes().chunks(47) {
            let mut packet = vec![tag];
            packet.extend_from_slice(chunk);
            packets.push_back(packet);
        }
    }
    packets.push_back(vec![2]);
    Ok(packets)
}
pub fn normalize_name(input: &str) -> Result<String, String> {
    let lower = input.to_ascii_lowercase();
    let name = lower.strip_suffix(".local").unwrap_or(&lower);
    if name.is_empty()
        || name.len() > 63
        || name.starts_with('-')
        || name.ends_with('-')
        || matches!(name, "localhost" | "local")
        || !name
            .bytes()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'-')
    {
        return Err(
            "Use 1..63 letters/digits/hyphens, no leading/trailing hyphen; optional .local suffix"
                .into(),
        );
    }
    Ok(name.into())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn normalizes_dns_label_not_url() {
        assert_eq!(normalize_name("LEDBOARD.Local").unwrap(), "ledboard");
        assert_eq!(normalize_name("panel-2").unwrap(), "panel-2");
        assert!(normalize_name(&"a".repeat(63)).is_ok());
        assert!(normalize_name(&format!("{}.local", "a".repeat(63))).is_ok());
        for name in [
            "",
            "-x",
            "x-",
            "a.b",
            "http://ledboard.local",
            "x.local.local",
            "x_y",
            "x local",
            "local",
            "localhost",
            "é",
            "x\0y",
        ] {
            assert!(normalize_name(name).is_err(), "{name:?}");
        }
        assert!(normalize_name(&"a".repeat(64)).is_err());
    }
    #[test]
    fn board_and_firmware_identities_are_distinct_and_bounded() {
        assert!(valid_display_name("David’s table"));
        for name in ["", " leading", "trailing ", "line\nfeed", "\u{80}"] {
            assert!(!valid_display_name(name));
        }
        assert!(!valid_display_name(&"é".repeat(32)));
        let mut app = Application {
            id: "table.app".into(),
            name: "Table".into(),
            title: "Living room controller".into(),
            icon_png_base64: include_str!("../../assets/branding/app-icon.png.base64")
                .trim()
                .into(),
        };
        assert!(app.valid());
        assert!(app.icon_bytes().is_some());
        app.name = "bad\nname".into();
        assert!(!app.valid());
        assert!(app.icon_bytes().is_none());
        app.name = "Table".into();
        app.icon_png_base64 = "https://remote/icon.png".into();
        assert!(app.icon_bytes().is_none());
        let packets = usb_write("LEDBOARD.Local", "é".repeat(30).as_str()).unwrap();
        assert_eq!(packets.front().unwrap(), &vec![0]);
        assert_eq!(packets.back().unwrap(), &vec![2]);
        assert!(packets.iter().all(|p| p.len() <= 48));
    }
    #[test]
    fn usb_identity_assembly_rejects_mixed_offsets_revisions_and_replayed_nonce() {
        let value = serde_json::json!({"hostname":"ledboard","display_name":"Table","revision":17});
        let bytes = serde_json::to_vec(&value).unwrap();
        let mut reader = UsbRead {
            nonce: 9,
            ..Default::default()
        };
        for (index, chunk) in bytes.chunks(39).enumerate() {
            let offset = index * 39;
            let mut packet = vec![9];
            packet.extend(17_u32.to_le_bytes());
            packet.extend((bytes.len() as u16).to_le_bytes());
            packet.extend((offset as u16).to_le_bytes());
            packet.extend(chunk);
            let mut mixed = packet.clone();
            mixed[0] = 8;
            assert!(reader.accept(&mixed).is_err());
            if index > 0 {
                let mut mixed = packet.clone();
                mixed[1] = 18;
                assert!(reader.accept(&mixed).is_err());
            }
            let result = reader.accept(&packet).unwrap();
            assert_eq!(result.is_some(), offset + chunk.len() == bytes.len());
        }
    }
}
