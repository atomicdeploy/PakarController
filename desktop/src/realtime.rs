//! Correlated, fenced commands: WS first, same-intent HTTP recovery only.
use crate::{DeviceStatus, Endpoint, Event, cell_batch, connection, peer_http};
use std::{
    sync::mpsc::Sender,
    time::{Duration, Instant},
};
#[derive(Clone)]
pub struct Intent {
    pub endpoint: Endpoint,
    pub scope: peer_http::Scope,
    pub stamp: connection::Stamp,
    pub id: String,
    pub packet: String,
    pub peer: peer_http::Pin,
    pub batch: Option<cell_batch::Correlation>,
    pub at: Instant,
    path: &'static str,
    form: Vec<(String, String)>,
}
pub fn supported(status: &DeviceStatus, path: &str) -> bool {
    status.command_clock_ms.is_some()
        && status
            .capabilities
            .as_ref()
            .is_some_and(|c| c.has("command-ack"))
        && matches!(
            path,
            "/api/cells"
                | "/api/cell"
                | "/api/effect"
                | "/api/mode"
                | "/api/brightness"
                | "/api/led/output"
        )
}

pub fn older_than(value: &DeviceStatus, current: Option<&DeviceStatus>) -> bool {
    current.is_some_and(|old| !value.board_id.is_empty() && !value.boot_id.is_empty() &&
        value.board_id==old.board_id && value.boot_id==old.boot_id &&
        matches!((value.command_clock_ms,old.command_clock_ms),(Some(now),Some(previous)) if now<previous))
}
impl Intent {
    pub fn new(
        endpoint: Endpoint,
        status: &DeviceStatus,
        path: &'static str,
        form: Vec<(String, String)>,
        scope: peer_http::Scope,
        stamp: connection::Stamp,
        batch: Option<cell_batch::Correlation>,
    ) -> Result<Self, String> {
        if !supported(status, path)
            || status.programming_latch
            || !status
                .capabilities
                .as_ref()
                .is_some_and(|c| c.allows(path, &form))
        {
            return Err("Realtime command capability unavailable".into());
        }
        let peer = peer_http::Pin::from_status(status)
            .ok_or("Realtime command requires a pinned board")?;
        if scope.pin.as_ref() != Some(&peer) || !stamp.current() {
            return Err("Command owner retired".into());
        }
        let id = uuid::Uuid::new_v4().simple().to_string();
        let command = match path {
            "/api/cells" => "cells",
            "/api/cell" => "cell",
            "/api/effect" => "effect",
            "/api/mode" => "mode",
            "/api/brightness" => "brightness",
            _ => "led-output",
        };
        let mut value = serde_json::json!({"type":"command","request_id":id,"board_id":status.board_id,
            "boot_id":status.boot_id,"expires_at_ms":status.command_clock_ms.unwrap().checked_add(8000).ok_or("Command clock overflow")?,"command":command});
        for (key, item) in &form {
            value[key] = if key == "id" || path == "/api/brightness" && key == "value" {
                serde_json::json!(item.parse::<u32>().map_err(|_| "Invalid command number")?)
            } else if key == "enabled" {
                serde_json::json!(item == "true")
            } else {
                serde_json::json!(item)
            };
        }
        Ok(Self {
            endpoint,
            scope,
            stamp,
            id,
            packet: value.to_string(),
            peer,
            batch,
            at: Instant::now(),
            path,
            form,
        })
    }
    pub fn admit(&self) -> Result<(), String> {
        if !self.stamp.current() {
            return Err("Connection retired; command not replayed".into());
        }
        if self.at.elapsed() >= Duration::from_secs(8) {
            return Err("Realtime intent expired; nothing replayed".into());
        }
        self.scope.admit()
    }
    pub fn verify(&self, value: serde_json::Value) -> Result<DeviceStatus, String> {
        if value["type"] != "command-result"
            || value["request_id"] != self.id
            || !self.peer.matches_identity(
                value["board_id"].as_str().unwrap_or_default(),
                value["boot_id"].as_str().unwrap_or_default(),
            )
        {
            return Err("Foreign command receipt; nothing replayed".into());
        }
        if value["accepted"] != true {
            return Err(format!(
                "Command rejected: {}",
                value["error"].as_str().unwrap_or("unknown")
            ));
        }
        let status: DeviceStatus = serde_json::from_value(value["status"].clone())
            .map_err(|_| "Command receipt has no status readback")?;
        if peer_http::Pin::from_status(&status).as_ref() != Some(&self.peer) {
            return Err("Command readback belongs to another boot".into());
        }
        let get = |key: &str| {
            self.form
                .iter()
                .find(|(k, _)| k == key)
                .map(|(_, v)| v.as_str())
                .unwrap_or_default()
        };
        let matches = match self.path {
            "/api/cells" => {
                let request = cell_batch::Request::new(
                    &crate::ui::cell_search::parse(get("ids"))?,
                    get("state"),
                    get("effect"),
                )?;
                value["count"].as_u64() == Some(request.ids.len() as u64) && request.verify(&status)
            }
            "/api/cell" => get("id")
                .parse::<usize>()
                .ok()
                .filter(|id| (1..=80).contains(id))
                .is_some_and(|id| {
                    status
                        .cells
                        .get(id - 1)
                        .is_some_and(|state| state == get("state"))
                }),
            "/api/effect" => status.effect == get("value"),
            "/api/mode" => status.mode == get("value"),
            "/api/brightness" => get("value").parse::<u8>().ok() == Some(status.brightness),
            _ => {
                (get("enabled").is_empty()
                    || status.led_output["enabled"].as_bool() == Some(get("enabled") == "true"))
                    && (get("mode").is_empty()
                        || status.led_output["mode"].as_str() == Some(get("mode")))
            }
        };
        if !matches {
            return Err("Command ACK received but readback differs; nothing replayed".into());
        }
        Ok(status)
    }
    pub fn http(&self) -> Result<DeviceStatus, String> {
        self.admit()?;
        let response = self
            .peer
            .client()?
            .post(format!(
                "{}/api/command",
                self.endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&self.endpoint.user, Some(&self.endpoint.password))
            .form(&[("packet", self.packet.as_str())])
            .send()
            .map_err(|_| "Same-intent HTTP recovery unavailable; outcome unknown")?;
        if !response.status().is_success() {
            return Err(format!(
                "Command recovery rejected: HTTP {}",
                response.status()
            ));
        }
        self.verify(
            response
                .json()
                .map_err(|_| "Invalid command recovery receipt")?,
        )
    }
    pub fn emit(&self, events: &Sender<Event>, result: Result<DeviceStatus, String>) {
        let send = |event| {
            let event = Event::ForScope(self.scope.clone(), Box::new(event));
            let event = crate::for_endpoint(&self.endpoint, event);
            let _ = events.send(Event::ForConnection(self.stamp.clone(), Box::new(event)));
        };
        match result {
            Ok(status) => {
                send(Event::Status(status));
                if let Some(batch) = &self.batch {
                    send(Event::CellBatchConfirmed(batch.clone()));
                } else {
                    send(Event::Accepted(
                        "Realtime command ACK and same-board readback verified".into(),
                    ));
                }
            }
            Err(error) => send(Event::OperationFailed(error)),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn status() -> DeviceStatus {
        serde_json::from_value(
            serde_json::json!({"hostname":"ledboard","board_id":"esp32s3-fixture",
            "boot_id":"a".repeat(32),"http_peer_fencing":true,"command_clock_ms":1000,
            "cells":vec!["white";80],"capabilities":{"board_kind":"table",
                "acknowledged_features":["table","cell-batch","leds","command-ack"],
                "table":{"columns":10,"rows":8,"pixels_per_cell":6}}}),
        )
        .unwrap()
    }
    fn intent(endpoint: Endpoint) -> (Intent, connection::Gate, peer_http::Queue) {
        let gate = connection::Gate::default();
        let epoch = gate.select(Some(crate::Transport::WiFi));
        let (tx, _) = std::sync::mpsc::channel();
        let mut queue = peer_http::Queue::new(tx);
        let current = status();
        queue.observe(&current);
        let batch = cell_batch::Request::new(&[1, 80], "red", "immediate").unwrap();
        let request = Intent::new(
            endpoint,
            &current,
            "/api/cells",
            batch.form(),
            queue.capture(),
            gate.stamp(crate::Transport::WiFi, epoch),
            None,
        )
        .unwrap();
        (request, gate, queue)
    }
    fn receipt(request: &Intent) -> serde_json::Value {
        let mut observed = status();
        observed.cells[0] = "red".into();
        observed.cells[79] = "red".into();
        serde_json::json!({"type":"command-result","request_id":request.id,"board_id":observed.board_id,
            "boot_id":observed.boot_id,"accepted":true,"count":2,"status":observed})
    }
    fn endpoint() -> Endpoint {
        Endpoint {
            base: "http://127.0.0.1:1".into(),
            user: String::new(),
            password: String::new(),
        }
    }
    #[test]
    fn realtime_ack_requires_id_boot_count_and_every_selected_value() {
        let (request, _, _) = intent(endpoint());
        let good = receipt(&request);
        assert_eq!(request.verify(good.clone()).unwrap().cells[79], "red");
        for field in ["request_id", "board_id", "boot_id"] {
            let mut bad = good.clone();
            bad[field] = serde_json::json!("foreign");
            assert!(request.verify(bad).is_err());
        }
        let mut bad = good.clone();
        bad["status"]["boot_id"] = serde_json::json!("b".repeat(32));
        assert!(request.verify(bad).is_err());
        let mut bad = good.clone();
        bad["status"]["cells"][79] = serde_json::json!("white");
        assert!(request.verify(bad).is_err());
        let mut bad = good.clone();
        bad["count"] = serde_json::json!(1);
        assert!(request.verify(bad).is_err());
        let mut bad = good;
        bad["accepted"] = serde_json::json!(false);
        assert!(request.verify(bad).is_err());
    }
    #[test]
    fn realtime_disconnect_peer_retirement_and_expiry_cannot_dispatch_recovery() {
        let (mut request, _, mut queue) = intent(endpoint());
        assert!(request.admit().is_ok());
        request.at -= Duration::from_secs(9);
        assert!(request.http().is_err());
        request.at = Instant::now();
        queue.clear();
        assert!(request.http().is_err());
        let (request, gate, _) = intent(endpoint());
        gate.select(None);
        assert!(request.http().is_err());
    }
    #[test]
    fn realtime_http_recovery_sends_exact_original_envelope_with_no_status_preflight() {
        let (mut request, _, _) = intent(endpoint());
        let body = serde_json::to_vec(&receipt(&request)).unwrap();
        let (endpoint, server) = crate::tests::mock_device(vec![(200, body)]);
        request.endpoint = endpoint;
        assert_eq!(request.http().unwrap().cells[79], "red");
        let requests = server.join().unwrap();
        assert_eq!(requests.len(), 1);
        assert!(requests[0].starts_with("POST /api/command "));
        let body = requests[0].split_once("\r\n\r\n").unwrap().1;
        let encoded = reqwest::Url::parse(&format!("http://fixture/?{body}")).unwrap();
        let fields = encoded.query_pairs().collect::<Vec<_>>();
        assert_eq!(fields.len(), 1);
        assert_eq!(fields[0].0, "packet");
        assert_eq!(fields[0].1, request.packet);
        assert!(
            requests[0]
                .to_lowercase()
                .contains(&format!("x-pakar-boot: {}", "a".repeat(32)))
        );
    }
    #[test]
    fn realtime_older_http_observation_cannot_undo_newer_command_state() {
        let old = status();
        let mut newer = old.clone();
        newer.command_clock_ms = Some(1001);
        assert!(older_than(&old, Some(&newer)));
        assert!(!older_than(&newer, Some(&old)));
        newer.boot_id = "b".repeat(32);
        assert!(!older_than(&old, Some(&newer)));
    }
    #[test]
    fn realtime_actual_ws_worker_delivers_receipt_without_http_or_poll_tick() {
        use std::sync::mpsc;
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = Endpoint {
            base: format!("http://{}", listener.local_addr().unwrap()),
            ..endpoint()
        };
        let (request, gate, _queue) = intent(endpoint.clone());
        let expected = receipt(&request);
        let initial = status();
        let packet = request.packet.clone();
        let server = std::thread::spawn(move || {
            let (stream, _) = listener.accept().unwrap();
            stream
                .set_read_timeout(Some(Duration::from_secs(4)))
                .unwrap();
            let mut socket = tungstenite::accept_hdr(
                stream,
                |request: &tungstenite::handshake::server::Request,
                 mut response: tungstenite::handshake::server::Response| {
                    assert_eq!(request.headers()["sec-websocket-protocol"], "arduino");
                    response
                        .headers_mut()
                        .insert("sec-websocket-protocol", "arduino".parse().unwrap());
                    Ok(response)
                },
            )
            .unwrap();
            socket
                .send(tungstenite::Message::Text(
                    serde_json::to_string(&initial).unwrap().into(),
                ))
                .unwrap();
            // The worker may probe liveness before it consumes the initial push.
            // Answer that read-only request; the mutation must still arrive as the
            // exact original envelope, without waiting for a polling tick.
            let deadline = Instant::now() + Duration::from_secs(4);
            loop {
                assert!(Instant::now() < deadline, "Command did not arrive");
                let actual = socket.read().unwrap();
                let text = actual.to_text().unwrap();
                if text == r#"{"type":"status"}"# {
                    socket
                        .send(tungstenite::Message::Text(
                            serde_json::to_string(&initial).unwrap().into(),
                        ))
                        .unwrap();
                    continue;
                }
                assert_eq!(text, packet);
                break;
            }
            socket
                .send(tungstenite::Message::Text(expected.to_string().into()))
                .unwrap();
            let _ = socket.read();
        });
        let (tx, rx) = mpsc::channel();
        let (event_tx, event_rx) = mpsc::channel();
        let epoch = gate.epoch();
        let worker = std::thread::spawn(move || {
            crate::ws_worker(rx, event_tx, eframe::egui::Context::default(), gate)
        });
        fn unbox(event: Event) -> Event {
            match event {
                Event::ForConnection(_, inner)
                | Event::ForEndpoint(_, inner)
                | Event::ForScope(_, inner) => unbox(*inner),
                other => other,
            }
        }
        tx.send(crate::WsCommand::Connect(endpoint, epoch)).unwrap();
        loop {
            if matches!(
                unbox(event_rx.recv_timeout(Duration::from_secs(4)).unwrap()),
                Event::Status(_)
            ) {
                break;
            }
        }
        let start = Instant::now();
        tx.send(crate::WsCommand::Control(request)).unwrap();
        let mut readback = None;
        loop {
            match unbox(event_rx.recv_timeout(Duration::from_secs(4)).unwrap()) {
                Event::Status(observed) => readback = Some(observed),
                Event::Accepted(_) | Event::CellBatchConfirmed(_) => {
                    let observed = readback.as_ref().expect("Verified receipt has readback");
                    assert_eq!(observed.cells[0], "red");
                    assert_eq!(observed.cells[79], "red");
                    break;
                }
                Event::OperationFailed(error) => panic!("Command failed: {error}"),
                _ => {}
            }
        }
        assert!(
            start.elapsed() < Duration::from_millis(500),
            "No one-second polling delay"
        );
        drop(tx);
        worker.join().unwrap();
        server.join().unwrap();
    }
}
