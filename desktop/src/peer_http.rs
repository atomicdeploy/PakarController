//! Actual PCController Session acquireWrite/requestAdmission before/after
//! admission and closing checks, adapted to a revocable native HTTP queue.
//! This is not import of its serial acquisition, ACK routing or reset policy.
use crate::{DeviceStatus, Event, HttpCommand};
use reqwest::blocking::Client;
use std::{
    collections::{HashSet, VecDeque},
    net::{IpAddr, SocketAddr},
    sync::{
        Arc, Mutex, OnceLock,
        atomic::{AtomicU64, Ordering},
        mpsc::{SendError, Sender},
    },
    time::{Duration, Instant},
};

const ADDRESS_CACHE_LIMIT: usize = 32;

/// Routing hints only, never board/boot ownership or permission to replay.
/// Adapted from PCController lanresolver's successful-TCP-address cache.
#[derive(Default)]
struct AddressCache {
    entries: VecDeque<(String, IpAddr)>,
    relookup: HashSet<String>,
}
impl AddressCache {
    fn remember(&mut self, domain: String, address: IpAddr) {
        self.entries.retain(|(known, _)| *known != domain);
        self.relookup.remove(&domain);
        if self.entries.len() >= ADDRESS_CACHE_LIMIT
            && let Some((evicted, _)) = self.entries.pop_front()
        {
            self.relookup.remove(&evicted);
        }
        self.entries.push_back((domain, address));
    }
    fn invalidate(&mut self, domain: &str) {
        self.entries.retain(|(known, _)| known != domain);
        self.relookup.remove(domain);
    }
    fn request_relookup(&mut self, domain: &str) {
        if self.entries.iter().any(|(known, _)| known == domain) {
            self.relookup.insert(domain.to_owned());
        }
    }
}
fn address_cache() -> &'static Mutex<AddressCache> {
    static CACHE: OnceLock<Mutex<AddressCache>> = OnceLock::new();
    CACHE.get_or_init(|| Mutex::new(AddressCache::default()))
}

fn local_origin(origin: &str) -> Option<(String, String, u16)> {
    let url = reqwest::Url::parse(origin).ok()?;
    if !matches!(url.scheme(), "http" | "https")
        || !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
        || !matches!(url.path(), "" | "/")
    {
        return None;
    }
    let domain = url.host_str()?.to_ascii_lowercase();
    let expected = domain.strip_suffix(".local")?;
    if crate::identity::normalize_name(expected).ok().as_deref() != Some(expected) {
        return None;
    }
    Some((
        domain.clone(),
        expected.to_owned(),
        url.port_or_known_default()?,
    ))
}

fn verified_address(
    origin: &str,
    status: &DeviceStatus,
    remote: Option<SocketAddr>,
) -> Option<(String, IpAddr)> {
    let (domain, expected, port) = local_origin(origin)?;
    let remote = remote?;
    if remote.port() != port
        || remote.ip().is_unspecified()
        || remote.ip().is_multicast()
        || Pin::from_status(status).is_none()
        || !crate::bridge::matches_hostname(status, &expected)
    {
        return None;
    }
    Some((domain, remote.ip()))
}

/// Call only with remote_addr() from the actual successfully decoded response.
/// Status IP/SSDP metadata is deliberately never consulted. No TTL expiration:
/// HTTP/auth/body-timeout failures do not evict a previously verified route.
pub fn remember_address(origin: &str, status: &DeviceStatus, remote: Option<SocketAddr>) {
    if let Some((domain, address)) = verified_address(origin, status, remote)
        && let Ok(mut cache) = address_cache().lock()
    {
        cache.remember(domain, address);
    }
}

/// A confirmed resolution failure can explicitly remove the obsolete hint.
pub fn invalidate_address(origin: &str) {
    if let Some((domain, _, _)) = local_origin(origin)
        && let Ok(mut cache) = address_cache().lock()
    {
        cache.invalidate(&domain);
    }
}

/// A failed CONNECT may request a fresh lookup on the next client without
/// discarding the last verified address. No command or body read is replayed.
/// Marks exist only for cached keys, so they share the same 32-entry bound.
pub fn request_relookup(origin: &str) {
    if let Some((domain, _, _)) = local_origin(origin)
        && let Ok(mut cache) = address_cache().lock()
    {
        cache.request_relookup(&domain);
    }
}

/// Preserve the URL hostname/Host header and HTTPS SNI/certificate checks while
/// bypassing repeated slow OS mDNS lookup for previously verified local peers.
/// Build a fresh client after invalidation; never automatically resend a write.
pub fn device_client_builder() -> reqwest::blocking::ClientBuilder {
    let mut builder = Client::builder()
        .no_proxy()
        .redirect(reqwest::redirect::Policy::none())
        .timeout(Duration::from_secs(4))
        .connect_timeout(Duration::from_secs(4))
        .pool_max_idle_per_host(0);
    if let Ok(cache) = address_cache().lock() {
        for (domain, address) in &cache.entries {
            if !cache.relookup.contains(domain) {
                builder = builder.resolve(domain, SocketAddr::new(*address, 0));
            }
        }
    }
    builder
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Pin {
    board: String,
    boot: String,
}
impl Pin {
    pub fn matches_identity(&self, board: &str, boot: &str) -> bool {
        self.board == board && self.boot == boot
    }
    pub fn from_status(s: &DeviceStatus) -> Option<Self> {
        (s.http_peer_fencing
            && !s.board_id.is_empty()
            && s.board_id.len() <= 64
            && s.board_id
                .bytes()
                .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == b'-' || c == b'_')
            && s.boot_id.len() == 32
            && s.boot_id
                .bytes()
                .all(|c| c.is_ascii_digit() || (b'a'..=b'f').contains(&c))
            && s.boot_id.bytes().any(|c| c != b'0'))
        .then(|| Self {
            board: s.board_id.clone(),
            boot: s.boot_id.clone(),
        })
    }
    pub fn client(&self) -> Result<Client, String> {
        let mut headers = reqwest::header::HeaderMap::new();
        headers.insert(
            "x-pakar-board",
            self.board.parse().map_err(|_| "Invalid board identity")?,
        );
        headers.insert(
            "x-pakar-boot",
            self.boot.parse().map_err(|_| "Invalid boot identity")?,
        );
        device_client_builder()
            .default_headers(headers)
            .build()
            .map_err(crate::diagnostics::describe_error)
    }
}
pub fn pinned_client(status: &DeviceStatus) -> Result<Client, String> {
    Pin::from_status(status)
        .ok_or("Read-only: peer has not advertised valid board/boot identity and HTTP fencing")?
        .client()
}
#[derive(Clone)]
pub struct Scope {
    pub pin: Option<Pin>,
    generation: u64,
    clock: Arc<AtomicU64>,
    queued_at: Instant,
}
impl Scope {
    pub fn current(&self) -> bool {
        self.clock.load(Ordering::Acquire) == self.generation
    }
    pub fn admit(&self) -> Result<(), String> {
        self.admit_with_limit(Duration::from_secs(8))
    }
    pub fn admit_read(&self) -> Result<(), String> {
        self.admit_with_limit(Duration::from_secs(24))
    }
    fn admit_with_limit(&self, limit: Duration) -> Result<(), String> {
        if !self.current() {
            return Err("Board session revoked before dispatch; nothing was sent".into());
        }
        if self.queued_at.elapsed() >= limit {
            return Err(
                "Queued request expired before dispatch; refresh instead of replaying".into(),
            );
        }
        Ok(())
    }
}
pub struct Queue {
    inner: Sender<HttpCommand>,
    clock: Arc<AtomicU64>,
    pin: Option<Pin>,
}
impl Queue {
    pub fn new(inner: Sender<HttpCommand>) -> Self {
        Self {
            inner,
            clock: Arc::new(AtomicU64::new(0)),
            pin: None,
        }
    }
    pub fn send(&self, command: HttpCommand) -> Result<(), SendError<HttpCommand>> {
        self.inner
            .send(HttpCommand::Scoped(self.capture(), Box::new(command)))
    }
    pub fn capture(&self) -> Scope {
        Scope {
            pin: self.pin.clone(),
            generation: self.clock.load(Ordering::Acquire),
            clock: self.clock.clone(),
            queued_at: Instant::now(),
        }
    }
    pub fn clear(&mut self) {
        self.pin = None;
        self.clock.fetch_add(1, Ordering::AcqRel);
    }
    pub fn observe(&mut self, status: &DeviceStatus) -> bool {
        let next = Pin::from_status(status);
        if next == self.pin {
            return false;
        }
        self.clear();
        self.pin = next;
        true
    }
    pub fn writable(&self) -> bool {
        self.pin.is_some()
    }
}

/// A retired result stays in Events, never another peer's drafts/credentials.
/// Payload/error bodies are deliberately not included in this receipt.
pub fn retired_receipt(event: &Event) -> Option<String> {
    let kind = match event {
        Event::Accepted(_) => "Command acceptance",
        Event::CellBatchConfirmed(_) | Event::SerialBatchConfirmed(..) => {
            "Atomic cell batch readback"
        }
        Event::OperationFailed(_) => "Command failure",
        Event::SettingsFinished(..) => "Settings result",
        Event::MaintenanceFinished(..) => "Maintenance result",
        Event::PixelModeFinished(..) | Event::PixelSent(..) | Event::PixelReadbackFinished(..) => {
            "Pixel diagnostic result"
        }
        Event::CredentialsUpdated(_) => "Credential result",
        Event::NameUpdated(..) => "Identity result",
        _ => return None,
    };
    Some(format!(
        "{kind} belongs to a retired board session; not applied to this peer. Inspect the original device before retrying."
    ))
}

/// Preserve generic retired outcomes even when the outer endpoint changed.
/// Never carry its response body or credentials into the new peer.
pub fn retired_envelope(event: &Event) -> Option<String> {
    match event {
        Event::ForConnection(stamp, inner) if !stamp.current() => {
            retired_envelope(inner).or_else(|| retired_receipt(inner))
        }
        Event::ForConnection(_, inner) => retired_envelope(inner),
        Event::ForEndpoint(_, inner) => retired_envelope(inner),
        Event::ForScope(scope, inner) if !scope.current() => retired_receipt(inner),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn status(boot: char) -> DeviceStatus {
        DeviceStatus {
            board_id: "esp8266-aabbccddeeff".into(),
            boot_id: boot.to_string().repeat(32),
            http_peer_fencing: true,
            ..Default::default()
        }
    }
    #[test]
    fn local_address_cache_accepts_only_verified_tcp_peer_not_advertised_ip() {
        let mut peer = status('a');
        peer.hostname = "ledboard".into();
        peer.ip = "203.0.113.99".into();
        let actual: SocketAddr = "192.0.2.10:80".parse().unwrap();
        let accepted = verified_address("http://LEDBOARD.local", &peer, Some(actual)).unwrap();
        assert_eq!(accepted, ("ledboard.local".into(), actual.ip()));
        assert!(verified_address("http://ledboard.local", &peer, None).is_none());
        peer.hostname = "foreign".into();
        assert!(verified_address("http://ledboard.local", &peer, Some(actual)).is_none());
        peer.hostname = "ledboard".into();
        peer.http_peer_fencing = false;
        assert!(verified_address("http://ledboard.local", &peer, Some(actual)).is_none());
        peer.http_peer_fencing = true;
        for remote in ["192.0.2.10:81", "0.0.0.0:80", "239.1.1.1:80"] {
            assert!(
                verified_address(
                    "http://ledboard.local",
                    &peer,
                    Some(remote.parse().unwrap())
                )
                .is_none()
            );
        }
        for origin in [
            "http://ledboard.example",
            "http://127.0.0.1",
            "http://user@ledboard.local",
            "http://ledboard.local/api",
            "http://ledboard.local?secret=x",
            "http://ledboard.local#part",
        ] {
            assert!(verified_address(origin, &peer, Some(actual)).is_none());
        }
    }
    #[test]
    fn local_address_cache_bounds_replaces_and_invalidates_without_ttl_or_authority() {
        let mut cache = AddressCache::default();
        let first: IpAddr = "192.0.2.10".parse().unwrap();
        let replacement: IpAddr = "192.0.2.11".parse().unwrap();
        for n in 0..ADDRESS_CACHE_LIMIT + 4 {
            cache.remember(format!("board-{n}.local"), first);
        }
        assert_eq!(cache.entries.len(), ADDRESS_CACHE_LIMIT);
        assert!(
            !cache
                .entries
                .iter()
                .any(|(name, _)| name == "board-0.local")
        );
        cache.remember("board-4.local".into(), replacement);
        assert_eq!(cache.entries.len(), ADDRESS_CACHE_LIMIT);
        assert_eq!(
            cache.entries.back(),
            Some(&("board-4.local".into(), replacement))
        );
        assert_eq!(
            cache
                .entries
                .iter()
                .filter(|(name, _)| name == "board-4.local")
                .count(),
            1
        );
        cache.invalidate("board-4.local");
        assert!(
            !cache
                .entries
                .iter()
                .any(|(name, _)| name == "board-4.local")
        );
        let mut peer = status('a');
        peer.hostname = "new-address".into();
        let (_, address) = verified_address(
            "http://new-address.local:8787",
            &peer,
            Some(SocketAddr::new(replacement, 8787)),
        )
        .unwrap();
        cache.remember("new-address.local".into(), address);
        assert_eq!(cache.entries.back().unwrap().1, replacement);
    }
    #[test]
    fn local_relookup_marks_retain_verified_hints_until_success_or_explicit_invalidation() {
        let mut cache = AddressCache::default();
        let old: IpAddr = "192.0.2.10".parse().unwrap();
        let fresh: IpAddr = "192.0.2.11".parse().unwrap();
        cache.remember("ledboard.local".into(), old);
        cache.request_relookup("ledboard.local");
        assert_eq!(cache.entries.back(), Some(&("ledboard.local".into(), old)));
        assert!(cache.relookup.contains("ledboard.local"));
        let mut status = status('a');
        status.hostname = "ledboard".into();
        let (domain, address) = verified_address(
            "http://ledboard.local",
            &status,
            Some(SocketAddr::new(fresh, 80)),
        )
        .unwrap();
        cache.remember(domain, address);
        assert_eq!(
            cache.entries.back(),
            Some(&("ledboard.local".into(), fresh))
        );
        assert!(!cache.relookup.contains("ledboard.local"));
        cache.request_relookup("ledboard.local");
        cache.invalidate("ledboard.local");
        assert!(cache.entries.is_empty() && cache.relookup.is_empty());
        for n in 0..ADDRESS_CACHE_LIMIT * 2 {
            let domain = format!("board-{n}.local");
            cache.remember(domain.clone(), old);
            cache.request_relookup(&domain);
            cache.request_relookup(&format!("unknown-{n}.local"));
        }
        assert_eq!(cache.entries.len(), ADDRESS_CACHE_LIMIT);
        assert_eq!(cache.relookup.len(), ADDRESS_CACHE_LIMIT);
        assert!(
            cache
                .relookup
                .iter()
                .all(|name| cache.entries.iter().any(|(key, _)| key == name))
        );
        assert!(!cache.relookup.contains("board-0.local"));
    }
    #[test]
    fn local_verified_address_override_preserves_http_hostname_and_explicit_port() {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let address = listener.local_addr().unwrap();
        let origin = format!("http://cache-route-fixture.local:{}", address.port());
        let mut peer = status('a');
        peer.hostname = "cache-route-fixture".into();
        let body = serde_json::to_vec(&peer).unwrap();
        let server = std::thread::spawn(move || {
            let mut requests = Vec::new();
            let started = Instant::now();
            while requests.len() < 2 {
                let (mut socket, _) = match listener.accept() {
                    Ok(connection) => connection,
                    Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => {
                        assert!(
                            started.elapsed() < Duration::from_secs(6),
                            "Expected two bounded loopback requests"
                        );
                        std::thread::sleep(Duration::from_millis(10));
                        continue;
                    }
                    Err(error) => panic!("{error}"),
                };
                socket
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let mut request = Vec::new();
                while !request.windows(4).any(|v| v == b"\r\n\r\n") {
                    let mut chunk = [0; 1024];
                    let count = socket.read(&mut chunk).unwrap();
                    assert!(count > 0 && request.len() + count <= 8192);
                    request.extend_from_slice(&chunk[..count]);
                }
                requests.push(String::from_utf8(request).unwrap());
                write!(
                    socket,
                    "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                    body.len()
                )
                .unwrap();
                socket.write_all(&body).unwrap();
            }
            requests
        });
        // Bootstrap an explicit simulated name resolution. Only the actual TCP
        // response below, not this fixture override or status.ip, seeds cache.
        let response = Client::builder()
            .no_proxy()
            .timeout(Duration::from_secs(2))
            .resolve("cache-route-fixture.local", address)
            .build()
            .unwrap()
            .get(format!("{origin}/api/status"))
            .send()
            .unwrap();
        let remote = response.remote_addr();
        let observed: DeviceStatus = response.json().unwrap();
        remember_address(&origin, &observed, remote);
        let again: DeviceStatus = device_client_builder()
            .build()
            .unwrap()
            .get(format!("{origin}/api/status"))
            .send()
            .unwrap()
            .json()
            .unwrap();
        invalidate_address(&origin);
        assert_eq!(again.hostname, "cache-route-fixture");
        for request in server.join().unwrap() {
            assert!(request.to_ascii_lowercase().contains(&format!(
                "host: cache-route-fixture.local:{}\r\n",
                address.port()
            )));
        }
    }
    #[test]
    fn shared_pin_validates_fence_and_complete_identity() {
        assert!(Pin::from_status(&status('a')).is_some());
        for mut s in [status('0'), status('A'), status('x')] {
            assert!(Pin::from_status(&s).is_none());
            s.boot_id.clear();
            assert!(Pin::from_status(&s).is_none());
        }
        for board in ["", "UPPER", "space name", "bad\n", &"a".repeat(65)] {
            let mut s = status('a');
            s.board_id = board.into();
            assert!(Pin::from_status(&s).is_none());
        }
        let mut s = status('a');
        s.http_peer_fencing = false;
        assert!(pinned_client(&s).is_err());
    }
    #[test]
    fn queued_admission_is_revoked_after_boot_or_endpoint_change() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut queue = Queue::new(tx);
        assert!(queue.observe(&status('a')));
        assert!(!queue.observe(&status('a')));
        let endpoint = crate::Endpoint {
            base: "http://ledboard.local".into(),
            user: "fixture".into(),
            password: "private".into(),
        };
        queue.send(HttpCommand::Status(endpoint.clone())).unwrap();
        let HttpCommand::Scoped(old, _) = rx.recv().unwrap() else {
            panic!()
        };
        assert!(old.admit().is_ok());
        assert!(queue.observe(&status('b')));
        assert!(!old.current());
        assert!(old.admit().is_err());
        queue.send(HttpCommand::Status(endpoint)).unwrap();
        let HttpCommand::Scoped(new, _) = rx.recv().unwrap() else {
            panic!()
        };
        assert!(new.admit().is_ok());
        queue.clear();
        assert!(!new.current());
        assert!(!queue.writable());
    }
    #[test]
    fn stale_queued_work_expires_but_inflight_receipts_do_not_expire() {
        let mut scope = Scope {
            pin: Pin::from_status(&status('a')),
            generation: 1,
            clock: Arc::new(AtomicU64::new(1)),
            queued_at: Instant::now() - Duration::from_secs(9),
        };
        assert!(scope.admit().is_err());
        assert!(scope.admit_read().is_ok()); // A queued read is not a stale mutation.
        assert!(scope.current());
        scope.queued_at = Instant::now() - Duration::from_secs(25);
        assert!(scope.admit_read().is_err());
        scope.queued_at = Instant::now();
        assert!(scope.admit().is_ok());
        scope.clock.store(2, Ordering::Release);
        assert!(scope.admit().is_err());
        assert!(scope.admit_read().is_err());
    }
    #[test]
    fn retired_receipts_never_expose_another_peers_secrets() {
        let receipt = retired_receipt(&Event::OperationFailed(
            "private password error body".into(),
        ))
        .unwrap();
        assert!(!receipt.contains("password"));
        assert!(
            retired_receipt(&Event::Configuration(
                "/api/cloud",
                serde_json::json!({"private":"secret"})
            ))
            .is_none()
        );
    }
    #[test]
    fn retired_endpoint_receipts_are_kept_without_payload_or_credentials() {
        let scope = scope();
        let endpoint = crate::Endpoint {
            base: "http://previous.local".into(),
            user: "private-user".into(),
            password: "private-password".into(),
        };
        let event = Event::ForEndpoint(
            endpoint,
            Box::new(Event::ForScope(
                scope.clone(),
                Box::new(Event::OperationFailed("private response".into())),
            )),
        );
        assert!(retired_envelope(&event).is_none());
        scope.clock.fetch_add(1, Ordering::AcqRel);
        let receipt = retired_envelope(&event).unwrap();
        assert!(receipt.contains("Command failure"));
        for secret in [
            "previous.local",
            "private-user",
            "private-password",
            "private response",
        ] {
            assert!(!receipt.contains(secret));
        }
    }
    fn scope() -> Scope {
        Scope {
            pin: Pin::from_status(&status('a')),
            generation: 0,
            clock: Arc::new(AtomicU64::new(0)),
            queued_at: Instant::now(),
        }
    }
    fn wire_status(boot: char, features: &[&str]) -> Vec<u8> {
        serde_json::to_vec(
            &serde_json::json!({"hostname":"ledboard","board_id":"esp8266-aabbccddeeff",
            "boot_id":boot.to_string().repeat(32),"http_peer_fencing":true,
            "capabilities":{"board_kind":"table","acknowledged_features":features}}),
        )
        .unwrap()
    }
    fn client() -> Client {
        Client::builder()
            .no_proxy()
            .timeout(Duration::from_secs(3))
            .build()
            .unwrap()
    }
    #[test]
    fn gui_settings_pin_every_nested_request_and_verify_same_boot() {
        let network = crate::tests::fixture_network();
        let s = wire_status('a', &["network"]);
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, s.clone()),
            (202, br#"{"accepted":true}"#.to_vec()),
            (200, serde_json::to_vec(&network).unwrap()),
            (200, s),
        ]);
        let write =
            crate::device_settings::Write::network(41, &network, "fixture-private", false).unwrap();
        let result = crate::scoped_http_request(
            &client(),
            &scope(),
            &HttpCommand::Settings(endpoint, write),
        )
        .unwrap();
        assert!(matches!(result, Event::SettingsFinished(41, _, Ok(_))));
        let requests = server.join().unwrap();
        assert_eq!(requests.len(), 4);
        assert!(requests[1].starts_with("POST /api/network "));
        assert!(requests[2].starts_with("GET /api/network "));
        for request in &requests[1..3] {
            let request = request.to_lowercase();
            assert!(request.contains("x-pakar-board: esp8266-aabbccddeeff"));
            assert!(request.contains(&format!("x-pakar-boot: {}", "a".repeat(32))));
        }
    }
    #[test]
    fn gui_preflight_reboot_or_capability_loss_never_dispatches_mutation() {
        for response in [wire_status('b', &["leds"]), wire_status('a', &[])] {
            let (endpoint, server) = crate::tests::mock_device(vec![(200, response)]);
            assert!(
                crate::scoped_http_request(
                    &client(),
                    &scope(),
                    &HttpCommand::Post(endpoint, "/api/brightness", vec![])
                )
                .is_err()
            );
            let requests = server.join().unwrap();
            assert_eq!(requests.len(), 1);
            assert!(requests[0].starts_with("GET /api/status "));
        }
    }
    #[test]
    fn gui_server_boot_conflict_is_not_replayed() {
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, wire_status('a', &["leds"])),
            (409, br#"{"error":"peer_changed"}"#.to_vec()),
        ]);
        assert!(
            crate::scoped_http_request(
                &client(),
                &scope(),
                &HttpCommand::Post(endpoint, "/api/brightness", vec![])
            )
            .is_err()
        );
        assert_eq!(server.join().unwrap().len(), 2);
    }
    #[test]
    fn gui_readback_from_new_boot_is_not_applied() {
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, wire_status('a', &[])),
            (200, b"{}".to_vec()),
            (200, wire_status('b', &[])),
        ]);
        let error = crate::scoped_http_request(
            &client(),
            &scope(),
            &HttpCommand::Configuration(endpoint, "/api/network"),
        )
        .err()
        .unwrap();
        assert!(error.contains("changed during readback"));
        assert_eq!(server.join().unwrap().len(), 3);
    }
    #[test]
    fn gui_revocation_during_preflight_stops_before_write() {
        let scope = scope();
        let clock = scope.clock.clone();
        let (endpoint, server) = crate::tests::mock_device_with_hook(
            vec![(200, wire_status('a', &["leds"]))],
            Box::new(move |_| {
                clock.fetch_add(1, Ordering::AcqRel);
            }),
        );
        let error = crate::scoped_http_request(
            &client(),
            &scope,
            &HttpCommand::Post(endpoint, "/api/brightness", vec![]),
        )
        .err()
        .unwrap();
        assert!(error.contains("before dispatch"));
        assert_eq!(server.join().unwrap().len(), 1);
    }
    #[test]
    fn gui_revocation_after_dispatch_reports_uncertainty_not_success() {
        let scope = scope();
        let clock = scope.clock.clone();
        let (endpoint, server) = crate::tests::mock_device_with_hook(
            vec![(200, wire_status('a', &["leds"])), (202, b"{}".to_vec())],
            Box::new(move |n| {
                if n == 2 {
                    clock.fetch_add(1, Ordering::AcqRel);
                }
            }),
        );
        let error = crate::scoped_http_request(
            &client(),
            &scope,
            &HttpCommand::Post(endpoint, "/api/brightness", vec![]),
        )
        .err()
        .unwrap();
        assert!(error.contains("outcome unknown"));
        assert_eq!(server.join().unwrap().len(), 2);
    }
    #[test]
    fn gui_unfenced_mutation_fails_before_network_and_preserves_correlation() {
        let mut scope = scope();
        scope.pin = None;
        let endpoint = crate::Endpoint {
            base: "http://127.0.0.1:1".into(),
            user: "fixture".into(),
            password: "private".into(),
        };
        let write = crate::device_settings::Write::network(
            41,
            &crate::tests::fixture_network(),
            "fixture-private",
            false,
        )
        .unwrap();
        let command = HttpCommand::Scoped(scope, Box::new(HttpCommand::Settings(endpoint, write)));
        let error = crate::http_request(&client(), command.clone())
            .err()
            .unwrap();
        assert!(error.starts_with("Read-only:"));
        let Event::ForScope(_, inner) = command.failure(error) else {
            panic!()
        };
        assert!(matches!(*inner, Event::SettingsFinished(41, _, Err(_))));
        assert!(!inner.http_succeeded());
    }
}
