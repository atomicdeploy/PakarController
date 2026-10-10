//! Explicit, non-replayed maintenance operations against the integrated firmware.
use crate::Endpoint;
use reqwest::blocking::{Client, multipart};
use std::{
    io::{Read, Write},
    path::PathBuf,
    time::Duration,
};

#[derive(Clone)]
pub enum Operation {
    ReconnectCloud,
    Probe,
    Backup,
    Download { eeprom: bool, destination: PathBuf },
    StageAvr(Vec<u8>),
    ProgramAvr,
    UpdateEsp(Vec<u8>),
}
impl Operation {
    pub fn warning(&self) -> &'static str {
        match self {
            Self::ReconnectCloud => {
                "Reconnect MQTT and WebSocket sessions using the stored configuration. Unsaved changes are not applied. Acceptance does not prove a successful broker connection."
            }
            Self::Probe => {
                "ISP probe resets and temporarily interrupts the AVR. Confirm wiring and power before proceeding."
            }
            Self::Backup => {
                "Backup resets the AVR and reads flash (including bootloader) and EEPROM into ESP storage. Existing on-device backups will be replaced."
            }
            Self::ProgramAvr => {
                "Program the CURRENT shared staged image, which another client may have replaced. This firmware backs up first, then erases/programs/verifies the AVR. The API does not expose a staged-image hash. Confirm image provenance and electrical readiness; never retry an uncertain result blindly."
            }
            Self::StageAvr(_) => {
                "Store this complete 32 KiB AVR binary (including the intended bootloader). Staging replaces the shared image but does not program the AVR."
            }
            Self::UpdateEsp(_) => {
                "Replace ESP firmware and reboot. Confirm this binary is built for this ESP8266 and preserves the device's pin mapping. A lost connection does not prove failure; reconnect before retrying."
            }
            Self::Download { .. } => {
                "Export the existing backup. No board write is performed and existing PC files will not be overwritten."
            }
        }
    }
}
pub fn validate_image(esp: bool, bytes: &[u8]) -> Result<(), String> {
    if esp {
        if bytes.len() < 16 || bytes.len() > 2 * 1024 * 1024 || bytes[0] != 0xe9 {
            return Err("Select an uncompressed ESP8266 .bin (E9 header, 16 bytes–2 MiB). Device flash capacity is validated by the updater.".into());
        }
    } else if bytes.len() != 32768 {
        return Err("AVR staging requires exactly 32768 bytes; Intel HEX and partial application images are not accepted".into());
    }
    Ok(())
}
pub fn run(client: &Client, endpoint: &Endpoint, operation: Operation) -> Result<String, String> {
    let (path, post) = match &operation {
        Operation::ReconnectCloud => ("/api/v1/cloud/reconnect", true),
        Operation::Probe => ("/api/v1/avr/probe", false),
        Operation::Backup => ("/api/v1/avr/backup", true),
        Operation::Download { eeprom: true, .. } => ("/api/v1/avr/backup/eeprom", false),
        Operation::Download { .. } => ("/api/v1/avr/backup/flash", false),
        Operation::StageAvr(_) => ("/api/v1/avr/image", true),
        Operation::ProgramAvr => ("/api/v1/avr/program", true),
        Operation::UpdateEsp(_) => ("/update", true),
    };
    let url = format!("{}{path}", endpoint.base.trim_end_matches('/'));
    let request = if post {
        client.post(url)
    } else {
        client.get(url)
    };
    let mut request = request
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .timeout(Duration::from_secs(180));
    match &operation {
        Operation::StageAvr(bytes) | Operation::UpdateEsp(bytes) => {
            let esp = matches!(operation, Operation::UpdateEsp(_));
            validate_image(esp, bytes)?;
            let part = multipart::Part::bytes(bytes.clone()).file_name(if esp {
                "esp8266.bin"
            } else {
                "avr.bin"
            });
            request = request.multipart(
                multipart::Form::new().part(if esp { "firmware" } else { "image" }, part),
            );
        }
        _ => {}
    }
    let response = request.send().map_err(|_| "Maintenance outcome unknown: transport failed. Nothing was replayed; reconnect and inspect before any retry.".to_string())?;
    if !response.status().is_success() {
        return Err(format!(
            "Device rejected maintenance (HTTP {}). Inspect device state before retrying.",
            response.status()
        ));
    }
    if let Operation::Download {
        eeprom,
        destination,
    } = operation
    {
        let expected = if eeprom { 1024 } else { 32768 };
        let bytes = response.take(expected + 1).bytes();
        // Bound the read even if the peer sends an unexpected body.
        let bytes = bytes
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| "Backup download failed".to_string())?;
        if bytes.len() != expected as usize {
            return Err(format!(
                "Backup length mismatch: expected {expected} bytes; no file was written"
            ));
        }
        let mut file = std::fs::OpenOptions::new()
            .create_new(true)
            .write(true)
            .open(&destination)
            .map_err(|e| format!("Cannot create export (existing files are preserved): {e}"))?;
        file.write_all(&bytes)
            .and_then(|_| file.sync_all())
            .map_err(|e| format!("Export write failed; file may be incomplete: {e}"))?;
        return Ok(format!(
            "Exported {expected} verified-length bytes to {}",
            destination.display()
        ));
    }
    if matches!(operation, Operation::StageAvr(_)) {
        if response
            .text()
            .map_err(|_| "Staging acknowledgment unreadable")?
            .trim()
            != "AVR image stored"
        {
            return Err("Unexpected staging acknowledgment; image state unconfirmed".into());
        }
        return Ok("AVR image stored, NOT programmed. Shared staging has no advertised hash; confirm provenance before programming.".into());
    }
    let value: serde_json::Value = response
        .json()
        .map_err(|_| "Maintenance acknowledgment unreadable; outcome unconfirmed")?;
    match operation {
        Operation::ReconnectCloud => (value["accepted"].as_bool() == Some(true)).then(|| "Cloud reconnect accepted. Check live MQTT/WebSocket status to verify the connection.".into()),
        Operation::Probe => value["signature"]
            .as_str()
            .filter(|s| *s == "1E950F")
            .map(|s| format!("ATmega328P ISP signature confirmed: {s}")),
        Operation::Backup => (value["backed_up"].as_bool() == Some(true))
            .then(|| "Device confirmed flash + EEPROM backup in ESP storage".into()),
        Operation::ProgramAvr => (value["programmed"].as_bool() == Some(true)
            && value["verified"].as_bool() == Some(true))
        .then(|| "Device confirmed AVR programming and verification".into()),
        Operation::UpdateEsp(_) => (value["updated"].as_bool() == Some(true)
            && value["rebooting"].as_bool() == Some(true))
        .then(|| {
            "ESP accepted firmware and scheduled reboot. Reconnect to verify running version."
                .into()
        }),
        _ => None,
    }
    .ok_or("Device did not confirm the expected maintenance result".into())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_wrong_target_or_partial_images() {
        assert!(validate_image(false, &vec![0; 32768]).is_ok());
        assert!(validate_image(false, &vec![0; 32767]).is_err());
        assert!(validate_image(true, &vec![0; 32768]).is_err());
        let mut esp = vec![0; 1024];
        esp[0] = 0xe9;
        assert!(validate_image(true, &esp).is_ok());
        assert!(validate_image(true, &[0xe9]).is_err());
    }
}
