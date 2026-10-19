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
        "ST: ssdp:all\r\nUSER-AGENT: PakarController/",env!("CARGO_PKG_VERSION")," native\r\n\r\n"
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
            if locations.len() < 16 { locations.insert(location); }
        }
    }
    locations.into_iter().filter(|v| lan::verified_origin(v)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

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
        assert!(lan::ssdp_origin(b"NOTIFY * HTTP/1.1\r\nLOCATION: http://192.0.2.7/\r\n", "192.0.2.7:1900".parse().unwrap()).is_none());
    }
}
