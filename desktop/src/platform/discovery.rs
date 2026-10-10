//! Local controller discovery adapted from PCController's layered discovery.
//!
//! The human-facing endpoint remains the stable mDNS hostname. SSDP locations
//! are short-lived transport fallbacks and are accepted only after the normal
//! authenticated status request succeeds.

use std::{
    collections::BTreeSet,
    net::{Ipv4Addr, SocketAddrV4, UdpSocket},
    time::{Duration, Instant},
};

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
        "ST: ssdp:all\r\n\r\n"
    );
    if socket.send_to(request.as_bytes(), SSDP_ADDRESS).is_err() {
        return Vec::new();
    }
    let deadline = Instant::now() + timeout;
    let mut buffer = [0_u8; 2048];
    let mut locations = BTreeSet::new();
    while Instant::now() < deadline {
        let Ok((count, _source)) = socket.recv_from(&mut buffer) else {
            continue;
        };
        if let Some(location) = parse_location(&buffer[..count]) {
            locations.insert(location);
        }
    }
    locations.into_iter().collect()
}

fn parse_location(packet: &[u8]) -> Option<String> {
    let text = std::str::from_utf8(packet).ok()?;
    if !text.to_ascii_uppercase().starts_with("HTTP/1.1 200") {
        return None;
    }
    let location = text.lines().find_map(|line| {
        let (name, value) = line.split_once(':')?;
        name.trim()
            .eq_ignore_ascii_case("location")
            .then(|| value.trim())
    })?;
    let parsed = reqwest::Url::parse(location).ok()?;
    let host = parsed.host_str()?;
    let authority = parsed
        .port()
        .map(|port| format!("{host}:{port}"))
        .unwrap_or_else(|| host.to_owned());
    Some(format!("{}://{authority}", parsed.scheme()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extracts_origin_from_ssdp_location() {
        let response = b"HTTP/1.1 200 OK\r\nLOCATION: http://192.0.2.7:80/description.xml\r\nST: upnp:rootdevice\r\n\r\n";
        assert_eq!(
            parse_location(response).as_deref(),
            Some("http://192.0.2.7")
        );
    }

    #[test]
    fn rejects_non_responses() {
        assert!(parse_location(b"NOTIFY * HTTP/1.1\r\nLOCATION: http://192.0.2.7/\r\n").is_none());
    }
}
