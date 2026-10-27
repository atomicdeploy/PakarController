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
    pub pixel_request: Option<u64>,
    pub correction_request: Option<crate::pixel_corrections::PreviewRequest>,
    path: &'static str,
    form: Vec<(String, String)>,
    minigame_revision: Option<u64>,
}
pub fn supported(status: &DeviceStatus, path: &str) -> bool {
    (!crate::minigame::path(path) || crate::minigame::ready(status))
        && (path != "/api/game/reset"
        || status
            .capabilities
            .as_ref()
            .is_some_and(|c| c.has("reset-ack")))
        && (path != crate::pixel_corrections::PREVIEW_PATH || status.capabilities.as_ref()
            .is_some_and(|c|c.has(crate::pixel_corrections::PREVIEW_COMMAND_FEATURE)))
        && (!matches!(path,"/api/audio"|"/api/buzzer"|"/api/outputs") || status.avr_online
            && status.capabilities.as_ref().is_some_and(|c|c.has("avr-command-ack")))
        && status.command_clock_ms.is_some()
        && status
            .capabilities
            .as_ref()
            .is_some_and(|c| c.has("command-ack"))
        && matches!(
            path,
            "/api/cells"
                | "/api/led/presentation"
                | "/api/led/preview"
                | "/api/game/reset"
                | "/api/game/settings"
                | "/api/game/press"
                | crate::minigame::CONFIGURE
                | crate::minigame::CONTROL
                | crate::minigame::INPUT
                | "/api/cells/assignments"
                | "/api/cells/lock"
                | "/api/cell"
                | "/api/effect"
                | "/api/mode"
                | "/api/brightness"
                | "/api/led/output"
                | "/api/led/diagnostic"
                | "/api/audio"
                | "/api/buzzer"
                | "/api/outputs"
                | crate::pixel_corrections::PREVIEW_PATH
        )
}

pub struct VerifiedReceipt {
    pub status: DeviceStatus,
    pub corrections: Option<crate::pixel_corrections::Configuration>,
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
        // Live status was admitted before reaching here. Recheck an advertised
        // receiver policy if it changed before this command was constructed.
        if status.compatibility.is_some() {
            crate::peer_identity::validate_board(status)?;
        }
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
        if crate::minigame::path(path) { crate::minigame::validate(status,path,&form)?; }
        if path == "/api/led/output" && let Some((_, backend)) = form.iter().find(|(key, _)| key == "backend") {
            if !crate::led_backend::allowed(status, backend) {
                return Err("LED backend unavailable or switching; nothing sent".into());
            }
        }
        if scope.pin.as_ref() != Some(&peer) || !stamp.current() {
            return Err("Command owner retired".into());
        }
        let id = uuid::Uuid::new_v4().simple().to_string();
        let correction_request = if path == crate::pixel_corrections::PREVIEW_PATH {
            Some(crate::pixel_corrections::PreviewRequest::from_form(status,&form)?)
        } else { None };
        if matches!(path,"/api/audio"|"/api/buzzer"|"/api/outputs") {
            let get=|name:&str|form.iter().find(|(key,_)|key==name).map(|(_,value)|value.as_str());
            let valid=match path {
                "/api/audio"=>form.len()==1 && get("cue").is_some_and(|cue|status.audio_cues.iter().any(|available|available==cue)),
                "/api/buzzer"=>form.len()==2 && get("frequency").is_some_and(|value|value.parse::<u16>().is_ok_and(|hz|(20..=12000).contains(&hz)))
                    && get("duration_ms").is_some_and(|value|value.parse::<u16>().is_ok_and(|ms|(1..=5000).contains(&ms))),
                "/api/outputs"=>form.len()==1 && get("mask").is_some_and(|value|value.parse::<u8>().is_ok()),
                _=>false,
            };
            if !valid {return Err("Invalid or unadvertised AVR command; nothing sent".into());}
        }
        if path == "/api/game/press" && (form.len() != 1 || form[0].0 != "key"
            || !form[0].1.parse::<u8>().is_ok_and(|key| (1..=9).contains(&key))) {
            return Err("Invalid key press".into());
        }
        if path == "/api/game/settings" && form.is_empty() {
            return Err("Empty game settings".into());
        }
        if path == "/api/game/settings"
            && form
                .iter()
                .any(|(name, _)| matches!(name.as_str(), "winner_cue" | "reset_cue"))
            && !status
                .capabilities
                .as_ref()
                .is_some_and(|c| c.has("game-melodies"))
        {
            return Err("Game melodies unavailable".into());
        }
        if path == "/api/game/settings"
            && !crate::usb_table::key_settings_network_verify(status, &form)
        {
            // Verify syntax here; actual values are checked again after ACK.
            for (name, text) in &form {
                let ok = match name.as_str() {
                    "presentation" => crate::game_presentation::parse(text).is_ok(),
                    "winner_cue" | "reset_cue" => crate::usb_table::game_cue_id(text).is_some(),
                    "chase_neighbors" | "chase_invert" | "extended_effects" => {
                        matches!(text.as_str(), "true" | "false")
                    }
                    "chase_period_ms" => text
                        .parse::<u32>()
                        .ok()
                        .is_some_and(|v| (400..=3000).contains(&v)),
                    "enabled_mask" => text
                        .parse::<u32>()
                        .ok()
                        .is_some_and(|v| (1..=511).contains(&v)),
                    "auto_reset_ms" => text.parse::<u32>().ok().is_some_and(|v| v <= 86400000),
                    "idle_rgb" | "win_rgb" | "lose_rgb" => {
                        text.parse::<u32>().ok().is_some_and(|v| v <= 0xffffff)
                    }
                    _ => false,
                };
                if !ok {
                    return Err("Invalid game setting".into());
                }
            }
        }
        let command = match path {
            crate::minigame::CONFIGURE => "minigame-configure",
            crate::minigame::CONTROL => "minigame-control",
            crate::minigame::INPUT => "minigame-input",
            "/api/led/presentation" => "state-presentation",
            "/api/led/preview" => "color-preview",
            "/api/game/press" => "game-press",
            "/api/game/reset" => "reset",
            "/api/game/settings" => "game-settings",
            "/api/cells" => "cells",
            "/api/cells/assignments" => "cell-assignments",
            "/api/cells/lock" => "cell-lock",
            "/api/cell" => "cell",
            "/api/effect" => "effect",
            "/api/mode" => "mode",
            "/api/brightness" => "brightness",
            "/api/led/diagnostic" => "pixel-diagnostic",
            "/api/audio" => "audio",
            "/api/buzzer" => "buzzer",
            "/api/outputs" => "outputs",
            crate::pixel_corrections::PREVIEW_PATH => "pixel-corrections-preview",
            _ => "led-output",
        };
        let mut value = serde_json::json!({"type":"command","request_id":id,"board_id":status.board_id,
            "boot_id":status.boot_id,"expires_at_ms":status.command_clock_ms.unwrap().checked_add(8000).ok_or("Command clock overflow")?,"command":command});
        for (key, item) in &form {
            value[key] = if key == "configuration" && path == crate::pixel_corrections::PREVIEW_PATH {
                correction_request.as_ref().ok_or("Missing correction preview")?.payload()
            } else if key == "settings" && path == crate::minigame::CONFIGURE {
                crate::minigame::settings(item,&status.minigame)?
            } else if key == "settings" && path == "/api/led/presentation" {
                crate::state_appearance::parse(item)?
            } else if key == "presentation" {
                crate::game_presentation::parse(item)?
            } else if key == "key" && matches!(path,"/api/game/press" | crate::minigame::INPUT) {
                serde_json::json!(item.parse::<u8>().ok().filter(|key| (1..=9).contains(key)).ok_or("Invalid key")?)
            } else if key == "assignments" {
                let assignments: Vec<crate::cell_assignments::Assignment> =
                    serde_json::from_str(item).map_err(|_| "Invalid assignments")?;
                serde_json::to_value(
                    crate::cell_assignments::Document::new(assignments)?.assignments,
                )
                .map_err(|e| e.to_string())?
            } else if key == "id"
                || path == "/api/buzzer" && matches!(key.as_str(), "frequency"|"duration_ms")
                || path == "/api/outputs" && key == "mask"
                || path == "/api/led/preview" && matches!(key.as_str(), "rgb" | "duration_ms")
                || path == "/api/brightness" && key == "value"
                || path == "/api/game/settings"
                    && matches!(
                        key.as_str(),
                        "enabled_mask"
                            | "idle_rgb"
                            | "win_rgb"
                            | "lose_rgb"
                            | "auto_reset_ms"
                            | "chase_period_ms"
                    )
                || path == "/api/led/diagnostic"
                    && matches!(key.as_str(), "index" | "count" | "rgb" | "group_layout_revision")
            {
                serde_json::json!(item.parse::<u32>().map_err(|_| "Invalid command number")?)
            } else if key == "enabled"
                || path == "/api/led/preview" && key == "cancel"
                || key == "off"
                || key == "extended_effects"
                || key == "chase_neighbors"
                || key == "chase_invert"
            {
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
            pixel_request: None,
            correction_request,
            path,
            form,
            minigame_revision: if crate::minigame::path(path) {status.minigame["revision"].as_u64()} else {None},
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
    pub fn verify(&self, value: serde_json::Value) -> Result<VerifiedReceipt, String> {
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
            let reason=value["reason"].as_str().or_else(||value["error"].as_str()).unwrap_or("unknown");
            return Err(if self.correction_request.is_some() {crate::pixel_corrections::rejection(reason)}
                else {crate::cell_assignments::rejection(reason)});
        }
        let status: DeviceStatus = serde_json::from_value(value["status"].clone())
            .map_err(|_| "Command receipt has no status readback")?;
        if status.compatibility.is_some() {
            crate::peer_identity::validate_board(&status)?;
        }
        if peer_http::Pin::from_status(&status).as_ref() != Some(&self.peer) {
            return Err("Command readback belongs to another boot".into());
        }
        if crate::minigame::path(self.path) && !matches!(
            (self.minigame_revision,status.minigame["revision"].as_u64()),
            (Some(before),Some(after)) if after>=before) {
            return Err("Game readback revision is missing or stale; nothing replayed".into());
        }
        if let Some(request) = &self.correction_request {
            self.admit()?;
            let summary=&value["pixel_corrections_preview"];
            let lease=summary["lease_id"].as_u64().and_then(|v|u32::try_from(v).ok());
            if summary["base_revision"].as_u64()!=Some(u64::from(request.value.revision))
                || value["corrections_revision"].as_u64()!=Some(u64::from(request.value.revision))
                || if request.clear {summary["active"]!=false || lease!=Some(0)} else {
                    summary["active"]!=true || lease.is_none_or(|id|id==0 || request.lease!=0&&id!=request.lease)
                    || summary["remaining_ms"].as_u64().is_none_or(|ms|ms==0||ms>crate::pixel_corrections::PREVIEW_TTL_MS)
                } {
                return Err("Correction preview receipt differs; nothing replayed".into());
            }
            let mut expected=request.clone();
            if !request.clear {expected.lease=lease.unwrap();}
            let corrections=crate::pixel_corrections::read_preview(&self.endpoint,&self.peer,&expected,||self.admit())?;
            return Ok(VerifiedReceipt{status,corrections:Some(corrections)});
        }
        let get = |key: &str| {
            self.form
                .iter()
                .find(|(k, _)| k == key)
                .map(|(_, v)| v.as_str())
                .unwrap_or_default()
        };
        if self.path == "/api/led/output" && !get("backend").is_empty() {
            return crate::led_backend::finish_network(&self.endpoint, status, get("backend"), || self.admit())
                .map(|status|VerifiedReceipt{status,corrections:None});
        }
        let matches = match self.path {
            crate::minigame::CONFIGURE | crate::minigame::CONTROL | crate::minigame::INPUT =>
                crate::minigame::verify(self.path,&self.form,&status),
            "/api/audio"|"/api/buzzer"=>value["avr_acknowledged"]==true,
            // ACK confirms the accepted command payload. active_outputs is a
            // separate AVR snapshot and may legitimately arrive a frame later.
            "/api/outputs"=>value["avr_acknowledged"]==true && value["acknowledged_mask"].as_u64()==get("mask").parse::<u8>().ok().map(u64::from),
            "/api/led/presentation" => crate::state_appearance::matches(
                &crate::state_appearance::parse(get("settings"))?, &status.state_presentation),
            "/api/led/preview" => if get("cancel")=="true" {
                status.color_preview["active"]==false
            }else{status.color_preview["active"]==true && status.color_preview["rgb"].as_u64()==get("rgb").parse().ok()},
            "/api/game/press" => value["avr_acknowledged"] == true,
            "/api/game/reset" => value["avr_acknowledged"] == true,
            "/api/game/settings" => {
                crate::usb_table::key_settings_network_verify(&status, &self.form)
            }
            "/api/cells/assignments" => {
                let assignments: Vec<crate::cell_assignments::Assignment> =
                    serde_json::from_str(get("assignments")).map_err(|_| "Invalid assignments")?;
                let doc = crate::cell_assignments::Document::new(assignments)?;
                value["count"].as_u64() == Some(doc.assignments.len() as u64) && doc.verify(&status)
            }
            "/api/cells/lock" => status.red_lock == Some(get("enabled") == "true"),
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
                        .is_some_and(|state| crate::cell_assignments::same_state(state, get("state")))
                }),
            "/api/effect" => status.effect == get("value"),
            "/api/mode" => status.mode == get("value"),
            "/api/brightness" => get("value").parse::<u8>().ok() == Some(status.brightness),
            "/api/led/diagnostic" => {
                let (probe, off) = crate::pixel_diagnostic::Probe::from_form(&status, &self.form)
                    .ok_or("Diagnostic layout changed or requested span is invalid")?;
                probe.matches(&status, off)
            }
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
        Ok(VerifiedReceipt{status,corrections:None})
    }
    pub fn http(&self) -> Result<VerifiedReceipt, String> {
        self.admit()?;
        if self.correction_request.is_some() {
            return Err("Correction preview outcome unknown; no fallback mutation or command replayed".into());
        }
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
    pub fn emit(&self, events: &Sender<Event>, result: Result<VerifiedReceipt, String>) {
        let send = |event| {
            let event = Event::ForScope(self.scope.clone(), Box::new(event));
            let event = crate::for_endpoint(&self.endpoint, event);
            let _ = events.send(Event::ForConnection(self.stamp.clone(), Box::new(event)));
        };
        if let Some(request) = &self.correction_request {
            let result=result.and_then(|receipt| {
                send(Event::Status(receipt.status));
                receipt.corrections.ok_or_else(||"Missing correction preview readback".to_owned())
            });
            send(Event::CorrectionPreviewFinished(request.id,result));
            return;
        }
        let result=result.map(|receipt|receipt.status);
        if let Some(id) = self.pixel_request {
            if let Ok(status) = &result {
                send(Event::Status(status.clone()));
            }
            send(Event::PixelCommandFinished(id, result));
            return;
        }
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
            serde_json::json!({"hostname":"ledboard","board_id":"esp32s3-fixture","version":crate::peer_identity::VERSION,
            "compatibility":crate::peer_identity::advertised_policy(),
            "boot_id":"a".repeat(32),"http_peer_fencing":true,"command_clock_ms":1000,
            "cells":vec!["white";80],"capabilities":{"board_kind":"table","modes":[{"id":"cell","code":1,"kind":"presentation"}],
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
    #[test]
    fn minigame_intents_remain_typed_pinned_and_readback_verified() {
        let mut current=status();
        current.capabilities.as_mut().unwrap().role="first-wins".into();
        current.minigame=serde_json::json!({"ready":true,"revision":1,"phase":"ready",
            "available_modes":[{"id":"simon","label":"Simon"}],"settings":{"mode":"simon","rounds":5}});
        let gate=connection::Gate::default();let epoch=gate.select(Some(crate::Transport::WiFi));
        let (tx,_)=std::sync::mpsc::channel();let mut queue=peer_http::Queue::new(tx);queue.observe(&current);
        let form=vec![("settings".into(),"{\"rounds\":7}".into())];
        let request=Intent::new(endpoint(),&current,crate::minigame::CONFIGURE,form,
            queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).unwrap();
        let packet:serde_json::Value=serde_json::from_str(&request.packet).unwrap();
        assert_eq!(packet["command"],"minigame-configure");assert_eq!(packet["settings"]["rounds"],7);
        assert_eq!(packet["board_id"],current.board_id);assert!(packet["expires_at_ms"].as_u64().is_some());
        let mut result=serde_json::json!({"type":"command-result","request_id":request.id,
            "board_id":current.board_id,"boot_id":current.boot_id,"accepted":true,"status":current});
        assert!(request.verify(result.clone()).is_err());
        result["status"]["minigame"]["settings"]["rounds"]=serde_json::json!(7);
        assert!(request.verify(result.clone()).is_ok());
        result["boot_id"]=serde_json::json!("foreign");assert!(request.verify(result).is_err());
        current.minigame["ready"]=serde_json::json!(false);
        assert!(!supported(&current,crate::minigame::CONTROL));
    }
    #[test]
    fn game_settings_use_typed_websocket_intent_and_selected_field_readback() {
        let mut current = status();
        current.pixels = 216;
        current.mode = "first-wins".into();
        current.capabilities.as_mut().unwrap().role = "first-wins".into();
        current.capabilities.as_mut().unwrap().modes = vec![crate::capabilities::Mode {
            id: "first-wins".into(),
            code: 2,
            label: "First wins".into(),
            kind: "game".into(),
        }];
        current
            .capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .extend(["inputs".into(), "outputs".into(), "key-settings".into()]);
        current.key_game =
            serde_json::json!({"enabled_mask":3,"win_rgb":65280,"extended_effects":false});
        let gate = connection::Gate::default();
        let epoch = gate.select(Some(crate::Transport::WiFi));
        let (tx, _) = std::sync::mpsc::channel();
        let mut queue = peer_http::Queue::new(tx);
        queue.observe(&current);
        let selected = vec![
            ("win_rgb".into(), "255".into()),
            ("extended_effects".into(), "true".into()),
        ];
        assert!(supported(&current, "/api/game/settings"));
        let request = Intent::new(
            endpoint(),
            &current,
            "/api/game/settings",
            selected,
            queue.capture(),
            gate.stamp(crate::Transport::WiFi, epoch),
            None,
        )
        .unwrap();
        let packet: serde_json::Value = serde_json::from_str(&request.packet).unwrap();
        assert_eq!(packet["command"], "game-settings");
        assert_eq!(packet["win_rgb"], 255);
        assert_eq!(packet["extended_effects"], true);
        assert!(
            packet.get("enabled_mask").is_none(),
            "an unrelated mask must not overwrite a peer's change"
        );
        let mut observed = current.clone();
        observed.key_game["enabled_mask"] = 15.into(); // Another client changed this concurrently.
        observed.key_game["win_rgb"] = 255.into();
        observed.key_game["extended_effects"] = true.into();
        let receipt = |status: &DeviceStatus| {
            serde_json::json!({"type":"command-result","request_id":request.id,
            "board_id":status.board_id,"boot_id":status.boot_id,"accepted":true,"status":status})
        };
        assert!(request.verify(receipt(&observed)).is_ok());
        observed.key_game["win_rgb"] = 65280.into();
        assert!(request.verify(receipt(&observed)).is_err());
        assert!(
            crate::usb_table::key_settings_patch(&[("extended_effects".into(), "maybe".into())])
                .is_err()
        );
    }
    fn endpoint() -> Endpoint {
        Endpoint {
            base: "http://127.0.0.1:1".into(),
            user: String::new(),
            password: String::new(),
        }
    }
    fn correction_intent() -> (Intent,DeviceStatus,connection::Gate,peer_http::Queue) {
        let mut current=status();current.pixels=6;
        current.capabilities.as_mut().unwrap().acknowledged_features.extend([
            crate::pixel_corrections::PREVIEW_FEATURE.into(),crate::pixel_corrections::PREVIEW_COMMAND_FEATURE.into()]);
        let gate=connection::Gate::default();let epoch=gate.select(Some(crate::Transport::WiFi));
        let (tx,_)=std::sync::mpsc::channel();let mut queue=peer_http::Queue::new(tx);queue.observe(&current);
        let form=vec![("configuration".into(),serde_json::json!({"revision":7,"enabled":true,
            "rules":[{"kind":"black","pixel":0,"offset":0,"enabled":true}],"lease_id":0,"ttl_ms":10000}).to_string())];
        let mut request=Intent::new(endpoint(),&current,crate::pixel_corrections::PREVIEW_PATH,form,
            queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).unwrap();
        request.correction_request.as_mut().unwrap().id=31;
        (request,current,gate,queue)
    }
    fn correction_receipt(request:&Intent,status:&DeviceStatus) -> serde_json::Value {
        serde_json::json!({"type":"command-result","request_id":request.id,"accepted":true,
            "board_id":status.board_id,"boot_id":status.boot_id,"status":status,"corrections_revision":7,
            "pixel_corrections_preview":{"active":true,"lease_id":42,"base_revision":7,"remaining_ms":9500}})
    }
    #[test]
    fn pixel_corrections_realtime_typed_receipt_requires_authoritative_mapping() {
        let (mut request,current,_gate,_queue)=correction_intent();
        let packet:serde_json::Value=serde_json::from_str(&request.packet).unwrap();
        assert_eq!(packet["command"],"pixel-corrections-preview");
        assert!(packet["configuration"].is_object());assert_eq!(packet["configuration"]["rules"][0]["pixel"],0);
        let config=serde_json::json!({"revision":7,"enabled":true,"rules":[],"pixels":6,"sources":[0,1,2,3,4,5],
            "active_sources":[-1,1,2,3,4,5],"preview":{"active":true,"lease_id":42,"base_revision":7,"remaining_ms":9000}});
        let (endpoint,server)=crate::tests::mock_device(vec![(200,serde_json::to_vec(&config).unwrap()),
            (200,serde_json::to_vec(&current).unwrap())]);request.endpoint=endpoint;
        let result=request.verify(correction_receipt(&request,&current));
        assert!(result.is_ok());
        let (tx,rx)=std::sync::mpsc::channel();request.emit(&tx,result);
        let unwrap=|mut event:Event|{loop{event=match event {
            Event::ForScope(_,inner)|Event::ForEndpoint(_,inner)|Event::ForConnection(_,inner)=>*inner,
            other=>break other,
        }} };
        assert!(matches!(unwrap(rx.recv().unwrap()),Event::Status(_)));
        assert!(matches!(unwrap(rx.recv().unwrap()),Event::CorrectionPreviewFinished(31,Ok(value)) if value.preview.lease_id==42));
        assert!(rx.try_recv().is_err(),"No repetitive success toast for each preview renewal");
        let requests=server.join().unwrap();assert_eq!(requests.len(),2);
        assert!(requests[0].starts_with("GET /api/pixels/corrections "));
        assert!(requests.iter().all(|request|request.starts_with("GET ")));
    }
    #[test]
    fn pixel_corrections_realtime_rejection_or_foreign_lease_never_falls_back() {
        let (mut request,current,_gate,_queue)=correction_intent();
        let mut rejected=correction_receipt(&request,&current);rejected["accepted"]=false.into();
        rejected["reason"]="corrections_preview_owned".into();
        assert!(request.verify(rejected).err().unwrap().contains("corrections_preview_owned"));
        assert!(request.http().err().unwrap().contains("no fallback mutation"));
        request.correction_request.as_mut().unwrap().lease=41;
        assert!(request.verify(correction_receipt(&request,&current)).err().unwrap().contains("receipt differs"));
        let mut unsupported=current.clone();unsupported.capabilities.as_mut().unwrap().acknowledged_features
            .retain(|feature|feature!=crate::pixel_corrections::PREVIEW_COMMAND_FEATURE);
        assert!(!supported(&unsupported,crate::pixel_corrections::PREVIEW_PATH));
    }
    #[test]
    fn realtime_avr_fast_commands_require_typed_bounds_and_actual_ack() {
        let mut current=status();current.avr_online=true;current.audio_cues=vec!["winner".into()];current.active_outputs=5;
        current.capabilities.as_mut().unwrap().acknowledged_features.extend(["buzzer".into(),"outputs".into(),"avr-command-ack".into()]);
        let gate=connection::Gate::default();let epoch=gate.select(Some(crate::Transport::WiFi));
        let (tx,_)=std::sync::mpsc::channel();let mut queue=peer_http::Queue::new(tx);queue.observe(&current);
        for (path,form,command) in [
            ("/api/audio",vec![("cue".into(),"winner".into())],"audio"),
            ("/api/buzzer",vec![("frequency".into(),"1000".into()),("duration_ms".into(),"70".into())],"buzzer"),
            ("/api/outputs",vec![("mask".into(),"5".into())],"outputs"),
        ] {
            let request=Intent::new(endpoint(),&current,path,form.clone(),queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).unwrap();
            let packet:serde_json::Value=serde_json::from_str(&request.packet).unwrap();assert_eq!(packet["command"],command);
            if path=="/api/buzzer" {assert_eq!(packet["frequency"],1000);assert_eq!(packet["duration_ms"],70);}
            if path=="/api/outputs" {assert_eq!(packet["mask"],5);}
            let mut receipt=serde_json::json!({"type":"command-result","request_id":request.id,"accepted":true,
                "board_id":current.board_id,"boot_id":current.boot_id,"status":current});
            assert!(request.verify(receipt.clone()).is_err());receipt["avr_acknowledged"]=true.into();
            if path=="/api/outputs" {
                assert!(request.verify(receipt.clone()).is_err());
                receipt["acknowledged_mask"]=5.into();
                receipt["status"]["active_outputs"]=0.into(); // A later snapshot updates the display.
            }
            assert!(request.verify(receipt).is_ok());
            let mut bad=form;bad[0].1="invalid".into();
            assert!(Intent::new(endpoint(),&current,path,bad,queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).is_err());
            let mut old=current.clone();old.capabilities.as_mut().unwrap().acknowledged_features.retain(|feature|feature!="avr-command-ack");
            assert!(!supported(&old,path));
        }
        for (frequency,duration) in [(19,70),(12001,70),(1000,0),(1000,5001)] {
            assert!(Intent::new(endpoint(),&current,"/api/buzzer",vec![("frequency".into(),frequency.to_string()),
                ("duration_ms".into(),duration.to_string())],queue.capture(),gate.stamp(crate::Transport::WiFi,epoch),None).is_err());
        }
    }
    #[test]
    fn realtime_pixel_diagnostic_requires_exact_complete_readback_and_typed_intent() {
        let (mut request, _, _) = intent(endpoint());
        let probe = crate::pixel_diagnostic::Probe {
            index: 474,
            group: true,
            rgb: 0x1234ab,
            ..Default::default()
        };
        request.path = "/api/led/diagnostic";
        request.form = probe.form(false);
        let mut observed = status();
        observed.pixels = 480;
        observed.mode = "cell".into();
        observed.effect = "none".into();
        observed
            .capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .push("pixel-diagnostic".into());
        observed.led_output = serde_json::json!({"enabled":true,"mode":"effects"});
        observed.pixel_frame=Some(serde_json::from_value(serde_json::json!({"revision":1,"pending":false,"brightness":255,"physical":(1..=80).collect::<Vec<_>>(),
            "rgb":probe.frame(false).unwrap().iter().map(|v|format!("{v:02x}")).collect::<String>()})).unwrap());
        let good = serde_json::json!({"type":"command-result","request_id":request.id,"board_id":observed.board_id,
            "boot_id":observed.boot_id,"accepted":true,"status":observed});
        assert!(request.verify(good.clone()).is_ok());
        let mut pending = good.clone();
        pending["status"]["pixel_frame"]["pending"] = true.into();
        assert!(request.verify(pending).is_err());
        let mut bad = good.clone();
        bad["status"]["pixel_frame"]["rgb"] = serde_json::json!("00".repeat(1440));
        assert!(request.verify(bad).is_err());
        let mut absent = good;
        absent["status"]["pixel_frame"] = serde_json::Value::Null;
        assert!(request.verify(absent).is_err());
    }
    #[test]
    fn realtime_ack_requires_id_boot_count_and_every_selected_value() {
        let (request, _, _) = intent(endpoint());
        let good = receipt(&request);
        assert_eq!(request.verify(good.clone()).unwrap().status.cells[79], "red");
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
        assert_eq!(request.http().unwrap().status.cells[79], "red");
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
                if serde_json::from_str::<serde_json::Value>(text)
                    .ok()
                    .is_some_and(|v| v["type"] == "peer-introduction")
                {
                    continue; // Admission identity is not an actuation or a status poll.
                }
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
