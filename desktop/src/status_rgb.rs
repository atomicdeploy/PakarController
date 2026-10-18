//! Actual PCController StatusLedController/StatusLedMath owns animation on the
//! board. Pealayer status_bar mirrors emitted bytes; clients never synthesize
//! breathing or apply brightness a second time. Configuration is board-owned.
use serde::{Deserialize, Serialize};
#[derive(Clone, Deserialize)]
pub struct Observation {
    pub board_id: String,
    pub boot_id: String,
    pub revision: u32,
    pub status_rgb: crate::ui::status_bar::PhysicalStatusRgb,
}
impl Observation {
    pub fn parse(value: serde_json::Value) -> Option<Self> {
        if value["type"] != "status-rgb" {
            return None;
        }
        if !value["status_rgb"]["available"].is_boolean()
            || !value["status_rgb"]["have_output"].is_boolean()
        {
            return None;
        }
        let value: Self = serde_json::from_value(value).ok()?;
        (value.revision > 0
            && value.status_rgb.revision == value.revision
            && value.status_rgb.rgb.is_some())
        .then_some(value)
    }
}
pub fn observe(status: &mut crate::DeviceStatus, incoming: Observation) -> bool {
    if !status
        .capabilities
        .as_ref()
        .is_some_and(|c| c.has("status-rgb"))
        || !crate::peer_http::Pin::from_status(status)
            .is_some_and(|pin| pin.matches_identity(&incoming.board_id, &incoming.boot_id))
        || status
            .status_rgb
            .as_ref()
            .is_some_and(|old| incoming.revision <= old.revision)
    {
        return false;
    }
    status.status_rgb = Some(incoming.status_rgb);
    true
}
pub fn retain(status: &mut crate::DeviceStatus, previous: Option<&crate::DeviceStatus>) {
    if !status
        .capabilities
        .as_ref()
        .is_some_and(|c| c.has("status-rgb"))
    {
        return;
    }
    if let Some(old) = previous
        .filter(|old| old.board_id == status.board_id && old.boot_id == status.boot_id)
        .and_then(|old| old.status_rgb.as_ref())
        .filter(|old| {
            status
                .status_rgb
                .as_ref()
                .is_none_or(|new| old.revision > new.revision)
        })
    {
        status.status_rgb = Some(old.clone());
    }
}
#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
pub struct Configuration {
    pub enabled: bool,
    pub pin: i8,
    pub pin_confirmed: bool,
    pub brightness: u8,
}
impl Configuration {
    pub fn validate(&self) -> Result<(), String> {
        if (self.pin == -1 && !self.enabled && !self.pin_confirmed)
            || (matches!(self.pin, 38 | 48) && (!self.enabled || self.pin_confirmed))
        {
            Ok(())
        } else {
            Err("Choose GPIO38 or GPIO48 and explicitly confirm the onboard pixel before enabling it".into())
        }
    }
    pub fn form(&self) -> Result<Vec<(String, String)>, String> {
        self.validate()?;
        Ok(vec![
            ("enabled".into(), self.enabled.to_string()),
            ("pin".into(), self.pin.to_string()),
            ("pin_confirmed".into(), self.pin_confirmed.to_string()),
            ("brightness".into(), self.brightness.to_string()),
        ])
    }
    pub fn read(value: serde_json::Value) -> Result<Self, String> {
        let result: Self = serde_json::from_value(value)
            .map_err(|_| "RGB configuration is incomplete; refresh before editing")?;
        result.validate()?;
        Ok(result)
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rgb_configuration_never_invents_confirmation_or_uses_matrix_uart_pins() {
        assert!(
            Configuration::read(serde_json::json!({"enabled":false,"pin":48,"brightness":64}))
                .is_err()
        );
        let mut value = Configuration {
            enabled: true,
            pin: 48,
            pin_confirmed: false,
            brightness: 64,
        };
        assert!(value.form().is_err());
        value.pin_confirmed = true;
        assert!(value.form().is_ok());
        for pin in [4, 10, 17, 18, -1] {
            value.pin = pin;
            assert!(value.form().is_err());
        }
        value = Configuration {
            enabled: false,
            pin: -1,
            pin_confirmed: false,
            brightness: 0,
        };
        assert!(value.form().is_ok());
    }
    #[test]
    fn saved_ack_requires_exact_persisted_config_readback_not_optimistic_preview() {
        let value = Configuration {
            enabled: true,
            pin: 48,
            pin_confirmed: true,
            brightness: 64,
        };
        let request = crate::device_settings::Write::status_rgb(7, &value).unwrap();
        assert!(
            request
                .verify_ack(&serde_json::json!({"saved":true}))
                .is_ok()
        );
        assert!(
            request
                .verify_ack(&serde_json::json!({"updated":true}))
                .is_err()
        );
        let mut readback = serde_json::to_value(value).unwrap();
        assert!(request.verify_readback(&readback).is_err());
        readback["persisted"] = true.into();
        assert!(request.verify_readback(&readback).is_ok());
        readback["pin"] = 38.into();
        assert!(request.verify_readback(&readback).is_err());
    }
    #[test]
    fn compact_rgb_updates_only_the_matching_peer_and_newer_rgb_not_hardware_clock() {
        let mut s:crate::DeviceStatus=serde_json::from_value(serde_json::json!({"board_id":"board-a","boot_id":"a".repeat(32),"http_peer_fencing":true,"hostname":"ledboard",
            "command_clock_ms":100,"cells":["red"],"capabilities":{"board_kind":"controller","name":"Board","hostname":"ledboard","acknowledged_features":["status-rgb"]}})).unwrap();
        let event = |revision, boot: &str| {
            Observation::parse(serde_json::json!({"type":"status-rgb","board_id":"board-a","boot_id":boot,"revision":revision,"status_rgb":{"revision":revision,"available":true,"have_output":true,"rgb":[1,2,3]}})).unwrap()
        };
        assert!(observe(&mut s, event(3, &"a".repeat(32))));
        assert_eq!(s.command_clock_ms, Some(100));
        assert_eq!(s.cells, vec!["red"]);
        assert!(!observe(&mut s, event(3, &"a".repeat(32))));
        assert!(!observe(&mut s, event(4, &"b".repeat(32))));
    }
}
