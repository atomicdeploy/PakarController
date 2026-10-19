//! PCController artifacts/download.go's bounded verified-reader semantics,
//! adapted to small board images in RAM, not another disk cache/programmer.
use crate::{Endpoint, firmware_progress::Transfer};
use reqwest::blocking::{Client, multipart};
use sha2::{Digest, Sha256};
use std::{
    io::{Cursor, Read},
    time::Duration,
};

#[derive(Clone)]
pub enum Source {
    Local(Vec<u8>),
    Http {
        url: String,
        sha256: String,
        bearer: String,
    },
}
pub fn url(value: &str) -> Result<reqwest::Url, String> {
    let u = reqwest::Url::parse(value).map_err(|_| "Invalid firmware file URL")?;
    if value.len() > 2048
        || !matches!(u.scheme(), "http" | "https")
        || u.host_str().is_none()
        || !u.username().is_empty()
        || u.password().is_some()
        || u.fragment().is_some()
        || value.bytes().any(|c| c <= 32 || c >= 127 || c == b'\\')
    {
        return Err(
            "Use a direct HTTP(S) file URL without embedded credentials or fragment".into(),
        );
    }
    Ok(u)
}
pub fn validate_remote(value: &str, sha: &str, bearer: &str) -> Result<(), String> {
    url(value)?;
    if !crate::maintenance::valid_hash(sha)
        || bearer.len() > 256
        || bearer.bytes().any(|b| !(33..=126).contains(&b))
    {
        return Err(
            "A lowercase SHA-256 is required; optional bearer must be printable ASCII".into(),
        );
    }
    Ok(())
}
pub fn digest(bytes: &[u8]) -> String {
    Sha256::digest(bytes)
        .iter()
        .map(|b| format!("{b:02x}"))
        .collect()
}
fn download(source: Source, esp: bool, progress: &Transfer) -> Result<Vec<u8>, String> {
    let Source::Http {
        url,
        sha256,
        bearer,
    } = source
    else {
        if let Source::Local(bytes) = source {
            return Ok(bytes);
        }
        unreachable!()
    };
    validate_remote(&url, &sha256, &bearer)?;
    progress.admit()?;
    progress.set("downloading file", 0, None);
    // Never reuse the board client: its Basic/pinned headers must not leave LAN.
    let remote = crate::peer_identity::client_builder()
        .redirect(reqwest::redirect::Policy::none())
        .connect_timeout(Duration::from_secs(8))
        .timeout(Duration::from_secs(180))
        .build()
        .map_err(|_| "Cannot initialize firmware download")?;
    let mut request = remote.get(url).header("Accept", "application/octet-stream");
    if !bearer.is_empty() {
        request = request.bearer_auth(bearer);
    }
    let mut response = request
        .send()
        .map_err(|_| "Firmware file download failed; no board write")?;
    let maximum = crate::maintenance::maximum_image_bytes(esp);
    let total = response.content_length();
    if response.status() != reqwest::StatusCode::OK
        || total.is_some_and(|n| n == 0 || n > maximum)
        || response
            .headers()
            .get("content-encoding")
            .is_some_and(|v| v != "identity")
    {
        return Err("Firmware download rejected, encoded or oversized; no board write".into());
    }
    let mut bytes = Vec::new();
    let mut block = [0u8; 16384];
    loop {
        progress.admit()?;
        let n = response
            .read(&mut block)
            .map_err(|_| "Firmware download truncated; no board write")?;
        if n == 0 {
            break;
        }
        if bytes.len() + n > maximum as usize {
            return Err("Firmware exceeds its image limit; no board write".into());
        }
        bytes.extend_from_slice(&block[..n]);
        progress.set("downloading file", bytes.len() as u64, total);
    }
    if total.is_some_and(|n| n != bytes.len() as u64) || digest(&bytes) != sha256 {
        return Err("Firmware length/SHA-256 mismatch; no board write".into());
    }
    Ok(bytes)
}
struct UploadReader {
    data: Cursor<Vec<u8>>,
    progress: Transfer,
    total: u64,
}
impl Read for UploadReader {
    fn read(&mut self, b: &mut [u8]) -> std::io::Result<usize> {
        self.progress.admit().map_err(std::io::Error::other)?;
        let n = self.data.read(b)?;
        self.progress
            .set("uploading to board", self.data.position(), Some(self.total));
        Ok(n)
    }
}
pub fn run(
    client: &Client,
    endpoint: &Endpoint,
    source: Source,
    esp: bool,
    progress: &Transfer,
) -> Result<String, String> {
    let result = (|| {
        let bytes = download(source, esp, progress)?;
        crate::maintenance::validate_image(esp, &bytes)?;
        progress.admit()?;
        progress.set("checking board before upload", 0, None);
        let before = crate::blocking_status(client, endpoint)?;
        if progress.scope.pin.as_ref() != crate::peer_http::Pin::from_status(&before).as_ref()
            || !before
                .capabilities
                .as_ref()
                .is_some_and(|c| c.has(if esp { "ota" } else { "isp" }))
        {
            return Err("Board/boot/capability changed before upload; no firmware sent".into());
        }
        crate::maintenance::validate_image_for_board(esp, &bytes, &before)?;
        if before.remote_firmware["busy"] == true
            || before.remote_firmware["phase"] == "ready"
            || before.update["phase"] == "receiving"
            || before.update["phase"] == "verifying"
            || before.programming_latch
        {
            return Err("Board maintenance already owned; no firmware sent".into());
        }
        progress.admit()?;
        progress.before_upload()?;
        let total = bytes.len() as u64;
        progress.set("uploading to board", 0, Some(total));
        let reader = UploadReader {
            data: Cursor::new(bytes),
            progress: progress.clone(),
            total,
        };
        let part = multipart::Part::reader_with_length(reader, total).file_name(if esp {
            "esp.bin"
        } else {
            "avr.bin"
        });
        let mut request = client
            .post(format!(
                "{}{}",
                endpoint.base.trim_end_matches('/'),
                if esp { "/update" } else { "/api/avr/image" }
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .timeout(Duration::from_secs(180));
        if esp {
            request = request.header("X-Pakar-Firmware-Size", total);
        }
        let response=request.multipart(multipart::Form::new().part(if esp {"firmware"}else{"image"},part))
            .send().map_err(|_|"Firmware upload outcome unknown; not replayed. Inspect the board before retrying")?;
        progress.set("waiting for board acknowledgment", total, None);
        if !response.status().is_success() {
            return Err(format!(
                "Board rejected firmware (HTTP {}); not replayed",
                response.status()
            ));
        }
        let mut ack = Vec::new();
        response
            .take(8193)
            .read_to_end(&mut ack)
            .map_err(|_| "Unreadable firmware acknowledgment")?;
        if ack.len() > 8192 {
            return Err("Oversized firmware acknowledgment; outcome unknown".into());
        }
        if esp {
            let v: serde_json::Value = serde_json::from_slice(&ack)
                .map_err(|_| "Invalid firmware acknowledgment; outcome unknown")?;
            if v["updated"] != true || v["rebooting"] != true {
                return Err("Board did not confirm firmware acceptance".into());
            }
            Ok("ESP accepted firmware; awaiting reboot and running-image verification. Not completion.".into())
        } else if ack == b"AVR image stored" {
            Ok("AVR image uploaded and stored, NOT programmed. Apply remains explicit.".into())
        } else {
            Err("Board did not confirm AVR staging; outcome unknown".into())
        }
    })();
    progress.finish(&result);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn firmware_source_urls_and_digest_are_not_credentials_or_redirect_policy() {
        assert_eq!(
            digest(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
        for u in [
            "https://updates.example/esp.bin",
            "http://ledboard.local/firmware/download",
        ] {
            assert!(validate_remote(u, &"a".repeat(64), "").is_ok());
        }
        for u in [
            "file:///private",
            "http://user:pw@host/file",
            "https://host/file#fragment",
            "https://host/ bad",
        ] {
            assert!(url(u).is_err());
        }
        assert!(validate_remote("https://host/file", "bad", "").is_err());
    }
    #[test]
    fn firmware_source_real_http_download_is_verified_before_one_pinned_upload() {
        use crate::{DeviceStatus, peer_http};
        use std::io::Write;
        use std::net::TcpListener;
        let server = TcpListener::bind("127.0.0.1:0").unwrap();
        let addr = server.local_addr().unwrap();
        let status = serde_json::json!({"version":crate::peer_identity::VERSION,"hostname":"fixture","board_id":"esp32s3-fixture","boot_id":"b".repeat(32),"http_peer_fencing":true,"ota_capacity_bytes":0x330000,
            "capabilities":{"board_kind":"controller","acknowledged_features":["ota"]},"remote_firmware":{"busy":false,"phase":"idle"}});
        let s: DeviceStatus = serde_json::from_value(status.clone()).unwrap();
        let mut image = vec![0; 2_220_352];
        image[0] = 0xe9;image[1]=4;image[12]=9;image[23]=1;
        let hash = digest(&image);
        let expected = image.clone();
        let worker = std::thread::spawn(move || {
            for index in 0..3 {
                let (mut tcp, _) = server.accept().unwrap();
                tcp.set_read_timeout(Some(Duration::from_secs(5))).unwrap();
                let mut request = Vec::new();
                let mut block = [0u8; 4096];
                loop {
                    let n = tcp.read(&mut block).unwrap();
                    assert!(n > 0);
                    request.extend_from_slice(&block[..n]);
                    if let Some(end) = request.windows(4).position(|p| p == b"\r\n\r\n") {
                        let head = String::from_utf8_lossy(&request[..end]).to_lowercase();
                        let size = head
                            .lines()
                            .find_map(|l| {
                                l.strip_prefix("content-length: ")
                                    .and_then(|s| s.parse::<usize>().ok())
                            })
                            .unwrap_or(0);
                        if request.len() >= end + 4 + size {
                            break;
                        }
                    }
                }
                let text = String::from_utf8_lossy(&request);
                let lower = text.to_lowercase();
                let reply = if index == 0 {
                    assert!(text.starts_with("GET /image.bin "));
                    assert!(!lower.contains("authorization:") && !lower.contains("x-pakar-board:"));
                    expected.clone()
                } else if index == 1 {
                    assert!(text.starts_with("GET /api/status "));
                    serde_json::to_vec(&status).unwrap()
                } else {
                    assert!(text.starts_with("POST /update "));
                    assert!(lower.contains(&format!("x-pakar-firmware-size: {}",expected.len())));
                    assert!(
                        lower.contains("x-pakar-board: esp32s3-fixture")
                            && lower.contains(&format!("x-pakar-boot: {}", "b".repeat(32)))
                    );
                    assert!(request.windows(expected.len()).any(|v| v == expected));
                    b"{\"updated\":true,\"rebooting\":true}".to_vec()
                };
                write!(
                    tcp,
                    "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                    reply.len()
                )
                .unwrap();
                tcp.write_all(&reply).unwrap();
            }
        });
        let (tx, _) = std::sync::mpsc::channel();
        let mut q = peer_http::Queue::new(tx);
        q.observe(&s);
        let p = Transfer::new(q.capture(), &s);
        let endpoint = Endpoint {
            base: format!("http://{addr}"),
            user: String::new(),
            password: String::new(),
        };
        let result = run(
            &peer_http::pinned_client(&s).unwrap(),
            &endpoint,
            Source::Http {
                url: format!("http://{addr}/image.bin"),
                sha256: hash,
                bearer: String::new(),
            },
            true,
            &p,
        );
        assert!(result.unwrap().contains("Not completion"));
        let observed = p.snapshot(Some(&s)).unwrap();
        assert!(!observed.verified && !observed.active);
        worker.join().unwrap();
    }
}
