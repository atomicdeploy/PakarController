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
    Download {
        eeprom: bool,
        destination: PathBuf,
    },
    StageAvr(Vec<u8>),
    ProgramAvr,
    UpdateEsp(Vec<u8>),
    RemoteStage {
        url: String,
        sha256: String,
        targets: u8,
        bearer: String,
    },
    RemoteApply(RemoteJob),
    RemoteCancel(RemoteJob),
    RemoteStatus,
}
impl Operation {
    pub fn warning(&self) -> &'static str {
        match self {
            Self::RemoteStage { .. } => {
                "Download and SHA-256-verify the pinned manifest and selected complete firmware images into ESP storage. This stages only: neither board is programmed. Endpoint credentials are not saved by this app."
            }
            Self::RemoteApply(_) => {
                "Apply exactly this advertised job identity. Durable AVR Prog safety, full recovery backup, EEPROM preservation/readback and matching pair versions are required. Power loss is not an atomic two-board rollback. An uncertain outcome must not be retried blindly."
            }
            Self::RemoteCancel(_) => {
                "Cancel this exact download/staged job. Applying jobs cannot be cancelled. Staged files remain for inspection; no automatic rollback occurs."
            }
            Self::RemoteStatus => {
                "Read the observed remote firmware transaction. No hardware write."
            }
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
                "Replace ESP firmware and reboot. Confirm this binary is built for this exact ESP board/module and preserves its pin mapping. A lost connection does not prove failure; reconnect before retrying."
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
            return Err("Select an uncompressed ESP .bin (E9 header, 16 bytes–2 MiB). Confirm the exact board/module; device flash capacity is validated by the updater.".into());
        }
    } else if bytes.len() != 32768 {
        return Err("AVR staging requires exactly 32768 bytes; Intel HEX and partial application images are not accepted".into());
    }
    Ok(())
}
pub fn run(client: &Client, endpoint: &Endpoint, operation: Operation) -> Result<String, String> {
    if matches!(
        operation,
        Operation::RemoteStage { .. }
            | Operation::RemoteApply(_)
            | Operation::RemoteCancel(_)
            | Operation::RemoteStatus
    ) {
        return remote_run(client, endpoint, operation);
    }
    let (path, post) = match &operation {
        Operation::ReconnectCloud => ("/api/cloud/reconnect", true),
        Operation::Probe => ("/api/avr/probe", false),
        Operation::Backup => ("/api/avr/backup", true),
        Operation::Download { eeprom: true, .. } => ("/api/avr/backup/eeprom", false),
        Operation::Download { .. } => ("/api/avr/backup/flash", false),
        Operation::StageAvr(_) => ("/api/avr/image", true),
        Operation::ProgramAvr => ("/api/avr/program", true),
        Operation::UpdateEsp(_) => ("/update", true),
        _ => unreachable!(),
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
                "esp.bin"
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
#[derive(Clone, Debug, serde::Deserialize, serde::Serialize)]
pub struct RemoteJob {
    pub job_id: u32,
    pub boot_id: String,
    pub phase: String,
    pub targets: u8,
    pub manifest_sha256: String,
    pub busy: bool,
    pub cancellable: bool,
    pub received_bytes: u32,
    pub expected_bytes: u32,
    pub error: String,
}
pub fn valid_hash(s: &str) -> bool {
    s.len() == 64
        && s.bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}
pub fn validate_remote(url: &str, sha: &str, targets: u8) -> Result<(), String> {
    let parsed = reqwest::Url::parse(url).map_err(|_| "Invalid firmware URL")?;
    if url.len() > 383
        || parsed.scheme() != "https"
        || parsed.host_str().is_none()
        || !parsed.username().is_empty()
        || parsed.password().is_some()
        || parsed.fragment().is_some()
        || url.bytes().any(|b| b <= 32 || b >= 127 || b == b'\\')
    {
        return Err(
            "Use a direct HTTPS hostname URL without embedded credentials or fragment".into(),
        );
    }
    if !valid_hash(sha) || !(1..=3).contains(&targets) {
        return Err(
            "A lowercase 64-digit manifest SHA-256 and target 1, 2 or 3 are required".into(),
        );
    }
    Ok(())
}
pub fn parse_job(value: serde_json::Value) -> Result<RemoteJob, String> {
    let job: RemoteJob =
        serde_json::from_value(value).map_err(|_| "Malformed remote job response")?;
    if !(1..=3).contains(&job.targets)
        || job.boot_id.is_empty()
        || job.boot_id.len() > 80
        || job.received_bytes > job.expected_bytes
        || (job.job_id > 0 && !valid_hash(&job.manifest_sha256))
        || ![
            "idle",
            "manifest",
            "download_esp",
            "download_avr",
            "ready",
            "prepare",
            "program_avr",
            "program_esp",
            "awaiting_reboot",
            "verify_pair",
            "release_pair",
            "complete",
            "failed",
            "cancelled",
        ]
        .contains(&job.phase.as_str())
    {
        return Err("Invalid remote transaction identity/state".into());
    }
    Ok(job)
}
pub fn result_severity(result: &Result<String,String>) -> crate::messaging::Severity {
    use crate::messaging::Severity;
    let Ok(text)=result else {return Severity::Error;};
    let job=serde_json::from_str(text).ok().and_then(|v|parse_job(v).ok());
    match job.as_ref().map(|j|j.phase.as_str()) {
        Some("complete") => Severity::Success,
        Some("failed") => Severity::Error,
        Some(_) => Severity::Info, // Accepted/staged/applying is NOT completion.
        None => Severity::Success, // Legacy run() already validated positive ACK.
    }
}
fn remote_run(
    client: &Client,
    endpoint: &Endpoint,
    operation: Operation,
) -> Result<String, String> {
    let (path, form) = match &operation {
        Operation::RemoteStatus => ("/api/firmware/remote", None),
        Operation::RemoteStage {
            url,
            sha256,
            targets,
            bearer,
        } => {
            validate_remote(url, sha256, *targets)?;
            if bearer.len() > 256 || bearer.bytes().any(|b| !(33..=126).contains(&b)) {
                return Err("Invalid firmware endpoint bearer token".into());
            }
            (
                "/api/firmware/fetch",
                Some(vec![
                    ("url", url.clone()),
                    ("sha256", sha256.clone()),
                    ("targets", targets.to_string()),
                    ("apply", "false".into()),
                    ("bearer", bearer.clone()),
                ]),
            )
        }
        Operation::RemoteApply(job) | Operation::RemoteCancel(job) => {
            parse_job(serde_json::to_value(job).map_err(|e| e.to_string())?)?;
            let apply = matches!(operation, Operation::RemoteApply(_));
            if (apply && job.phase != "ready")
                || (!apply && !job.cancellable && job.phase != "ready")
            {
                return Err(
                    "Observed job is not in the required state; refresh before proceeding".into(),
                );
            }
            (
                if apply {
                    "/api/firmware/apply"
                } else {
                    "/api/firmware/cancel"
                },
                Some(vec![
                    ("job_id", job.job_id.to_string()),
                    ("boot_id", job.boot_id.clone()),
                    ("sha256", job.manifest_sha256.clone()),
                ]),
            )
        }
        _ => return Err("Not a remote operation".into()),
    };
    let url = format!("{}{path}", endpoint.base.trim_end_matches('/'));
    let request = if let Some(form) = form {
        client.post(url).form(&form)
    } else {
        client.get(url)
    };
    let response = request
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .timeout(Duration::from_secs(15))
        .send()
        .map_err(|_| "Remote firmware request outcome unknown. No mutation was replayed.")?;
    if !response.status().is_success() {
        return Err(format!(
            "Remote firmware rejected (HTTP {})",
            response.status()
        ));
    }
    let mut bytes = Vec::new();
    response
        .take(8193)
        .read_to_end(&mut bytes)
        .map_err(|_| "Remote acknowledgment unreadable")?;
    if bytes.len() > 8192 {
        return Err("Oversized remote acknowledgment".into());
    }
    let value: serde_json::Value =
        serde_json::from_slice(&bytes).map_err(|_| "Invalid remote acknowledgment JSON")?;
    let job = parse_job(value.clone())?;
    match operation {
        Operation::RemoteStage {
            sha256, targets, ..
        } if job.job_id == 0
            || job.manifest_sha256 != sha256
            || job.targets != targets
            || job.phase != "manifest" =>
        {
            return Err("Peer did not acknowledge the pinned staging request".into());
        }
        Operation::RemoteApply(expected) | Operation::RemoteCancel(expected) => {
            if job.job_id != expected.job_id
                || job.boot_id != expected.boot_id
                || job.manifest_sha256 != expected.manifest_sha256
                || (path.ends_with("apply") && job.phase != "prepare")
                || (path.ends_with("cancel") && job.phase != "cancelled")
            {
                return Err("Remote mutation acknowledgment did not match this exact job".into());
            }
        }
        _ => {}
    }
    serde_json::to_string_pretty(&value).map_err(|e| e.to_string())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_untrusted_urls_hashes_and_unobserved_jobs() {
        let sha="a".repeat(64);
        assert!(validate_remote("https://firmware.example/PakarController/manifest.json",&sha,3).is_ok());
        for url in ["http://firmware.example/file","https://user:pass@firmware.example/file",
                    "https://firmware.example/file#fragment","https://firmware.example/ bad"] {
            assert!(validate_remote(url,&sha,3).is_err());
        }
        assert!(validate_remote("https://host/file",&"A".repeat(64),3).is_err());
        assert!(validate_remote("https://host/file",&sha,4).is_err());
        assert!(parse_job(serde_json::json!({})).is_err());
    }
    #[test]
    fn remote_requests_pin_identity_and_never_replay_lost_or_wrong_ack() {
        use std::net::TcpListener;
        let listener=TcpListener::bind("127.0.0.1:0").unwrap();
        let address=listener.local_addr().unwrap();
        let sha="b".repeat(64);
        let ready=RemoteJob{job_id:7,boot_id:"observed-boot".into(),phase:"ready".into(),targets:3,
            manifest_sha256:sha.clone(),busy:false,cancellable:false,received_bytes:32768,expected_bytes:32768,error:String::new()};
        let job=ready.clone();
        let server=std::thread::spawn(move|| {
            for (i,stream) in listener.incoming().take(2).enumerate() {
                let mut stream=stream.unwrap();stream.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
                let mut request=Vec::new();let mut chunk=[0;1024];
                loop {
                    let n=stream.read(&mut chunk).unwrap();assert!(n>0);request.extend_from_slice(&chunk[..n]);
                    if let Some(end)=request.windows(4).position(|p|p==b"\r\n\r\n") {
                        let header=String::from_utf8_lossy(&request[..end]).to_lowercase();
                        let length=header.lines().find_map(|s|s.strip_prefix("content-length: ").and_then(|v|v.parse::<usize>().ok())).unwrap_or(0);
                        if request.len()>=end+4+length {break;}
                    }
                }
                let text=String::from_utf8(request).unwrap();
                assert!(text.starts_with(if i==0{"POST /api/firmware/fetch"}else{"POST /api/firmware/apply"}));
                assert!(text.to_lowercase().contains("authorization: basic "));
                let body=text.split("\r\n\r\n").nth(1).unwrap();
                let form:std::collections::HashMap<_,_>=reqwest::Url::parse(&format!("http://fixture/?{body}")).unwrap().query_pairs().into_owned().collect();
                if i==0 {assert_eq!(form.get("apply").unwrap(),"false");assert_eq!(form.get("sha256").unwrap(),&job.manifest_sha256);}
                else {assert_eq!(form.get("job_id").unwrap(),"7");assert_eq!(form.get("boot_id").unwrap(),"observed-boot");}
                let mut reply=job.clone();reply.phase=if i==0{"manifest"}else{"prepare"}.into();
                if i==1 {reply.boot_id="wrong-boot".into();}
                let bytes=serde_json::to_vec(&reply).unwrap();
                write!(stream,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",bytes.len()).unwrap();
                stream.write_all(&bytes).unwrap();
            }
        });
        let endpoint=Endpoint{base:format!("http://{address}"),user:"fixture".into(),password:"fixture-private".into()};
        let client=Client::builder().no_proxy().build().unwrap();
        assert!(run(&client,&endpoint,Operation::RemoteStage{url:"https://firmware.example/manifest.json".into(),sha256:sha,targets:3,bearer:String::new()}).is_ok());
        assert!(run(&client,&endpoint,Operation::RemoteApply(ready)).unwrap_err().contains("exact job"));
        server.join().unwrap();
    }
    #[test]
    fn accepted_update_is_not_reported_as_completion() {
        use crate::messaging::Severity;
        let mut value=serde_json::json!({"job_id":1,"boot_id":"observed",
            "phase":"prepare","targets":3,"manifest_sha256":"a".repeat(64),
            "busy":true,"cancellable":false,"received_bytes":0,"expected_bytes":0,"error":""});
        assert_eq!(result_severity(&Ok(value.to_string())),Severity::Info);
        value["phase"]="complete".into();assert_eq!(result_severity(&Ok(value.to_string())),Severity::Success);
        value["phase"]="failed".into();assert_eq!(result_severity(&Ok(value.to_string())),Severity::Error);
        assert_eq!(result_severity(&Err("ACK lost".into())),Severity::Error);
    }
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
