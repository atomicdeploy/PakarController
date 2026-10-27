//! Local controller discovery adapted from PCController's layered discovery.
//!
//! The human-facing endpoint remains the stable mDNS hostname. SSDP locations
//! are source-bound, credential-free verified hints before normal peer reads.

use std::{
    collections::BTreeSet,
    net::{Ipv4Addr, SocketAddrV4, UdpSocket},
    time::{Duration, Instant},
};

#[path = "lan_discovery.rs"]
mod lan;
pub use lan::Advertiser;
pub fn board_locations(timeout: Duration) -> Vec<String> {
    lan::scan_origins(timeout)
}

pub struct VerifiedBoard {
    pub endpoint: crate::Endpoint,
    pub status: crate::DeviceStatus,
    source: String,
    remote: Option<std::net::SocketAddr>,
}
impl VerifiedBoard {
    pub fn remember_route(&self) {
        crate::peer_http::remember_address(&self.source, &self.status, self.remote);
    }
}
/// Public status only: never send saved credentials or try an actuation merely
/// because SSDP/MNDP/DNS-SD named an endpoint. Actual TCP peer seeds the cache.
pub fn probe_board(origin: &str) -> Option<VerifiedBoard> {
    let client = crate::peer_http::device_client_builder()
        .timeout(Duration::from_millis(900))
        .build()
        .ok()?;
    probe_board_with_client(origin, &client, true)
}
pub fn probe_recovery(logical: &str, numeric: &str) -> Option<VerifiedBoard> {
    let logical_url = reqwest::Url::parse(logical).ok()?;
    let numeric_url = reqwest::Url::parse(numeric).ok()?;
    if logical_url.scheme() != numeric_url.scheme()
        || logical_url.port_or_known_default() != numeric_url.port_or_known_default()
    {
        return None;
    }
    let ip = numeric_url
        .host_str()?
        .trim_matches(['[', ']'])
        .parse::<std::net::IpAddr>()
        .ok()?;
    // This client is private to the public GET below. Saved IPs never enter
    // the normal mutation resolver, and HTTPS still validates logical Host/SNI.
    let client = crate::peer_http::device_client_builder()
        .resolve(logical_url.host_str()?, std::net::SocketAddr::new(ip, 0))
        .timeout(Duration::from_millis(900))
        .build()
        .ok()?;
    probe_board_with_client(logical, &client, false)
}
fn probe_board_with_client(
    origin: &str,
    client: &reqwest::blocking::Client,
    physical_only: bool,
) -> Option<VerifiedBoard> {
    let response = client
        .get(format!("{}/api/status", origin.trim_end_matches('/')))
        .send()
        .ok()?;
    if !response.status().is_success() {
        return None;
    }
    crate::peer_identity::validate_response_headers(response.headers()).ok()?;
    let remote = response.remote_addr();
    use std::io::Read;
    let mut bytes = Vec::new();
    response.take(65537).read_to_end(&mut bytes).ok()?;
    if bytes.len() > 65536 {
        return None;
    }
    let status: crate::DeviceStatus = serde_json::from_slice(&bytes).ok()?;
    if !compatible_board(&status) || (physical_only && status.bridge.is_some()) {
        return None;
    }
    let endpoint = crate::Endpoint {
        base: crate::peer_http::configured_origin(origin, &status)?,
        user: String::new(),
        password: String::new(),
    };
    Some(VerifiedBoard {
        endpoint,
        status,
        source: origin.into(),
        remote,
    })
}
fn physical_board(status: &crate::DeviceStatus) -> bool {
    status.bridge.is_none() && compatible_board(status)
}
fn compatible_board(status: &crate::DeviceStatus) -> bool {
    crate::peer_http::Pin::from_status(status).is_some()
        && status.capabilities.as_ref().is_some_and(|c| c.known())
        && crate::peer_identity::validate_board(status).is_ok()
}
/// Multiple interface paths of one physical board count once, not as multiple
/// devices. A genuine second board requires the user's selection, not a race.
pub fn unique_board(boards: Vec<VerifiedBoard>) -> Option<VerifiedBoard> {
    let mut unique = std::collections::BTreeMap::new();
    for board in boards {
        if physical_board(&board.status) {
            unique.entry(board.status.board_id.clone()).or_insert(board);
        }
    }
    if unique.len() == 1 {
        unique.into_values().next()
    } else {
        None
    }
}
pub enum BoardChoice {
    Known(VerifiedBoard),
    New(VerifiedBoard),
}
/// Finish the complete bounded sweep before choosing. A known physical board
/// may recover another interface among unrelated devices; a new target needs
/// exactly one physical identity across the WHOLE sweep, regardless of names.
pub fn choose_board(
    boards: Vec<VerifiedBoard>,
    expected: Option<&str>,
    allow_new: bool,
) -> Option<BoardChoice> {
    if let Some(expected) = expected {
        if let Some(index) = boards
            .iter()
            .position(|board| board.status.board_id == expected && physical_board(&board.status))
        {
            return Some(BoardChoice::Known(boards.into_iter().nth(index)?));
        }
    }
    if allow_new {
        unique_board(boards).map(BoardChoice::New)
    } else {
        None
    }
}
pub fn probe_boards(origins: Vec<String>, stamp: &crate::connection::Stamp) -> Vec<VerifiedBoard> {
    let queue = std::sync::Mutex::new(std::collections::VecDeque::from(origins));
    let found = std::sync::Mutex::new(Vec::new());
    std::thread::scope(|scope| {
        for _ in 0..4 {
            scope.spawn(|| {
                while stamp.current() {
                    let Some(origin) = queue.lock().unwrap().pop_front() else {
                        break;
                    };
                    if let Some(board) = probe_board(&origin)
                        && stamp.current()
                    {
                        found.lock().unwrap().push(board);
                    }
                }
            });
        }
    });
    if stamp.current() {
        found.into_inner().unwrap()
    } else {
        Vec::new()
    }
}

const SSDP_ADDRESS: SocketAddrV4 = SocketAddrV4::new(Ipv4Addr::new(239, 255, 255, 250), 1900);

pub fn ssdp_locations(timeout: Duration) -> Vec<String> {
    let Ok(socket) = UdpSocket::bind((Ipv4Addr::UNSPECIFIED, 0)) else {
        return Vec::new();
    };
    let _ = socket.set_multicast_ttl_v4(2);
    let _ = socket.set_read_timeout(Some(Duration::from_millis(180)));
    let request = concat!(
        "M-SEARCH * HTTP/1.1\r\n",
        "HOST: 239.255.255.250:1900\r\n",
        "MAN: \"ssdp:discover\"\r\n",
        "MX: 1\r\n",
        "ST: ssdp:all\r\nUSER-AGENT: PakarController/",
        env!("CARGO_PKG_VERSION"),
        " native\r\n\r\n"
    );
    if socket.send_to(request.as_bytes(), SSDP_ADDRESS).is_err() {
        return Vec::new();
    }
    let deadline = Instant::now() + timeout;
    let mut buffer = [0_u8; 2048];
    let mut locations = BTreeSet::new();
    while Instant::now() < deadline {
        let Ok((count, source)) = socket.recv_from(&mut buffer) else {
            continue;
        };
        if let Some(location) = lan::ssdp_origin(&buffer[..count], source) {
            if locations.len() < 16 {
                locations.insert(location);
            }
        }
    }
    locations
        .into_iter()
        .filter(|v| lan::verified_origin(v))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    fn board(id: &str, origin: &str) -> VerifiedBoard {
        let status=serde_json::from_value(serde_json::json!({"version":crate::peer_identity::VERSION,"hostname":"discovery-fixture","board_id":id,"boot_id":"a".repeat(32),"http_peer_fencing":true,
            "capabilities":{"board_kind":"table","acknowledged_features":["table","leds"],"table":{"columns":10,"rows":8,"pixels_per_cell":6}}})).unwrap();
        VerifiedBoard {
            endpoint: crate::Endpoint {
                base: origin.into(),
                user: String::new(),
                password: String::new(),
            },
            status,
            source: origin.into(),
            remote: None,
        }
    }
    #[test]
    fn discovery_auto_selects_one_physical_identity_not_one_interface() {
        assert!(unique_board(vec![]).is_none());
        assert!(
            unique_board(vec![
                board("board-a", "http://192.0.2.1"),
                board("board-a", "http://192.0.2.2")
            ])
            .is_some()
        );
        assert!(
            unique_board(vec![
                board("board-a", "http://192.0.2.1"),
                board("board-b", "http://192.0.2.2")
            ])
            .is_none()
        );
        let mut bridge = board("board-a", "http://192.0.2.1");
        bridge.status.bridge = Some(serde_json::json!({"instance_id":"host"}));
        assert!(unique_board(vec![bridge]).is_none());
        let mut unsupported = board("board-a", "http://192.0.2.1");
        unsupported.status.version = "0.0.1".into();
        assert!(unique_board(vec![unsupported]).is_none());
        let mut unverified = board("board-a", "http://192.0.2.1");
        unverified.status.http_peer_fencing = false;
        assert!(unique_board(vec![unverified]).is_none());
    }
    #[test]
    fn discovery_full_sweep_never_chooses_first_matching_hostname_or_foreign_pinned_board() {
        // Every fixture deliberately advertises the same hostname.
        let multiple = || {
            vec![
                board("board-a", "http://192.0.2.1"),
                board("board-b", "http://192.0.2.2"),
            ]
        };
        assert!(choose_board(multiple(), None, true).is_none());
        assert!(
            matches!(choose_board(multiple(),Some("board-b"),false),Some(BoardChoice::Known(value)) if value.status.board_id=="board-b")
        );
        assert!(choose_board(multiple(), Some("missing-board"), false).is_none());
        assert!(
            matches!(choose_board(vec![board("new-board","http://192.0.2.3")],Some("missing-board"),true),Some(BoardChoice::New(value)) if value.status.board_id=="new-board")
        );
    }
    #[test]
    fn discovery_disconnect_or_usb_prevents_even_readonly_candidate_contact() {
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let origin = format!("http://{}", listener.local_addr().unwrap());
        for method in [None, Some(crate::Transport::Serial)] {
            let gate = crate::connection::Gate::default();
            let epoch = gate.select(Some(crate::Transport::WiFi));
            let stamp = gate.stamp(crate::Transport::WiFi, epoch);
            gate.select(method);
            assert!(probe_boards(vec![origin.clone()], &stamp).is_empty());
        }
        assert!(listener.accept().is_err());
    }
    #[test]
    fn discovery_late_status_after_owner_retires_is_not_auto_selected() {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let origin = format!("http://{}", listener.local_addr().unwrap());
        let gate = crate::connection::Gate::default();
        let epoch = gate.select(Some(crate::Transport::WiFi));
        let stamp = gate.stamp(crate::Transport::WiFi, epoch);
        let owner = gate.clone();
        let response = serde_json::to_vec(&board("board-a", &origin).status).unwrap();
        let server = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            stream
                .set_read_timeout(Some(Duration::from_secs(2)))
                .unwrap();
            let mut request = [0; 4096];
            let bytes = stream.read(&mut request).unwrap();
            assert!(
                std::str::from_utf8(&request[..bytes])
                    .unwrap()
                    .starts_with("GET /api/status ")
            );
            owner.select(None);
            write!(
                stream,
                "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                response.len()
            )
            .unwrap();
            stream.write_all(&response).unwrap();
        });
        assert!(probe_boards(vec![origin], &stamp).is_empty());
        server.join().unwrap();
    }

    #[test]
    fn extracts_origin_from_ssdp_location() {
        let response = b"HTTP/1.1 200 OK\r\nLOCATION: http://192.0.2.7:80/description.xml\r\nST: upnp:rootdevice\r\n\r\n";
        assert_eq!(
            lan::ssdp_origin(response, "192.0.2.7:1900".parse().unwrap()).as_deref(),
            Some("http://192.0.2.7:80")
        );
    }

    #[test]
    fn rejects_non_responses() {
        assert!(
            lan::ssdp_origin(
                b"NOTIFY * HTTP/1.1\r\nLOCATION: http://192.0.2.7/\r\n",
                "192.0.2.7:1900".parse().unwrap()
            )
            .is_none()
        );
    }
}
