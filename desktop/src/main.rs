use std::{
    io::{ErrorKind, Read, Write},
    sync::mpsc::{self, Receiver, Sender, TryRecvError},
    thread,
    time::{Duration, Instant},
};

mod access;
mod audio;
mod brand_image;
mod board_silent;
mod branding;
mod bridge;
mod capabilities;
mod client_telemetry;
mod client_management;
mod client_install;
mod client_secrets;
mod status_rgb;
mod cli_output;
mod config;
mod native_workspace;
mod workspace_layout;
mod configuration_reads;
mod connection;
mod device_settings;
mod diagnostics;
mod display;
mod identity;
mod maintenance;
mod firmware_progress;
mod firmware_source;
mod firmware_feed;
mod messaging;
mod pc_contract;
mod peer_http;
mod realtime;
mod pixel_diagnostic;
mod platform;
mod streaming;
mod terminal_ui;
mod usb_table;
mod cell_batch;
mod ui;

use base64::{Engine, engine::general_purpose::STANDARD as BASE64};
use eframe::egui;
use reqwest::blocking::Client;
use serde::Deserialize;
use connection::Method as Transport;
use tungstenite::{Message, client::IntoClientRequest};

const OP_GET_STATUS: u8 = 0x02;
const OP_BUZZER: u8 = 0x10;
const OP_AUDIO: u8 = 0x11;
const OP_DISPLAY: u8 = 0x38;
const OP_SET_MODE: u8 = 0x4b;
const OP_CELL_ACTION: u8 = 0x4c;
const OP_RESET: u8 = 0x4d;
const OP_OUTPUT_MASK: u8 = 0x4e;
const OP_BRIGHTNESS: u8 = 0x51;
const OP_PATTERN: u8 = 0x52;
const OP_LED_OUTPUT: u8 = 0x5c;
const OP_NETWORK_CONFIG: u8 = 0x53;
const OP_STATUS: u8 = 0x90;

#[derive(Clone, PartialEq)]
struct Endpoint {
    base: String,
    user: String,
    password: String,
}

#[derive(Debug, Clone, Default, Deserialize, serde::Serialize)]
struct DeviceStatus {
    #[serde(default)]
    bridge: Option<serde_json::Value>,
    #[serde(default,deserialize_with="ui::status_bar::optional_status_rgb")]
    status_rgb: Option<ui::status_bar::PhysicalStatusRgb>,
    #[serde(default)]
    board_id: String,
    #[serde(default)]
    boot_id: String,
    #[serde(default)]
    http_peer_fencing: bool,
    #[serde(default)]
    command_clock_ms: Option<u64>,
    #[serde(default)]
    capabilities: Option<capabilities::Manifest>,
    #[serde(default)]
    version: String,
    #[serde(default)]
    avr_version: Option<String>,
    #[serde(default)]
    audio_cues: Vec<String>,
    #[serde(default)]
    hostname: String,
    #[serde(default)]
    ip: String,
    #[serde(default)]
    uplink: String,
    #[serde(default)]
    uplink_ipv4_ready: Option<bool>,
    #[serde(default)]
    ethernet: serde_json::Value,
    #[serde(default)]
    mode: String,
    #[serde(default)]
    avr_online: bool,
    #[serde(default)]
    ap_started: bool,
    #[serde(default)]
    ap_enabled: bool,
    #[serde(default)]
    ap_ip: String,
    #[serde(default)]
    station_enabled: bool,
    #[serde(default)]
    station_connected: bool,
    #[serde(default)]
    station_ssid: String,
    #[serde(default)]
    station_ip: String,
    #[serde(default)]
    station_rssi: i16,
    #[serde(default)]
    mdns_started: bool,
    #[serde(default)]
    ssdp_started: bool,
    #[serde(default)]
    netbios_started: bool,
    #[serde(default)]
    canonical_url: String,
    #[serde(default)]
    tm1637_ack: bool,
    #[serde(default, deserialize_with = "display::optional_lcd")]
    lcd: Option<display::LcdObservation>,
    #[serde(default)]
    tm1637_segments: Vec<u8>,
    #[serde(default)]
    tm1637_brightness: u8,
    #[serde(default)]
    tm1637_host_active: bool,
    #[serde(default)]
    programming_latch: bool,
    #[serde(default)]
    winner: u8,
    #[serde(default)]
    selected_cell: u8,
    #[serde(default)]
    brightness: u8,
    #[serde(default)]
    led_output: serde_json::Value,
    #[serde(default)]
    update: serde_json::Value,
    #[serde(default)]
    ota_capacity_bytes: serde_json::Value,
    #[serde(default,deserialize_with="firmware_progress::optional_upgrade")]
    upgrade:Option<firmware_progress::Upgrade>,
    #[serde(default)]
    remote_firmware: serde_json::Value,
    #[serde(default)]
    avr_maintenance: serde_json::Value,
    #[serde(default)]
    effect: String,
    #[serde(default)]
    pixels: u16,
    #[serde(default)]
    active_inputs: u8,
    #[serde(default)]
    input_9: bool,
    #[serde(default)]
    active_outputs: u8,
    #[serde(default)]
    esp_uart_errors: u16,
    #[serde(default)]
    avr_uart_errors: u16,
    #[serde(default)]
    usb_serial_errors: u16,
    #[serde(default)]
    ntp_synced: bool,
    #[serde(default)]
    utc_epoch: u64,
    #[serde(default)]
    websocket_clients: u8,
    #[serde(default)]
    mqtt_enabled: bool,
    #[serde(default)]
    mqtt_connected: bool,
    #[serde(default)]
    mqtt_state: i16,
    #[serde(default)]
    cloud_websocket_enabled: bool,
    #[serde(default)]
    cloud_websocket_connected: bool,
    #[serde(default)]
    cloud_error: String,
    #[serde(default)]
    link_mode: String,
    #[serde(default)]
    cells: Vec<String>,
    #[serde(default)]
    pixel_preview_requested: bool,
    #[serde(default)]
    pixel_frame: Option<ui::cell_pixels::Frame>,
}

fn led_output_mode(status: &DeviceStatus) -> Option<&str> {
    status.led_output.get("mode").and_then(|v| v.as_str())
        .filter(|mode| matches!(*mode, "static" | "effects"))
}

#[derive(Clone)]
enum HttpCommand {
    Scoped(peer_http::Scope, Box<HttpCommand>),
    Status(Endpoint),
    Mapping(Endpoint, u64),
    Configuration(Endpoint, &'static str),
    BoardSilent(Endpoint, u64, board_silent::Change),
    Post(Endpoint, &'static str, Vec<(String, String)>),
    CellBatch(Endpoint,cell_batch::Correlation),
    AccessChange(Endpoint, Endpoint, bool),
    NameChange(Endpoint, String, Option<String>),
    Settings(Endpoint, device_settings::Write),
    Maintenance(Endpoint, u64, maintenance::Operation),
    PixelReadback(Endpoint, u64, u16, peer_http::Pin),
}

impl HttpCommand {
    // Adapted from PCController Session.requestAdmission.grantNextLocked:
    // priority only reorders unsent work, never preempts/replays a wire request.
    fn queue_priority(&self) -> u8 {
        match self {
            Self::Scoped(_, inner) => inner.queue_priority(),
            Self::Configuration(..) => 0,
            Self::Status(_) | Self::Mapping(..) => 1,
            _ => 2,
        }
    }
    fn is_status(&self) -> bool { match self {Self::Scoped(_, command) => command.is_status(),Self::Status(_) => true,_ => false} }
    fn discoverable_read(&self) -> bool { match self {Self::Scoped(_, command) => command.discoverable_read(),Self::Status(_) | Self::Mapping(..) | Self::Configuration(..) => true,_ => false} }
    fn failure(&self, error: String) -> Event {
        match self {
            Self::Scoped(scope, command) => Event::ForScope(scope.clone(), Box::new(command.failure(error))),
            Self::BoardSilent(_, id, _) => Event::BoardSilentFinished(*id, Err(error)),
            Self::Settings(_, write) => Event::SettingsFinished(write.id, write.group, Err(error)),
            Self::Maintenance(_, id, _) => Event::MaintenanceFinished(*id, Err(error)),
            Self::PixelReadback(_, id, ..) => Event::PixelReadbackFinished(*id, Err(error)),
            Self::Mapping(_, id) => Event::MappingFailed(*id, error),
            _ if self.diagnostic_source().is_some() => Event::DiagnosticFailure(self.diagnostic_source().unwrap(), error),
            _ => Event::OperationFailed(error),
        }
    }
    fn diagnostic_source(&self) -> Option<String> {
        match self {
            Self::Scoped(_, command) => command.diagnostic_source(),
            Self::Status(_) => Some("HTTP GET /api/status".into()),
            Self::Mapping(..) => Some("HTTP GET /api/cells/mapping".into()),
            Self::Configuration(_, path) => Some(format!("HTTP GET {path}")),
            _ => None, // Explicit operations keep their own correlated outcomes.
        }
    }
    fn endpoint(&self) -> &Endpoint {
        match self {
            Self::Scoped(_, command) => command.endpoint(),
            Self::Status(endpoint)
            | Self::Mapping(endpoint, ..)
            | Self::BoardSilent(endpoint, ..)
            | Self::Configuration(endpoint, ..)
            | Self::AccessChange(endpoint, ..)
            | Self::NameChange(endpoint, ..)
            | Self::Settings(endpoint, ..)
            | Self::Maintenance(endpoint, ..)
            | Self::PixelReadback(endpoint, ..)
            | Self::Post(endpoint, ..) => endpoint,
            Self::CellBatch(endpoint, ..) => endpoint,
        }
    }

    fn with_base(&self, base: String) -> Self {
        let replace = |endpoint: &Endpoint| Endpoint {
            base: base.clone(),
            user: endpoint.user.clone(),
            password: endpoint.password.clone(),
        };
        match self {
            Self::Scoped(scope, command) => Self::Scoped(scope.clone(), Box::new(command.with_base(base.clone()))),
            Self::Status(endpoint) => Self::Status(replace(endpoint)),
            Self::Mapping(endpoint, id) => Self::Mapping(replace(endpoint), *id),
            Self::Configuration(endpoint, path) => Self::Configuration(replace(endpoint), path),
            Self::BoardSilent(endpoint, id, desired) => Self::BoardSilent(replace(endpoint), *id, *desired),
            Self::Post(endpoint, path, form) => Self::Post(replace(endpoint), path, form.clone()),
            Self::CellBatch(endpoint,correlation)=>Self::CellBatch(replace(endpoint),correlation.clone()),
            Self::AccessChange(endpoint, next, enabled) => {
                Self::AccessChange(replace(endpoint), replace(next), *enabled)
            }
            Self::NameChange(endpoint, name, label) => Self::NameChange(replace(endpoint), name.clone(), label.clone()),
            Self::Settings(endpoint, write) => Self::Settings(replace(endpoint), write.clone()),
            Self::Maintenance(endpoint, id, operation) => {
                Self::Maintenance(replace(endpoint), *id, operation.clone())
            }
            Self::PixelReadback(endpoint, id, index, peer) => Self::PixelReadback(replace(endpoint), *id, *index, peer.clone()),
        }
    }
}

#[derive(Debug, Clone, Default, Deserialize)]
struct CellMapping {
    logical_id: u8,
    physical_cell: u8,
    name: String,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct CellMappingResponse {
    #[serde(default)]
    board_id: Option<String>,
    #[serde(default)]
    boot_id: Option<String>,
    #[serde(default)]
    physical: Vec<u8>,
    #[serde(default)]
    names: Vec<String>,
}
impl CellMappingResponse {
    fn decode(self) -> Result<Vec<CellMapping>, String> {
        let mappings: Vec<_> = self.physical.into_iter().enumerate().map(|(index, physical_cell)| CellMapping {
            logical_id: u8::try_from(index + 1).unwrap_or(0), physical_cell,
            name: self.names.get(index).cloned().unwrap_or_else(|| (index + 1).to_string()),
        }).collect();
        if !streaming::valid_mapping(&mappings) {
            return Err("Board mapping is incomplete or invalid; previous same-peer mapping retained, nothing written".into());
        }
        Ok(mappings)
    }
}
struct MappingPending {
    id: u64,
    peer: peer_http::Pin,
    started: Instant,
}
fn status_observation_fresh(last: Option<Instant>) -> bool {
    last.is_some_and(|at| at.elapsed() < Duration::from_secs(8))
}
fn websocket_status_ready(status: &DeviceStatus) -> bool {
    !status.hostname.is_empty() && status.capabilities.as_ref().is_some_and(|manifest| manifest.known())
}
fn websocket_status(text: &str) -> Option<DeviceStatus> {
    let status = serde_json::from_str::<DeviceStatus>(text).ok()?;
    websocket_status_ready(&status).then_some(status)
}
enum WsCommand {
    Control(realtime::Intent),
    Connect(Endpoint, u64),
    Disconnect,
    Rate(u16),
    Mapping(Vec<CellMapping>),
    Stream(bool),
    Sample(bool),
    Pixel(pixel_diagnostic::Request),
    ArmPixel(pixel_diagnostic::Request),
}
enum SerialCommand {
    Bridge(String, usb_table::Admission, u8, Vec<u8>, bridge::Reply),
    Port(String, u64),
    Send(String, usb_table::Admission, u8, Vec<u8>),
    Identity(String, usb_table::Admission, String, String),
    BoardSilent(String, usb_table::Admission, u64, board_silent::Change),
}
enum Event {
    ForConnection(connection::Stamp, Box<Event>),
    ForScope(peer_http::Scope, Box<Event>),
    ForEndpoint(Endpoint, Box<Event>),
    StreamStats(f64),
    Status(DeviceStatus),
    UpgradeProgress(firmware_progress::Upgrade),
    StatusRgb(status_rgb::Observation),
    Mapping(u64, Vec<CellMapping>, Option<(String, String)>),
    MappingFailed(u64, String),
    Configuration(&'static str, serde_json::Value),
    Accepted(String),
    BoardSilentFinished(u64, Result<pc_contract::Settings, String>),
    CredentialsUpdated(Endpoint),
    NameUpdated(Endpoint, bool),
    SerialIdentity(String, serde_json::Value),
    SerialIdentitySaved(String, Result<(), String>),
    SerialSnapshot(String, usb_table::Snapshot),
    SerialOperation(String, Result<String, String>),
    SerialTiming(String, usb_table::Timing),
    SerialCapture(String, usb_table::CaptureProgress),
    SerialCommandState(String, DeviceStatus),
    CellBatchConfirmed(cell_batch::Correlation),
    SerialBatchConfirmed(String,cell_batch::Correlation),
    SerialQueueDepth(String, usize),
    SerialOwner(String, u64),
    DiagnosticFailure(String, String),
    DiagnosticRecovered(String),
    SerialFailure(String, String),
    OperationFailed(String),
    SettingsFinished(
        u64,
        device_settings::Group,
        Result<serde_json::Value, String>,
    ),
    MaintenanceFinished(u64, Result<String, String>),
    PixelModeFinished(u64, Result<DeviceStatus, String>),
    PixelSent(pixel_diagnostic::Request, Result<(), String>),
    PixelReadbackFinished(u64, Result<String, String>),
    WebSocket(bool),
    Serial(String, bool, Option<DeviceStatus>),
}

impl Event {
    fn http_status(&self) -> Option<&DeviceStatus> { match self { Self::ForScope(_, inner) => inner.http_status(),Self::Status(status) => Some(status),_ => None } }
    fn http_succeeded(&self) -> bool { match self { Self::ForScope(_, inner) => inner.http_succeeded(),Self::MappingFailed(..) | Self::DiagnosticFailure(..) | Self::OperationFailed(_) | Self::BoardSilentFinished(_, Err(_)) | Self::SettingsFinished(_, _, Err(_)) | Self::MaintenanceFinished(_, Err(_)) | Self::PixelModeFinished(_, Err(_)) | Self::PixelReadbackFinished(_, Err(_)) => false,_ => true } }
}

fn for_endpoint(endpoint: &Endpoint, event: Event) -> Event {
    Event::ForEndpoint(endpoint.clone(), Box::new(event))
}

struct SessionEvents { sender: Sender<Event>, stamp: connection::Stamp }
impl SessionEvents {
    fn send(&self, event: Event) -> Result<(), mpsc::SendError<Event>> {
        self.sender.send(Event::ForConnection(self.stamp.clone(), Box::new(event)))
    }
}

fn current_event(event: Event, endpoint: &Endpoint, serial_port: &str) -> Option<Event> {
    match event {
        Event::ForEndpoint(origin, inner) if origin == *endpoint => Some(*inner),
        Event::ForEndpoint(..) => None,
        Event::Serial(ref port, ..) if port != serial_port => None,
        Event::SerialFailure(ref port, ..) if port != serial_port => None,
        Event::SerialSnapshot(ref port, ..) | Event::SerialOperation(ref port, ..) | Event::SerialTiming(ref port, ..) | Event::SerialCapture(ref port, ..) | Event::SerialCommandState(ref port, ..) | Event::SerialBatchConfirmed(ref port, ..) | Event::SerialQueueDepth(ref port, ..) | Event::SerialOwner(ref port, ..) if port != serial_port => None,
        other => Some(other),
    }
}

fn http_request(client: &Client, command: HttpCommand) -> Result<Event, String> {
    if let HttpCommand::Scoped(scope, command) = command {
        return scoped_http_request(client, &scope, &command).map(|result| Event::ForScope(scope, Box::new(result)));
    }
    let mapping_id = match &command { HttpCommand::Mapping(_, id) => *id, _ => 0 };
    if let HttpCommand::PixelReadback(endpoint, id, index, peer) = &command {
        return Ok(Event::PixelReadbackFinished(*id, pixel_diagnostic::verify_frame(client, endpoint, peer, *index)));
    }
    if let HttpCommand::Maintenance(endpoint, id, operation) = &command {
        return Ok(Event::MaintenanceFinished(
            *id,
            maintenance::run(client, endpoint, operation.clone()),
        ));
    }
    if let HttpCommand::Settings(endpoint, write) = &command {
        // Exactly one mutation. Neither discovery nor readback failure replays it.
        let result = (|| -> Result<serde_json::Value, String> {
            let response = client.post(format!("{}{}", endpoint.base.trim_end_matches('/'), write.group.path()))
                .basic_auth(&endpoint.user, Some(&endpoint.password)).form(&write.form).send()
                .map_err(|_| "Configuration write outcome is unknown. Refresh before retrying; no write was replayed.".to_string())?
                .error_for_status().map_err(|e| format!("Configuration rejected: {e}"))?;
            let accepted = response.json::<serde_json::Value>().map_err(|_| {
                "Write response could not be validated; refresh before retrying".to_string()
            })?;
            write.verify_ack(&accepted)?;
            let readback = client.get(format!("{}{}", endpoint.base.trim_end_matches('/'), write.group.path()))
                .basic_auth(&endpoint.user, Some(&endpoint.password)).send()
                .map_err(|_| "Write accepted; readback unavailable, possibly after Wi-Fi reassociation. Reconnect and refresh; no write was replayed.".to_string())?
                .error_for_status().map_err(|e| format!("Write accepted; readback failed: {e}"))?
                .json::<serde_json::Value>().map_err(|_| "Write accepted; malformed readback. Refresh before retrying.".to_string())?;
            write.verify_readback(&readback)?;
            Ok(readback)
        })();
        return Ok(Event::SettingsFinished(write.id, write.group, result));
    }
    if let HttpCommand::NameChange(endpoint, input, label) = &command {
        let name = identity::normalize_name(input)?;
        if label.as_ref().is_some_and(|s| !identity::valid_display_name(s)) { return Err("Invalid board display name".into()); }
        let mut form = vec![("hostname", name.as_str())];
        if let Some(label) = label { form.push(("display_name", label.as_str())); }
        let response = client
            .post(format!(
                "{}/api/device/name",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .form(&form)
            .send()
            .map_err(diagnostics::describe_error)?;
        if !response.status().is_success() {
            return Err(format!(
                "HTTP {}: {}",
                response.status(),
                response.text().unwrap_or_default()
            ));
        }
        let value: serde_json::Value = response.json().map_err(diagnostics::describe_error)?;
        if value.get("accepted").and_then(|v| v.as_bool()) != Some(true)
            || value.get("persisted").and_then(|v| v.as_bool()) != Some(true)
            || value.get("hostname").and_then(|v| v.as_str()) != Some(name.as_str())
            || label.as_ref().is_some_and(|s| value["display_name"].as_str() != Some(s.as_str()))
        {
            return Err(
                "Device did not confirm the saved name; no retry or endpoint switch was attempted"
                    .into(),
            );
        }
        let mut next = endpoint.clone();
        next.base = format!("http://{name}.local");
        let pending = value["apply_pending"].as_bool().unwrap_or(true);
        if !pending {
            let readback: serde_json::Value = client.get(format!("{}/api/device/name",endpoint.base.trim_end_matches('/')))
                .basic_auth(&endpoint.user, Some(&endpoint.password)).send().map_err(|_| "Identity saved but readback unavailable; refresh before retrying")?
                .error_for_status().map_err(|e|e.to_string())?.json().map_err(|e|e.to_string())?;
            if readback["hostname"] != value["hostname"] || readback["display_name"] != value["display_name"] { return Err("Identity saved but readback mismatch; no retry attempted".into()); }
        }
        return Ok(Event::NameUpdated(next, pending));
    }
    if let HttpCommand::AccessChange(endpoint, next, enabled) = &command {
        let response = client
            .post(format!(
                "{}/api/auth",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .form(&[
                ("enabled", &enabled.to_string()),
                ("current_password", &endpoint.password),
                ("username", &next.user),
                ("password", &next.password),
            ])
            .send()
            .map_err(diagnostics::describe_error)?;
        if !response.status().is_success() {
            return Err(format!(
                "HTTP {}: {}",
                response.status(),
                response.text().unwrap_or_default()
            ));
        }
        let value: serde_json::Value = response.json().map_err(diagnostics::describe_error)?;
        if value.get("updated").and_then(|v| v.as_bool()) != Some(true) {
            return Err("Device did not confirm a saved credential change".into());
        }
        if value.get("enabled").and_then(|v|v.as_bool()) != Some(*enabled) { return Err("Device did not confirm the authentication setting".into()); }
        let mut active = next.clone();
        if !enabled { active.user.clear(); active.password.clear(); }
        return Ok(Event::CredentialsUpdated(active));
    }
    if let HttpCommand::Configuration(endpoint, path) = &command {
        let response = client
            .get(format!("{}{}", endpoint.base.trim_end_matches('/'), path))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .send()
            .map_err(diagnostics::describe_error)?
            .error_for_status()
            .map_err(diagnostics::describe_error)?;
        return response
            .json::<serde_json::Value>()
            .map(|value| Event::Configuration(path, value))
            .map_err(diagnostics::describe_error);
    }
    let (endpoint, request, response_kind) = match command {
        HttpCommand::Scoped(..) => unreachable!(),
        HttpCommand::Status(endpoint) => {
            let url = format!("{}/api/status", endpoint.base.trim_end_matches('/'));
            let request = client.get(url).query(&[(
                "pixels",
                if config::live().cell_shape != ui::cell_pixels::Shape::Rounded {
                    "1"
                } else {
                    "0"
                },
            )]);
            (endpoint, request, 1_u8)
        }
        HttpCommand::Mapping(endpoint, _) => {
            let url = format!(
                "{}/api/cells/mapping",
                endpoint.base.trim_end_matches('/')
            );
            let request = client.get(url);
            (endpoint, request, 2_u8)
        }
        HttpCommand::Configuration(..)
        | HttpCommand::AccessChange(..)
        | HttpCommand::BoardSilent(..)
        | HttpCommand::NameChange(..) => unreachable!(),
        HttpCommand::Settings(..) => unreachable!(),
        HttpCommand::Maintenance(..) => unreachable!(),
        HttpCommand::PixelReadback(..) => unreachable!(),
        HttpCommand::CellBatch(..) => unreachable!("Cell batch requires scoped admission"),
        HttpCommand::Post(endpoint, path, form) => {
            let url = format!("{}{}", endpoint.base.trim_end_matches('/'), path);
            let request = client.post(url).form(&form);
            (endpoint, request, 0_u8)
        }
    };
    let response = request
        .basic_auth(endpoint.user, Some(endpoint.password))
        .send()
        .map_err(|error|{
            if error.is_connect(){peer_http::request_relookup(&endpoint.base);}
            diagnostics::describe_error(error)
        })?;
    let code = response.status();
    let remote = response.remote_addr();
    if !code.is_success() {
        return Err(format!(
            "HTTP {code}: {}",
            response.text().unwrap_or_default()
        ));
    }
    match response_kind {
        1 => {
            let status=response.json::<DeviceStatus>().map_err(diagnostics::describe_error)?;
            peer_http::remember_address(&endpoint.base,&status,remote);
            Ok(Event::Status(status))
        },
        2 => response
            .json::<CellMappingResponse>()
            .map_err(diagnostics::describe_error)
            .and_then(|value| {
                let identity = match (&value.board_id, &value.boot_id) {
                    (None, None) => None,
                    (Some(board), Some(boot)) => Some((board.clone(), boot.clone())),
                    _ => return Err("Mapping response has incomplete board/boot identity; not applied".into()),
                };
                value.decode().map(|cells| Event::Mapping(mapping_id, cells, identity))
            }),
        _ => Ok(Event::Accepted(format!(
            "Device accepted the command ({code})"
        ))),
    }
}

fn scoped_http_request(client: &Client, scope: &peer_http::Scope, command: &HttpCommand) -> Result<Event, String> {
    // Adapt donor acquireWrite's checks before AND after waiting/admission.
    let mutating = !matches!(command, HttpCommand::Status(_) | HttpCommand::Mapping(..) | HttpCommand::Configuration(..)
        | HttpCommand::Maintenance(_, _, maintenance::Operation::RemoteStatus | maintenance::Operation::Download {..})
        | HttpCommand::PixelReadback(..));
    let admit = || if mutating { scope.admit() } else { scope.admit_read() };
    admit()?;
    if matches!(command, HttpCommand::Status(_)) { return http_request(client, command.clone()); }
    if let HttpCommand::PixelReadback(_, _, _, peer) = command {
        if scope.pin.as_ref() != Some(peer) { return Err("Pixel readback peer was revoked".into()); }
        let result = http_request(&peer.client()?, command.clone())?;
        if !scope.current() { return Err("Pixel readback belongs to a retired board; not applied".into()); }
        return Ok(result);
    }
    if matches!(command, HttpCommand::Mapping(..)) && let Some(peer) = &scope.pin {
        // The mapping response identifies its own producer. A fresh selected
        // WS peer must not depend on an unrelated HTTP status preflight before
        // this small read is even sent. Headers are routing fences, not proof:
        // validate the actual response identity and the still-current lease.
        let result = http_request(&peer.client()?, command.clone())?;
        scope.admit_read()?;
        if let Event::Mapping(_, _, Some((board, boot))) = &result {
            if !peer.matches_identity(board, boot) {
                return Err("Mapping belongs to a different board/boot; not applied".into());
            }
            return Ok(result);
        }
        // Older firmware has no producer envelope. Discard this unbound read
        // and perform the original status -> mapping -> status fences below;
        // a status observed only after a mapping cannot establish its owner.
    }
    if mutating && scope.pin.is_none() { return Err("Read-only: peer has not advertised valid board/boot identity and HTTP fencing".into()); }
    let before = blocking_status(client, command.endpoint())?;
    let observed = peer_http::Pin::from_status(&before);
    if scope.pin != observed { return Err("Board/boot identity changed before dispatch; nothing was sent".into()); }
    admit()?;
    if mutating {
        let allowed = before.capabilities.as_ref().is_some_and(|c| match command {
            HttpCommand::Post(_, path, form) => c.allows(path, form),
            HttpCommand::CellBatch(_,correlation)=>c.allows("/api/cells",&correlation.request.form()),
            HttpCommand::Settings(_, write) => c.allows(write.group.path(), &write.form),
            HttpCommand::BoardSilent(..) => board_silent::settings_supported(&before),
            HttpCommand::NameChange(..) => c.has("identity"),
            HttpCommand::AccessChange(..) => c.has("network"),
            HttpCommand::Maintenance(_, _, operation) => c.has(match operation {
                maintenance::Operation::ReconnectCloud => "network",
                maintenance::Operation::Probe | maintenance::Operation::Backup | maintenance::Operation::StageAvr(_)
                | maintenance::Operation::ProgramAvr | maintenance::Operation::SourceImage {esp:false,..} => "isp",
                _ => "ota",
            }),
            _ => false,
        });
        if !allowed { return Err("This board no longer acknowledges the requested capability; nothing was sent".into()); }
    }
    let pinned = scope.pin.as_ref().map(peer_http::Pin::client).transpose()?;
    if let HttpCommand::BoardSilent(endpoint, id, desired) = command {
        if !desired.allowed(&before) {
            return Err("Board settings are unavailable or programming safety is active".into());
        }
        let client = pinned.as_ref().ok_or("Board settings require a verified peer")?;
        let mut transaction = board_silent::Transaction::new(*desired);
        loop {
            scope.admit()?;
            let (opcode, payload) = transaction.request();
            let receipt = pc_contract::request(client, &endpoint.base, &endpoint.user, &endpoint.password, opcode, &payload)?;
            let response = receipt["response_opcode"].as_u64().ok_or("Missing board response opcode")?;
            let payload = pc_contract::decode_hex(receipt["payload_hex"].as_str().ok_or("Missing board response")?)?;
            scope.admit()?;
            if let Some(settings) = transaction.accept(u8::try_from(response).map_err(|_|"Invalid response opcode")?, &payload)? {
                let after = blocking_status(client, endpoint)?;
                if scope.pin != peer_http::Pin::from_status(&after) || !scope.current() {
                    return Err("Board/session changed during settings readback; result not applied".into());
                }
                return Ok(Event::BoardSilentFinished(*id, Ok(settings)));
            }
        }
    }
    if let HttpCommand::CellBatch(endpoint,correlation)=command {
        if scope.pin.as_ref()!=Some(&correlation.peer) || correlation.route!=Transport::WiFi {
            return Err("Atomic batch admission does not belong to this peer/route".into());
        }
        cell_batch::send_verified(&before,endpoint,&correlation.request,Some(&correlation.peer),None,Some(&||scope.admit()))?;
        if !scope.current(){return Err("Batch belongs to a retired board; inspect original device, no replay".into());}
        return Ok(Event::CellBatchConfirmed(correlation.clone()));
    }
    if let HttpCommand::Post(endpoint,"/api/cells",form)=command {
        let get=|name:&str|form.iter().find(|(key,_)|key==name).map(|(_,value)|value.as_str()).ok_or_else(||format!("Missing batch {name}"));
        let request=cell_batch::Request::new(&ui::cell_search::parse(get("ids")?)?,get("state")?,get("effect")?)?;
        let receipt=cell_batch::send_verified(&before,endpoint,&request,scope.pin.as_ref(),None,Some(&||scope.admit()))?;
        if !scope.current(){return Err("Batch belongs to a retired board; inspect original device, no replay".into());}
        return Ok(Event::Accepted(receipt)); // No UI intent correlation: never complete another batch.
    }
    let result = http_request(pinned.as_ref().unwrap_or(client), command.clone())?;
    // Credential/hostname/self-update results may intentionally retire the endpoint.
    // Settings already performed readback; validate its observed epoch separately.
    // New mapping responses identify their own producer. Validate that exact
    // response rather than discarding valid mapping when an unrelated trailing
    // status request fails. Older firmware retains both status fences.
    let mapping_identified = if let Event::Mapping(_, _, Some((board, boot))) = &result {
        if !scope.pin.as_ref().is_some_and(|pin| pin.matches_identity(board, boot)) {
            return Err("Mapping belongs to a different board/boot; not applied".into());
        }
        true
    } else { false };
    let verify_read = (matches!(command, HttpCommand::Mapping(..)) && !mapping_identified)
        || matches!(command, HttpCommand::Configuration(..))
        || matches!(&result, Event::SettingsFinished(_, _, Ok(_)) | Event::NameUpdated(_, false));
    if verify_read {
        let after = blocking_status(client, command.endpoint()).map_err(|_| "Result received but board/boot readback unavailable; inspect before retrying".to_string())?;
        if scope.pin != peer_http::Pin::from_status(&after) { return Err("Board/boot changed during readback; result not applied to this peer".into()); }
    }
    if !scope.current() { return Err("Board session revoked after dispatch; outcome unknown, no replay".into()); }
    Ok(result)
}

fn http_worker(commands: Receiver<HttpCommand>, events: Sender<Event>, context: egui::Context, gate: connection::Gate) {
    let mut reporter = client_telemetry::Reporter::new();
    let mut discovered_base: Option<String> = None;
    let mut requested_base = String::new();
    let mut auth_probe: Option<(u64, String)> = None;
    let mut waiting = std::collections::VecDeque::new();
    loop {
        if waiting.is_empty() {
            match commands.recv() { Ok(command) => waiting.push_back(command), Err(_) => break }
        }
        while waiting.len() < 64 {
            match commands.try_recv() { Ok(command) => waiting.push_back(command), Err(_) => break }
        }
        // Strictly greater retains FIFO within a priority, like the donor.
        let mut best = 0;
        for index in 1..waiting.len() {
            if waiting[index].queue_priority() > waiting[best].queue_priority() { best = index; }
        }
        let command = waiting.remove(best).expect("nonempty HTTP queue");
        let stamp = gate.stamp(Transport::WiFi, gate.epoch());
        let lease = loop {
            if !stamp.current() || matches!(&command, HttpCommand::Scoped(scope, _) if !scope.current()) { break None; }
            if let Some(lease) = gate.acquire(Transport::WiFi, stamp.epoch) { break Some(lease); }
            thread::sleep(Duration::from_millis(20));
        };
        let Some(lease) = lease else { continue; };
        // A verified TCP address is a routing hint, never ownership. Rebuild
        // this small client from the latest cache rather than repeating a slow
        // Windows .local lookup for each pinned/status/configuration request.
        // No idle socket survives release to the selected USB-only method.
        let client=peer_http::device_client_builder().build().expect("HTTP client");
        let events = SessionEvents { sender: events.clone(), stamp: lease.stamp() };
        if requested_base != command.endpoint().base {
            discovered_base = None;
            requested_base = command.endpoint().base.clone();
        }
        let requested_name = reqwest::Url::parse(&command.endpoint().base)
            .ok()
            .and_then(|url| {
                url.host_str()
                    .and_then(|host| identity::normalize_name(host).ok())
            });
        let first = discovered_base
            .as_ref()
            .map(|base| command.with_base(base.clone()))
            .unwrap_or_else(|| command.clone());
        let mut result = http_request(&client, first);
        if result.is_err()
            && lease.current()
            && requested_name.is_some()
            && command.discoverable_read()
        {
            discovered_base = None;
            for base in platform::discovery::ssdp_locations(Duration::from_millis(1200)) {
                if !lease.current() { break; }
                let mut candidate = command.endpoint().clone();
                candidate.base = base.clone();
                // A rename must not make fallback select some other controller
                // merely because it shares credentials. Read/verify its label first.
                let Ok(status) = blocking_status(&client, &candidate) else {
                    continue;
                };
                if !requested_name.as_ref().is_some_and(|name| bridge::matches_hostname(&status, name)) {
                    continue;
                }
                if let Ok(event) = http_request(&client, command.with_base(base.clone())) {
                    discovered_base = Some(base);
                    result = Ok(event);
                    break;
                }
            }
        }
        if lease.current() && command.is_status() && reporter.due() && let Some(status) = result.as_ref().ok().and_then(Event::http_status)
            && status.capabilities.as_ref().is_some_and(|c| c.has("telemetry"))
            && let Ok(pinned) = peer_http::pinned_client(status)
            && match &command {HttpCommand::Scoped(scope, _) => scope.current(),_ => false} {
            let mut endpoint = command.endpoint().clone();
            if let Some(base) = &discovered_base {
                endpoint.base = base.clone();
            }
            reporter.report(&pinned, &endpoint);
        }
        if command.is_status() && lease.current() {
            let key = (stamp.epoch, command.endpoint().base.clone());
            let unauthorized = result.as_ref().err().is_some_and(|error| error.starts_with("HTTP 401"));
            if auth_probe.as_ref() != Some(&key) || unauthorized {
                let mut endpoint = command.endpoint().clone();
                if let Some(base) = &discovered_base { endpoint.base = base.clone(); }
                if let Ok(info) = http_request(&client, HttpCommand::Configuration(endpoint, "/api/auth/info")) {
                    auth_probe = Some(key);
                    let _ = events.send(for_endpoint(command.endpoint(), info));
                }
            }
        }
        let source = command.diagnostic_source();
        if result.as_ref().is_ok_and(Event::http_succeeded) {
            if let Some(source) = &source {
                let recovered = Event::DiagnosticRecovered(source.clone());
                let recovered = match &command {HttpCommand::Scoped(scope, _) => Event::ForScope(scope.clone(), Box::new(recovered)),_ => recovered};
                let _ = events.send(for_endpoint(command.endpoint(), recovered));
            }
        }
        let event = result.unwrap_or_else(|e| command.failure(e));
        if events
            .send(for_endpoint(command.endpoint(), event))
            .is_err()
        {
            break;
        }
        context.request_repaint();
    }
}

fn websocket_url(base: &str) -> Result<String, String> {
    let parsed = reqwest::Url::parse(base).map_err(|e| e.to_string())?;
    let scheme = if parsed.scheme() == "https" {
        "wss"
    } else {
        "ws"
    };
    let host = parsed.host_str().ok_or("endpoint has no host")?;
    let authority = match parsed.port() {
        Some(p) => format!("{host}:{p}"),
        None => host.into(),
    };
    Ok(format!("{scheme}://{authority}/ws"))
}

fn rainbow_packet(offset: u16, count: u8, commit: bool, hue: u16) -> Vec<u8> {
    let mut packet = vec![0_u8; 10 + count as usize * 3];
    packet[0..4].copy_from_slice(b"PKAR");
    // Bytes 4 and 5 stay reserved zero, never an API generation.
    packet[6] = offset as u8;
    packet[7] = (offset >> 8) as u8;
    packet[8] = count;
    packet[9] = u8::from(commit);
    for i in 0..count as usize {
        let h = ((hue as usize + (offset as usize + i) * 3) % 360) as f32;
        let x = (1.0 - ((h / 60.0) % 2.0 - 1.0).abs()) * 255.0;
        let (r, g, b) = match (h / 60.0) as u8 {
            0 => (255, x as u8, 0),
            1 => (x as u8, 255, 0),
            2 => (0, 255, x as u8),
            3 => (0, x as u8, 255),
            4 => (x as u8, 0, 255),
            _ => (255, 0, x as u8),
        };
        let at = 10 + i * 3;
        packet[at..at + 3].copy_from_slice(&[r, g, b]);
    }
    packet
}

fn pixel_packet(pixels: &[u8], offset: u16, count: u8, commit: bool) -> Vec<u8> {
    let mut packet = vec![0_u8; 10 + count as usize * 3];
    packet[0..4].copy_from_slice(b"PKAR");
    // Bytes 4 and 5 stay reserved zero, never an API generation.
    packet[6] = offset as u8;
    packet[7] = (offset >> 8) as u8;
    packet[8] = count;
    packet[9] = u8::from(commit);
    let start = offset as usize * 3;
    let end = start + count as usize * 3;
    packet[10..].copy_from_slice(&pixels[start..end]);
    packet
}

fn sampled_screen_packets(mapping: &[CellMapping]) -> Result<(Vec<u8>, Vec<u8>), String> {
    let monitors = xcap::Monitor::all().map_err(|error| error.to_string())?;
    let monitor = monitors
        .iter()
        .find(|monitor| monitor.is_primary().unwrap_or(false))
        .or_else(|| monitors.first())
        .ok_or("no screen is available")?;
    let image = monitor.capture_image().map_err(|error| error.to_string())?;
    let mut pixels = vec![0_u8; 480 * 3];
    for (cell, sample_x, sample_y) in
        streaming::sample_positions(image.width(), image.height(), mapping)?
    {
        let color = image.get_pixel(sample_x, sample_y).0;
        for sector in 0..6 {
            let at = (cell * 6 + sector) * 3;
            pixels[at..at + 3].copy_from_slice(&color[..3]);
        }
    }
    Ok((
        pixel_packet(&pixels, 0, 240, false),
        pixel_packet(&pixels, 240, 240, true),
    ))
}

fn reject_pixel(events: &Sender<Event>, request: pixel_diagnostic::Request) {
    let endpoint = request.endpoint.clone();
    let _ = events.send(for_endpoint(&endpoint, Event::PixelSent(request,
        Err("WebSocket is not ready; diagnostic frame was not queued or replayed".into()))));
}
fn recover_realtime(request:realtime::Intent,events:&Sender<Event>,gate:&connection::Gate) {
    let result=if let Some(_lease)=gate.acquire(Transport::WiFi,request.stamp.epoch){request.http()}
        else{Err("Realtime connection retired; no fallback sent".into())};
    request.emit(events,result);
}
fn ws_worker(commands: Receiver<WsCommand>, events: Sender<Event>, context: egui::Context, gate: connection::Gate) {
    let mut retry = diagnostics::Backoff::new(Duration::from_millis(600), Duration::from_secs(15));
    let mut endpoint: Option<Endpoint> = None;
    let mut generation = 0;
    let mut streaming = false;
    let mut sampling = false;
    let mut fps = streaming::DEFAULT_FPS;
    let mut mapping = Vec::new();
    loop {
        if endpoint.is_none() {
            match commands.recv() {
                Ok(WsCommand::Connect(e, epoch)) => { endpoint = Some(e); generation = epoch; }
                Ok(WsCommand::Disconnect) => { endpoint = None; streaming = false; sampling = false; mapping.clear(); }
                Ok(WsCommand::Rate(value)) => fps = value.min(streaming::MAX_FPS),
                Ok(WsCommand::Mapping(value)) => mapping = value,
                Ok(WsCommand::Stream(v)) => streaming = v,
                Ok(WsCommand::Sample(v)) => sampling = v,
                Ok(WsCommand::Pixel(request) | WsCommand::ArmPixel(request)) => reject_pixel(&events, request),
                Ok(WsCommand::Control(request))=>{recover_realtime(request,&events,&gate);context.request_repaint();},
                Err(_) => return,
            }
        }
        loop {
            let command = match commands.try_recv() {
                Ok(command) => command,
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => return,
            };
            match command {
                WsCommand::Connect(e, epoch) => {
                    if endpoint.as_ref() != Some(&e) {
                        mapping.clear();
                        sampling = false;
                        streaming = false;
                    }
                    endpoint = Some(e);
                    generation = epoch;
                }
                WsCommand::Disconnect => { endpoint = None; streaming = false; sampling = false; mapping.clear(); }
                WsCommand::Rate(value) => fps = value.min(streaming::MAX_FPS),
                WsCommand::Mapping(value) => mapping = value,
                WsCommand::Stream(v) => streaming = v,
                WsCommand::Sample(v) => sampling = v,
                WsCommand::Pixel(request) | WsCommand::ArmPixel(request) => reject_pixel(&events, request),
                WsCommand::Control(request)=>{recover_realtime(request,&events,&gate);context.request_repaint();},
            }
        }
        let Some(current) = endpoint.clone() else {
            continue;
        };
        let Some(lease) = gate.acquire(Transport::WiFi, generation) else {
            thread::sleep(Duration::from_millis(20)); continue;
        };
        let events = SessionEvents { sender: events.clone(), stamp: lease.stamp() };
        let mut controls:Vec<(realtime::Intent,Instant)>=Vec::new();
        let mut awaiting_route = false;
        let result = (|| -> Result<(), String> {
            let mut request = websocket_url(&current.base)?
                .into_client_request()
                .map_err(|e| e.to_string())?;
            let auth = format!(
                "Basic {}",
                BASE64.encode(format!("{}:{}", current.user, current.password))
            );
            request.headers_mut().insert(
                "Authorization",
                auth.parse().map_err(|_| "invalid authorization")?,
            );
            request
                .headers_mut()
                .insert("Sec-WebSocket-Protocol", "arduino".parse().unwrap());
            let Some(mut socket) = peer_http::connect_websocket(&current, request, lease.stamp())? else {
                awaiting_route = true;
                return Ok(());
            };
            let _ = events.send(for_endpoint(&current, Event::WebSocket(true)));
            // A successful TCP/HTTP upgrade is not recovery if the service then
            // immediately closes or never acknowledges an actual board status.
            let mut confirmed = false;
            let mut last_received=Instant::now();
            let mut last_status=Instant::now()-Duration::from_secs(2);
            let mut last_status_request=Instant::now()-Duration::from_secs(2);
            let mut observed_peer: Option<(peer_http::Pin, Instant, bool, bool)> = None;
            let mut pending_arm: Option<(pixel_diagnostic::Request, bool)> = None;
            context.request_repaint();
            let mut hue = 0_u16;
            let mut last_stream = Instant::now() - Duration::from_millis(50);
            let mut stats_at = Instant::now();
            let mut sent_frames = 0_u32;
            loop {
                if !lease.current() { break; }
                // A connected TCP handle is not a live WS channel. Probe only
                // when authoritative pushes go quiet; retire a silent socket
                // so later commands do not pay an800ms fallback on every click.
                if last_received.elapsed()>=Duration::from_secs(8) {
                    return Err("WebSocket heartbeat timed out; reconnecting read-only".into());
                }
                if last_status.elapsed()>=Duration::from_secs(2) && last_status_request.elapsed()>=Duration::from_secs(2) {
                    socket.send(Message::Text(r#"{"type":"status"}"#.into())).map_err(|e|e.to_string())?;
                    last_status_request=Instant::now();
                }
                if pending_arm.as_ref().is_some_and(|(request, _)| request.at.elapsed() >= Duration::from_secs(8)) {
                    if let Some((request, _)) = pending_arm.take() {
                        let _ = events.send(for_endpoint(&current, Event::PixelModeFinished(request.id,
                            Err("Effects-mode ACK/readback timed out; no frame sent or mode write replayed".into()))));
                    }
                }
                match commands.try_recv() {
                    Ok(WsCommand::Control(request))=>{
                        if let Err(error)=request.admit(){request.emit(&events.sender,Err(error));}
                        else if controls.len()>=8 {request.emit(&events.sender,Err("Realtime command queue full; nothing sent".into()));}
                        else {
                            let sent=socket.send(Message::Text(request.packet.clone().into())).is_ok();
                            if sent {controls.push((request,Instant::now()));}
                            else {recover_realtime(request,&events.sender,&gate);}
                        }
                        context.request_repaint();
                    }
                    Ok(WsCommand::Connect(new_endpoint, epoch)) => {
                        mapping.clear();
                        sampling = false;
                        streaming = false;
                        endpoint = Some(new_endpoint);
                        generation = epoch;
                        break;
                    }
                    Ok(WsCommand::Disconnect) => { endpoint = None; mapping.clear(); sampling = false; streaming = false; break; }
                    Ok(WsCommand::Rate(value)) => fps = value.min(streaming::MAX_FPS),
                    Ok(WsCommand::Mapping(value)) => {
                        mapping = value;
                        if !streaming::valid_mapping(&mapping) {
                            sampling = false;
                        }
                    }
                    Ok(WsCommand::Stream(value)) => {
                        streaming = value;
                        if value {
                            sampling = false;
                        }
                    }
                    Ok(WsCommand::Sample(value)) => {
                        sampling = value && streaming::valid_mapping(&mapping);
                        if value {
                            streaming = false;
                        }
                    }
                    Ok(WsCommand::ArmPixel(request)) => {
                        if request.endpoint != current || request.at.elapsed() >= Duration::from_secs(3)
                            || pending_arm.is_some() || streaming || sampling
                            || !observed_peer.as_ref().is_some_and(|(peer, at, _, can_arm)|
                                peer == &request.peer && *can_arm && at.elapsed() < Duration::from_secs(8)) {
                            let _ = events.send(for_endpoint(&current, Event::PixelModeFinished(request.id,
                                Err("Stop streams and select the same enabled Cell board with no global effect before diagnostics".into()))));
                        } else {
                            socket.send(Message::Text(r#"{"command":"led-output","mode":"effects"}"#.into()))
                                .map_err(|_| "Mode write outcome unknown; no replay")?;
                            pending_arm = Some((request, false));
                        }
                        context.request_repaint();
                    }
                    Ok(WsCommand::Pixel(request)) => {
                        let result = (|| -> Result<(), String> {
                            if request.endpoint != current || request.at.elapsed() >= Duration::from_secs(3)
                                || !lease.current() || streaming || sampling
                                || pending_arm.is_some()
                                || !observed_peer.as_ref().is_some_and(|(peer, at, ready, _)|
                                    peer == &request.peer && *ready && at.elapsed() < Duration::from_secs(8)) {
                                return Err("Diagnostic peer/mode expired or another stream is active; frame not sent".into());
                            }
                            let pixels = pixel_diagnostic::pixels(request.index).ok_or("Pixel index outside 0..479")?;
                            socket.send(Message::Binary(pixel_packet(&pixels, 0, 240, false).into()))
                                .map_err(|_| "Diagnostic write outcome unknown; no frame was replayed")?;
                            if !lease.current() { return Err("Connection released after first chunk; frame not committed".into()); }
                            socket.send(Message::Binary(pixel_packet(&pixels, 240, 240, true).into()))
                                .map_err(|_| "Diagnostic commit outcome unknown; no frame was replayed")?;
                            Ok(())
                        })();
                        let _ = events.send(for_endpoint(&current, Event::PixelSent(request, result)));
                        context.request_repaint();
                    }
                    Err(TryRecvError::Disconnected) => return Ok(()),
                    Err(TryRecvError::Empty) => {}
                }
                if streaming && last_stream.elapsed() >= streaming::interval(fps) {
                    let frame_at = Instant::now();
                    socket
                        .send(Message::Binary(rainbow_packet(0, 240, false, hue).into()))
                        .map_err(|e| e.to_string())?;
                    socket
                        .send(Message::Binary(rainbow_packet(240, 240, true, hue).into()))
                        .map_err(|e| e.to_string())?;
                    hue = (hue + 4) % 360;
                    last_stream = frame_at;
                    sent_frames += 1;
                }
                if sampling && last_stream.elapsed() >= streaming::interval(fps) {
                    let frame_at = Instant::now();
                    let (first, second) = sampled_screen_packets(&mapping)?;
                    socket
                        .send(Message::Binary(first.into()))
                        .map_err(|e| e.to_string())?;
                    socket
                        .send(Message::Binary(second.into()))
                        .map_err(|e| e.to_string())?;
                    last_stream = frame_at;
                    sent_frames += 1;
                }
                if stats_at.elapsed() >= Duration::from_secs(1) {
                    let measured = f64::from(sent_frames) / stats_at.elapsed().as_secs_f64();
                    let _ = events.send(for_endpoint(&current, Event::StreamStats(measured)));
                    sent_frames = 0;
                    stats_at = Instant::now();
                    context.request_repaint();
                }
                peer_http::websocket_read_timeout(&mut socket, streaming::read_budget(
                    streaming || sampling, fps, last_stream.elapsed()))?;
                match socket.read() {
                    Ok(Message::Text(text)) => {
                        last_received=Instant::now();
                        if let Ok(value)=serde_json::from_str::<serde_json::Value>(&text) && value["type"]=="status-rgb" {
                            if let Some(rgb)=status_rgb::Observation::parse(value){let _=events.send(for_endpoint(&current,Event::StatusRgb(rgb)));context.request_repaint();}
                            continue; // Optional RGB cannot become a partial full-status packet.
                        }
                        if let Ok(value)=serde_json::from_str::<serde_json::Value>(&text)
                            && value["type"]=="command-result"
                            && let Some(index)=controls.iter().position(|(request,_)|value["request_id"]==request.id) {
                            let (request,_)=controls.remove(index);
                            let result=request.verify(value);if result.is_ok(){last_status=Instant::now();}request.emit(&events.sender,result);
                            context.request_repaint();
                            continue;
                        }
                        if let Some(upgrade)=firmware_progress::websocket_upgrade(&text){
                            let _=events.send(for_endpoint(&current,Event::UpgradeProgress(upgrade)));
                            context.request_repaint();continue;
                        }
                        if pending_arm.as_ref().is_some_and(|(_, acked)| !acked) {
                            if let Some(ack) = pixel_diagnostic::mode_ack(&text) {
                                match ack {
                                    Ok(()) => {
                                        if let Some((_, acked)) = pending_arm.as_mut() { *acked = true; }
                                        socket.send(Message::Text(r#"{"type":"status"}"#.into())).map_err(|e| e.to_string())?;
                                    }
                                    Err(error) => if let Some((request, _)) = pending_arm.take() {
                                        let _ = events.send(for_endpoint(&current, Event::PixelModeFinished(request.id, Err(error))));
                                    },
                                }
                            }
                        }
                        if let Some(status) = websocket_status(&text) {
                            last_status=Instant::now();
                            observed_peer = peer_http::Pin::from_status(&status)
                                .map(|pin| (pin, Instant::now(), pixel_diagnostic::ready(&status), pixel_diagnostic::can_arm(&status)));
                            if pending_arm.as_ref().is_some_and(|(request, acked)| *acked
                                && peer_http::Pin::from_status(&status).as_ref() == Some(&request.peer)
                                && pixel_diagnostic::ready(&status)) {
                                if let Some((request, _)) = pending_arm.take() {
                                    let _ = events.send(for_endpoint(&current, Event::PixelModeFinished(request.id, Ok(status.clone()))));
                                }
                            }
                            if !confirmed && websocket_status_ready(&status) {
                                confirmed = true; retry.reset();
                                let _ = events.send(for_endpoint(&current, Event::DiagnosticRecovered("WebSocket".into())));
                            }
                            let _ = events.send(for_endpoint(&current, Event::Status(status)));
                            context.request_repaint();
                        }
                    }
                    Ok(Message::Close(_)) => return Err("WebSocket closed".into()),
                    Ok(_) => {last_received=Instant::now();}
                    Err(tungstenite::Error::Io(e))
                        if matches!(e.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) => {}
                    Err(e) => return Err(e.to_string()),
                }
                let mut index=0;
                while index<controls.len() {
                    if controls[index].1.elapsed()>=Duration::from_millis(800) {
                        let (request,_)=controls.remove(index);
                        recover_realtime(request,&events.sender,&gate);context.request_repaint();
                    }else{index+=1;}
                }
            }
            Ok(())
        })();
        if !awaiting_route {
            let _ = events.send(for_endpoint(&current, Event::WebSocket(false)));
            context.request_repaint();
        }
        drop(lease); // The closure dropped the socket before another method opens.
        for (request,_) in controls {recover_realtime(request,&events.sender,&gate);}
        if result.is_err() || awaiting_route {
            if let Err(error) = result {
                streaming = false;
                sampling = false; // Reconnect must not silently restart a stream.
                let _ = events.send(for_endpoint(
                    &current,
                    Event::DiagnosticFailure("WebSocket".into(), error),
                ));
            }
            // Interruptible: changing peer or stopping streaming need not wait
            // for the retry deadline. Mutations are never replayed here.
            let delay = if awaiting_route { Duration::from_millis(250) } else { let delay = retry.delay(); retry.failed(); delay };
            match commands.recv_timeout(delay) {
                Ok(WsCommand::Connect(e, epoch)) => { endpoint = Some(e); generation = epoch; mapping.clear(); streaming = false; sampling = false; retry.reset(); }
                Ok(WsCommand::Disconnect) => { endpoint = None; mapping.clear(); streaming = false; sampling = false; retry.reset(); }
                Ok(WsCommand::Rate(value)) => fps = value.min(streaming::MAX_FPS),
                Ok(WsCommand::Mapping(value)) => mapping = value,
                Ok(WsCommand::Stream(value)) => streaming = value,
                Ok(WsCommand::Sample(value)) => sampling = value,
                Ok(WsCommand::Pixel(request) | WsCommand::ArmPixel(request)) => reject_pixel(&events.sender, request),
                Ok(WsCommand::Control(request))=>{recover_realtime(request,&events.sender,&gate);context.request_repaint();},
                Err(mpsc::RecvTimeoutError::Disconnected) => return,
                Err(mpsc::RecvTimeoutError::Timeout) => {},
            }
        }
    }
}

fn crc8(data: &[u8]) -> u8 {
    let mut crc = 0_u8;
    for byte in data {
        crc ^= *byte;
        for _ in 0..8 {
            crc = if crc & 0x80 != 0 {
                (crc << 1) ^ 0x07
            } else {
                crc << 1
            };
        }
    }
    crc
}
fn cobs_encode(input: &[u8]) -> Vec<u8> {
    let mut out = vec![0_u8];
    let mut code_at = 0;
    let mut code = 1_u8;
    for byte in input {
        if *byte == 0 {
            out[code_at] = code;
            code_at = out.len();
            out.push(0);
            code = 1;
        } else {
            out.push(*byte);
            code += 1;
            if code == 0xff {
                out[code_at] = code;
                code_at = out.len();
                out.push(0);
                code = 1;
            }
        }
    }
    out[code_at] = code;
    out.push(0);
    out
}
fn cobs_decode(input: &[u8]) -> Option<Vec<u8>> {
    let mut out = Vec::new();
    let mut at = 0;
    while at < input.len() {
        let code = *input.get(at)? as usize;
        if code == 0 || at + code > input.len() + 1 {
            return None;
        }
        at += 1;
        for _ in 1..code {
            out.push(*input.get(at)?);
            at += 1;
        }
        if code != 0xff && at < input.len() {
            out.push(0);
        }
    }
    Some(out)
}
fn wire_frame(opcode: u8, sequence: u8, payload: &[u8]) -> Vec<u8> {
    let mut raw = vec![0xa5, 0, opcode, sequence, payload.len() as u8];
    raw.extend_from_slice(payload);
    raw.push(crc8(&raw));
    cobs_encode(&raw)
}
fn decode_frame(encoded: &[u8]) -> Option<(u8, Vec<u8>)> {
    decode_envelope(encoded).map(|(op,_,payload)|(op,payload))
}
fn decode_envelope(encoded: &[u8]) -> Option<(u8, u8, Vec<u8>)> {
    let raw = cobs_decode(encoded)?;
    if raw.len() < 6
        || raw[0] != 0xa5
        || raw[1] != 0
        || raw.len() != raw[4] as usize + 6
        || crc8(&raw[..raw.len() - 1]) != raw[raw.len() - 1]
    {
        return None;
    }
    Some((raw[2], raw[3], raw[5..raw.len() - 1].to_vec()))
}
fn serial_status(payload: &[u8]) -> Option<DeviceStatus> {
    if payload.len() < 19 {
        return None;
    }
    Some(DeviceStatus {
        capabilities: capabilities::Manifest::from_usb(payload),
        mode: ["unknown", "cell", "first-wins", "snake"]
            .get(payload[0] as usize)
            .unwrap_or(&"unknown")
            .to_string(),
        winner: payload[1],
        selected_cell: payload[2],
        brightness: payload[3],
        led_output: payload.get(42).filter(|v| **v <= 1).map(|v| {
            let mut state = serde_json::json!({"enabled": *v != 0});
            if let Some(mode) = payload.get(43).and_then(|v| ["static", "effects"].get(*v as usize)) {
                state["mode"] = (*mode).into();
            }
            state
        }).unwrap_or_default(),
        effect: ["none", "rainbow", "fire"]
            .get(payload[4] as usize)
            .unwrap_or(&"unknown")
            .to_string(),
        avr_online: payload[5] != 0,
        tm1637_ack: payload[6] != 0,
        tm1637_segments: payload.get(19..23).unwrap_or_default().to_vec(),
        tm1637_brightness: payload.get(23).copied().unwrap_or_default(),
        tm1637_host_active: payload.get(24).is_some_and(|value| *value != 0),
        avr_version: if payload.len() >= 29 && payload[5] != 0 && payload[28] > 0 {
            Some(format!("{}.{}.{}", payload[25], payload[26], payload[27]))
        } else {
            None
        },
        audio_cues: payload
            .get(28)
            .filter(|count| payload[5] != 0 && **count as usize <= audio::CUES.len())
            .map(|count| {
                audio::CUES[..*count as usize]
                    .iter()
                    .map(|(name, _)| (*name).to_owned())
                    .collect()
            })
            .unwrap_or_default(),
        programming_latch: payload[7] != 0,
        active_inputs: payload[8],
        input_9: payload[9] != 0,
        active_outputs: payload[10],
        esp_uart_errors: u16::from_le_bytes([payload[11], payload[12]]),
        avr_uart_errors: u16::from_le_bytes([payload[13], payload[14]]),
        station_connected: payload[15] != 0,
        ap_started: payload[16] != 0,
        ntp_synced: payload[17] != 0,
        link_mode: if payload[18] != 0 {
            "busy-wire"
        } else {
            "polling"
        }
        .into(),
        ..Default::default()
    })
}

fn serial_worker(commands: Receiver<SerialCommand>, events: Sender<Event>, context: egui::Context, gate: connection::Gate) {
    serial_worker_with(commands,events,context,gate,|name|serialport::new(name,115_200)
        .dtr_on_open(false).timeout(Duration::from_millis(10)).open().map_err(|error|error.to_string()));
}
// Inject the byte transport, not the scheduler/decoder, for deterministic tests
// of the actual production worker (PCController Session fakePort pattern).
fn serial_worker_with<P:Read+Write,F:FnMut(&str)->Result<P,String>>(
    commands:Receiver<SerialCommand>,events:Sender<Event>,context:egui::Context,gate:connection::Gate,mut open:F) {
    let mut port_name = String::new();
    let mut generation = 0;
    let mut sequence = 1_u8;
    let mut nonce = 0_u8;
    let mut physical_session=0_u64;
    loop {
        if port_name.is_empty() {
            match commands.recv() {
                Ok(SerialCommand::Port(p, epoch)) => { port_name = p; generation = epoch; }
                Ok(_) => {}
                Err(_) => return,
            }
        }
        if port_name.is_empty() { continue; }
        loop {
            let c = match commands.try_recv() {
                Ok(command) => command,
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => return,
            };
            if let SerialCommand::Port(p, epoch) = c { port_name = p; generation = epoch; }
        }
        if port_name.is_empty() { continue; }
        let Some(lease) = gate.acquire(Transport::Serial, generation) else {
            thread::sleep(Duration::from_millis(20)); continue;
        };
        let events = SessionEvents { sender: events.clone(), stamp: lease.stamp() };
        let mut port = match open(&port_name) {
            Ok(port) => port,
            Err(error) => {
                let _ = events.send(Event::SerialFailure(port_name.clone(), error.to_string()));
                let _ = events.send(Event::Serial(port_name.clone(),false,None));
                context.request_repaint();
                thread::sleep(Duration::from_secs(1));
                while let Ok(c) = commands.try_recv() { if let SerialCommand::Port(p, epoch)=c {port_name=p;generation=epoch;} }
                continue;
            }
        };
        let connected_port = port_name.clone();
        let Some(next_session)=physical_session.checked_add(1) else{return;};physical_session=next_session;
        let _=events.send(Event::SerialOwner(connected_port.clone(),physical_session));
        let _ = events.send(Event::Serial(connected_port.clone(),true,None));
        context.request_repaint();
        let mut encoded=Vec::new();
        let mut last_poll=Instant::now()-Duration::from_secs(1);
        let mut last_identity=Instant::now()-Duration::from_secs(20);
        let mut stamp=None;
        let mut identity: Option<serde_json::Value>=None;
        let mut read: Option<identity::UsbRead>=None;
        let mut read_wait: Option<(u8,Instant)>=None;
        let mut packets=std::collections::VecDeque::new();
        let mut write_wait: Option<(u8,Instant)>=None;
        let mut expected: Option<(String,String)>=None;
        let mut identity_admission:Option<usb_table::Admission>=None;
        let mut table_supported=false;
        let mut compact_supported=false;
        let mut table_read:Option<usb_table::Reader>=None;
        let mut paused_table:Option<usb_table::Reader>=None;
        let mut last_command_state:Option<(Vec<u8>,Instant)>=None;
        let mut table_wait:Option<(u8,Instant)>=None;
        let mut table_following=std::collections::VecDeque::<(u8,Instant)>::new();
        let mut table_started=Instant::now();
        let mut progress_at=Instant::now();
        let mut last_table=Instant::now()-Duration::from_secs(2);
        let mut table_snapshot:Option<DeviceStatus>=None;
        let mut table_observed=Instant::now()-Duration::from_secs(20);
        let mut intents=std::collections::VecDeque::<usb_table::Intent>::new();
        let mut operation:Option<usb_table::Operation>=None;
        let mut silent: Option<(u64, usb_table::Admission, board_silent::Transaction, Option<(u8, Instant)>)> = None;
        let mut reported_depth=0;
        'connected: loop {
            if !lease.current() { break; }
            while let Ok(c)=commands.try_recv() {
                match c {
                    SerialCommand::Port(p, epoch)=>{port_name=p;generation=epoch;break 'connected}
                    SerialCommand::Send(target,admission,op,payload) if target==connected_port && lease.current()=>{
                        if !admission.current(physical_session,table_snapshot.as_ref()) {
                            let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB command belongs to a retired UI/session/peer; nothing sent".into())));
                        }else if payload.len()>48 || intents.len()>=32 {
                            let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB command queue full or payload too large; nothing sent".into())));
                        }else if usb_table::table_command(op) && table_snapshot.is_none(){
                            let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB table snapshot not verified; nothing sent".into())));
                        }else{intents.push_back(usb_table::Intent{opcode:op,payload,admission,reply:None});}
                    }
                    SerialCommand::Bridge(target,admission,op,payload,reply) if target==connected_port && lease.current()=>{
                        let intent=usb_table::Intent{opcode:op,payload,admission,reply:Some(reply)};
                        if !intent.current(physical_session,table_snapshot.as_ref()) || intents.len()>=32 || !usb_table::table_command(op){
                            intent.reply(Err("USB bridge request expired, unsupported or owner unavailable; nothing sent".into()));
                        }else{intents.push_back(intent);}
                    }
                    SerialCommand::BoardSilent(target, admission, id, desired) if target == connected_port => {
                        if silent.is_some() || !admission.current(physical_session, table_snapshot.as_ref())
                            || !table_snapshot.as_ref().is_some_and(|s| desired.allowed(s)) {
                            let _ = events.send(Event::BoardSilentFinished(id, Err("USB board settings busy, unavailable or peer retired; nothing sent".into())));
                        } else { silent = Some((id, admission, board_silent::Transaction::new(desired), None)); }
                    }
                    SerialCommand::Identity(target,admission,host,label) if target==connected_port && lease.current()=>{
                        if admission.peer.is_none() || !admission.current(physical_session,table_snapshot.as_ref()) {
                            let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("USB identity request belongs to an unverified or retired board; nothing sent".into())));
                        }else if expected.is_some() {
                            let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("An identity write is already pending".into())));
                        } else {
                            match identity::usb_write(&host,&label) {
                                Ok(next)=>{packets=next;identity_admission=Some(admission);expected=Some((identity::normalize_name(&host).unwrap(),label));read=None;read_wait=None;}
                                Err(error)=>{let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err(error)));}
                            }
                        }
                    }
                    _=>{}
                }
            }
            if operation.as_ref().is_some_and(|op|op.ack.is_none() && op.sent.elapsed()>Duration::from_secs(2)) {
                let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB command ACK timed out; outcome unknown, nothing replayed".into())));
                break 'connected;
            }
            // READ windows share one absolute capture deadline, not a shorter
            // per-fragment timer. Commands retain their separate2s ACK bound.
            // A delayed read can finish only while its ORIGINAL8s age is valid.
            if (table_read.is_some() || paused_table.is_some()) && table_started.elapsed()>=usb_table::MAX_SNAPSHOT_AGE {
                if let Some(reader)=table_read.as_ref().or(paused_table.as_ref()) {
                    let _=events.send(Event::SerialCapture(connected_port.clone(),reader.progress("expired")));
                }
                table_read=None;paused_table=None;table_wait=None;table_following.clear();last_table=Instant::now();
                let error="USB table snapshot timed out; no state assumed and no command replayed".to_string();
                let _=events.send(Event::SerialFailure(connected_port.clone(),error.clone()));
                if operation.take().is_some(){let _=events.send(Event::SerialOperation(connected_port.clone(),Err(error)));}
            }
            // Yield only BETWEEN correlated chunks. Never preempt an in-flight
            // request or a post-ACK verification. A compact-capable peer can
            // resume immutable metadata after control; overlay newer actual
            // command readback and invalidate pre-command RGB before publishing.
            // This lets full identity/peripheral reads progress during bursts.
            if silent.is_none() && usb_table::yield_snapshot(!intents.is_empty(),table_snapshot.is_some(),table_read.is_some(),table_wait.is_some(),operation.is_some()) {
                if compact_supported{paused_table=table_read.take();}else{table_read=None;}
                last_table=Instant::now();
            }
            if silent.is_none() && operation.is_none() && table_read.is_none() && write_wait.is_none() && expected.is_none()
                && let Some(intent)=intents.front() {
                let is_table=usb_table::table_command(intent.opcode);
                if !intent.admission.current(physical_session,table_snapshot.as_ref()) || (is_table && !intent.current(physical_session,table_snapshot.as_ref())) {
                    intents.pop_front();
                    let _=events.send(Event::SerialOperation(connected_port.clone(),Err("Queued USB command expired or peer changed; nothing sent".into())));
                }else if !is_table || table_observed.elapsed()<usb_table::MAX_SNAPSHOT_AGE{
                    let intent=intents.pop_front().unwrap();
                    let packet=if is_table {usb_table::fenced(table_snapshot.as_ref().unwrap(),intent.opcode,&intent.payload)}else{Ok(intent.payload.clone())};
                    match packet {
                        Ok(payload)=>{
                            if !lease.current() || !intent.admission.current(physical_session,table_snapshot.as_ref()){
                                let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB command admission revoked before write; nothing sent".into())));continue 'connected;}
                            let sent=Instant::now();
                            if let Err(error)=port.write_all(&wire_frame(if is_table{usb_table::COMMAND}else{intent.opcode},sequence,&payload)){
                                let _=events.send(Event::SerialOperation(connected_port.clone(),Err(format!("USB write failed; outcome unknown, no replay: {error}"))));break 'connected;
                            }
                            operation=Some(usb_table::Operation{intent,sequence,sent,ack:None});sequence=sequence.wrapping_add(1);
                        }
                        Err(error)=>{let _=events.send(Event::SerialOperation(connected_port.clone(),Err(error)));}
                    }
                }
            }
            if operation.as_ref().is_some_and(|op|op.ack.is_some_and(|at|at.elapsed()>=Duration::from_millis(100))) {
                paused_table=None; // Missing compact companion: require a NEW post-ACK capture.
            }
            if silent.is_none() && operation.is_none() && table_read.is_none() && intents.is_empty() {
                table_read=paused_table.take();
            }
            if silent.is_none() && table_supported && table_read.is_none() && paused_table.is_none() && write_wait.is_none() && expected.is_none()
                && usb_table::snapshot_admitted(!intents.is_empty(),operation.as_ref(),compact_supported)
                && last_table.elapsed()>=Duration::from_secs(1) {
                nonce=nonce.wrapping_add(1);table_read=Some(usb_table::Reader::new(nonce));
                table_started=Instant::now();last_table=Instant::now();
            }
            if let Some((id, admission, transaction, wait)) = &silent {
                if !admission.current(physical_session, table_snapshot.as_ref())
                    || !table_snapshot.as_ref().is_some_and(|s| transaction.allowed(s))
                    || wait.is_some_and(|(_, at)| at.elapsed() > Duration::from_secs(2)) {
                    let _ = events.send(Event::BoardSilentFinished(*id, Err("USB settings timed out or peer retired; outcome unknown, no replay".into())));
                    silent = None;
                }
            }
            if operation.is_none() && table_read.is_none() && write_wait.is_none() && expected.is_none()
                && let Some((id, admission, transaction, wait)) = &mut silent && wait.is_none() {
                let (opcode, mut payload) = transaction.request(); payload.insert(0, opcode);
                if !lease.current() || !admission.current(physical_session, table_snapshot.as_ref())
                    || port.write_all(&wire_frame(0x56, sequence, &payload)).is_err() {
                    let _ = events.send(Event::BoardSilentFinished(*id, Err("USB settings write unavailable; outcome unknown, no replay".into())));
                    break 'connected;
                }
                *wait = Some((sequence, Instant::now())); sequence = sequence.wrapping_add(1);
            }
            if lease.current() && let Some(reader)=&table_read && table_wait.is_none(){
                if port.write_all(&wire_frame(usb_table::READ,sequence,&reader.request())).is_err(){break 'connected;}
                table_wait=Some((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                for request in reader.following_requests() {
                    if port.write_all(&wire_frame(usb_table::READ,sequence,&request)).is_err(){break 'connected;}
                    table_following.push_back((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                }
            }
            if progress_at.elapsed()>=Duration::from_millis(500) && let Some(reader)=table_read.as_ref().or(paused_table.as_ref()) {
                let _=events.send(Event::SerialCapture(connected_port.clone(),reader.progress("reading")));
                progress_at=Instant::now();
            }
            if write_wait.is_some_and(|(_,at)|at.elapsed()>Duration::from_secs(2)) ||
                read_wait.is_some_and(|(_,at)|at.elapsed()>Duration::from_secs(2)) ||
                (read.is_some() && last_identity.elapsed()>Duration::from_secs(8)) {
                packets.clear();write_wait=None;read_wait=None;read=None;identity=None;
                if expected.take().is_some() {
                    let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("Identity response timed out. Refresh before retrying; no mutation was replayed.".into())));
                }
                last_identity=Instant::now(); // Bounded backoff for a read-only refresh.
            }
            if lease.current() && write_wait.is_none() {
                if let Some(payload)=packets.pop_front() {
                    if !identity_admission.as_ref().is_some_and(|admission|admission.current(physical_session,table_snapshot.as_ref())){
                        packets.clear();expected=None;identity_admission=None;
                        let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("USB identity admission revoked before write; no replay".into())));continue 'connected;
                    }
                    if port.write_all(&wire_frame(0x55,sequence,&payload)).is_err(){break}
                    write_wait=Some((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                } else if let Some(reader)=&read {
                    if read_wait.is_none() {
                        if port.write_all(&wire_frame(0x59,sequence,&reader.request())).is_err(){break}
                        read_wait=Some((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                    }
                }
            }
            if lease.current() && last_poll.elapsed()>=Duration::from_millis(750) {
                if port.write_all(&wire_frame(OP_GET_STATUS,sequence,&[])).is_err(){break 'connected;}
                sequence=sequence.wrapping_add(1);last_poll=Instant::now();
            }
            let depth=intents.len()+usize::from(operation.is_some());
            if depth!=reported_depth {reported_depth=depth;let _=events.send(Event::SerialQueueDepth(connected_port.clone(),depth));context.request_repaint();}
            let mut bytes=[0_u8;128];
            match port.read(&mut bytes) {
                Ok(n)=>for byte in &bytes[..n] {
                    if *byte==0 {
                        if let Some((op,response_sequence,payload))=decode_envelope(&encoded) {
                            if (op == 0xb0 || op == 0x82) && silent.as_ref().is_some_and(|(_, _, _, wait)|
                                wait.is_some_and(|(s, _)| s == response_sequence)) {
                                let (id, _, transaction, wait) = silent.as_mut().unwrap();
                                *wait = None;
                                let result = if op == 0xb0 && !payload.is_empty() { transaction.accept(payload[0], &payload[1..]) }
                                    else { Err("ESP rejected board settings; refresh before retrying".into()) };
                                match result {
                                    Ok(None) => {}
                                    Ok(Some(settings)) => { let id = *id; silent = None; let _ = events.send(Event::BoardSilentFinished(id, Ok(settings))); }
                                    Err(error) => { let id = *id; silent = None; let _ = events.send(Event::BoardSilentFinished(id, Err(error))); }
                                }
                            } else if op==OP_STATUS {
                                let next_table_supported=payload.get(44)==Some(&1);
                                if table_supported && !next_table_supported {
                                    table_snapshot=None;table_read=None;paused_table=None;table_wait=None;table_following.clear();intents.clear();
                                    if operation.take().is_some(){let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB table capability disappeared; outcome unverified, no replay".into())));}
                                }
                                table_supported=next_table_supported;
                                compact_supported=table_supported && payload.get(45)==Some(&1);
                                if !table_supported && payload.len()>=38 && payload[31]&(1<<5)!=0 && packets.is_empty() && write_wait.is_none()
                                    && read.is_none() && (stamp!=Some(payload[37]) || last_identity.elapsed()>Duration::from_secs(10)) {
                                    if stamp!=Some(payload[37]) { identity=None; }
                                    stamp=Some(payload[37]);nonce=nonce.wrapping_add(1);
                                    read=Some(identity::UsbRead{nonce,..Default::default()});last_identity=Instant::now();
                                }
                                if !table_supported && let Some(mut status)=serial_status(&payload) {
                                    if let (Some(value),Some(c))=(&identity,&mut status.capabilities) {
                                        c.name=value["display_name"].as_str().unwrap_or_default().into();
                                        c.hostname=value["hostname"].as_str().unwrap_or_default().into();
                                        c.application=serde_json::from_value(value["application"].clone()).ok();
                                        status.hostname=c.hostname.clone();
                                    }
                                    let _=events.send(Event::Serial(connected_port.clone(),true,Some(status)));
                                }
                            } else if op==usb_table::RESPONSE && table_wait.is_some_and(|(s,_)|s==response_sequence) {
                                table_wait=table_following.pop_front();
                                match table_read.as_mut().unwrap().accept(&payload){
                                    Ok(Some(mut snapshot))=>{
                                        let _=events.send(Event::SerialCapture(connected_port.clone(),table_read.as_ref().unwrap().progress("complete")));
                                        table_read=None;table_observed=snapshot.observed_at;
                                        if let Some((packet,at))=&last_command_state && *at>snapshot.observed_at
                                            && peer_http::Pin::from_status(&snapshot.status)==table_snapshot.as_ref().and_then(peer_http::Pin::from_status) {
                                            // Background data may predate control. Never roll cells back
                                            // or present its old RGB as the newly applied pixel frame.
                                            if let Ok(status)=usb_table::command_state(&snapshot.status,packet[0],packet){snapshot.status=status;}
                                        }
                                        let pin=peer_http::Pin::from_status(&snapshot.status);
                                        if let Some(active)=operation.as_ref().filter(|op|op.ack.is_some()){
                                            let intent=&active.intent;
                                            let result=if intent.admission.peer==pin && intent.verify(&snapshot.status){Ok("USB command ACK and same-board snapshot readback verified".into())}
                                                else{Err("USB ACK received but board/boot or applied values differ; no replay attempted".into())};
                                            if intent.opcode==cell_batch::OPCODE && result.is_ok(){
                                                if let Some(correlation)=intent.admission.batch.as_ref(){
                                                    let _=events.send(Event::SerialBatchConfirmed(connected_port.clone(),correlation.clone()));
                                                }
                                            }
                                            active.intent.reply(result.as_ref().map(|_|snapshot.status.clone()).map_err(Clone::clone));
                                            if result.is_ok(){let _=events.send(Event::SerialTiming(connected_port.clone(),active.timing(physical_session,"full snapshot")));}
                                            let _=events.send(Event::SerialOperation(connected_port.clone(),result));operation=None;
                                        }
                                        table_snapshot=Some(snapshot.status.clone());
                                        let _=events.send(Event::SerialSnapshot(connected_port.clone(),snapshot));
                                    }
                                    Ok(None)=>{}
                                    Err(error)=>{
                                        table_read=None;table_wait=None;table_following.clear();last_table=Instant::now();
                                        let _=events.send(Event::SerialFailure(connected_port.clone(),error.clone()));
                                        if operation.take().is_some(){let _=events.send(Event::SerialOperation(connected_port.clone(),Err(error)));}
                                    }
                                }
                            } else if op==usb_table::COMMAND_STATE && compact_supported && operation.as_ref().is_some_and(|active|
                                active.sequence==response_sequence && active.ack.is_some() && payload.first()==Some(&active.intent.opcode)) {
                                let active=operation.as_ref().unwrap();
                                let result=table_snapshot.as_ref().ok_or_else(||"USB table owner unavailable".to_string())
                                    .and_then(|status|usb_table::command_state(status,active.intent.opcode,&payload))
                                    .and_then(|status|if active.intent.admission.peer==peer_http::Pin::from_status(&status) && active.intent.verify(&status){Ok(status)}else{Err("USB ACK readback differs from this intent/peer; no replay".into())});
                                match result {
                                    Ok(status)=>{
                                        // Do not renew capability/peripheral freshness using this partial readback.
                                        table_snapshot=Some(status.clone());table_read=paused_table.take();table_wait=None;table_following.clear();last_table=Instant::now();
                                        last_command_state=Some((payload.clone(),Instant::now()));
                                        let _=events.send(Event::SerialCommandState(connected_port.clone(),status.clone()));
                                        active.intent.reply(Ok(status));
                                        if let Some(correlation)=active.intent.admission.batch.as_ref(){let _=events.send(Event::SerialBatchConfirmed(connected_port.clone(),correlation.clone()));}
                                        let timing=active.timing(physical_session,"compact board readback");
                                        let message=format!("USB ACK + board readback · {:.1} ms (queue {:.1} ms)",timing.wire_confirm_ms,timing.queue_ms);
                                        let _=events.send(Event::SerialTiming(connected_port.clone(),timing));
                                        let _=events.send(Event::SerialOperation(connected_port.clone(),Ok(message)));
                                    }
                                    Err(error)=>{active.intent.reply(Err(error.clone()));table_read=paused_table.take();let _=events.send(Event::SerialOperation(connected_port.clone(),Err(error)));}
                                }
                                operation=None;
                            } else if operation.as_ref().is_some_and(|active|active.matches(op,response_sequence,&payload)){
                                if op==0x80 && payload[1]==0 {
                                    if usb_table::table_command(operation.as_ref().unwrap().intent.opcode){
                                        operation.as_mut().unwrap().ack=Some(Instant::now());table_read=None;table_wait=None;table_following.clear();last_table=Instant::now()-Duration::from_secs(2);
                                    }else{operation=None;table_read=paused_table.take();let _=events.send(Event::SerialOperation(connected_port.clone(),Ok("USB device ACK received; no table state inferred".into())));}
                                }else{operation=None;table_read=paused_table.take();let _=events.send(Event::SerialOperation(connected_port.clone(),Err(format!("USB command rejected (error {}); no replay",payload[1]))));}
                            } else if op==0x82 && payload.first()==Some(&usb_table::READ) && table_wait.is_some_and(|(s,_)|s==response_sequence){
                                table_read=None;table_wait=None;table_following.clear();last_table=Instant::now();
                                let error="Board rejected USB table snapshot; no state assumed".to_string();
                                let _=events.send(Event::SerialFailure(connected_port.clone(),error.clone()));
                                if operation.take().is_some(){let _=events.send(Event::SerialOperation(connected_port.clone(),Err(error)));}
                            } else if op==0xb2 && read_wait.is_some_and(|(s,_)|s==response_sequence) {
                                read_wait=None;
                                let result=read.as_mut().unwrap().accept(&payload);
                                match result {
                                    Ok(Some(value))=>{
                                        read=None;
                                        if let Some((host,label))=expected.take() {
                                            let valid=value["hostname"].as_str()==Some(host.as_str())&&value["display_name"].as_str()==Some(label.as_str());
                                            let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),if valid {Ok(())} else {Err("Identity ACK received but readback differs; no replay attempted".into())}));
                                        }
                                        let _=events.send(Event::SerialIdentity(connected_port.clone(),value.clone()));
                                        identity=Some(value);
                                    }
                                    Ok(None)=>{}
                                    Err(error)=>{
                                        read=None;identity=None;
                                        if expected.take().is_some(){let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err(error)));}
                                    }
                                }
                            } else if write_wait.is_some_and(|(s,_)|s==response_sequence) && (op==0x80 || op==0x82) && payload.first()==Some(&0x55) {
                                write_wait=None;
                                if op==0x82 {
                                    packets.clear();expected=None;
                                    let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err(format!("Board rejected identity write ({:?}); no replay attempted",payload.get(1)))));
                                } else if packets.is_empty() {
                                    identity=None;nonce=nonce.wrapping_add(1);
                                    read=Some(identity::UsbRead{nonce,..Default::default()});last_identity=Instant::now();
                                }
                            }
                            context.request_repaint();
                        }
                        encoded.clear();
                    } else if encoded.len()<80 {encoded.push(*byte)} else {encoded.clear()}
                },
                Err(e) if matches!(e.kind(),ErrorKind::TimedOut|ErrorKind::WouldBlock)=>{}
                Err(error)=>{
                    let _=events.send(Event::SerialFailure(connected_port.clone(),error.to_string()));
                    break;
                },
            }
        }
        drop(port); // Release the real handle before the next method may acquire.
        if let Some((id, ..)) = silent { let _ = events.send(Event::BoardSilentFinished(id, Err("USB disconnected during settings; outcome unknown, no replay".into()))); }
        let _=events.send(Event::SerialQueueDepth(connected_port.clone(),0));
        if operation.is_some() || !intents.is_empty(){let _=events.send(Event::SerialOperation(connected_port.clone(),Err("USB session ended; pending commands were not replayed. Inspect device state before retrying".into())));}
        if expected.is_some(){let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("USB disconnected during identity write; reconnect and read before retrying".into())));}
        let _=events.send(Event::Serial(connected_port,false,None));context.request_repaint();
    }
}
// A selected method never falls back to another transport, including on error.
fn command_route(selected: Transport, wifi_fresh: bool, usb_fresh: bool) -> Option<Transport> {
    match selected {
        Transport::WiFi if wifi_fresh => Some(Transport::WiFi),
        Transport::Serial if usb_fresh => Some(Transport::Serial),
        _ => None,
    }
}
struct PakarControllerApp {
    firmware_feed: Option<firmware_feed::Session>,
    firmware_transfer: Option<firmware_progress::Transfer>,
    firmware_feed_message: String,
    bridge: bridge::Runtime,
    connection_gate: connection::Gate,
    connection_enabled: bool,
    active_context: Option<String>,
    config: config::AppConfig,
    workspace: native_workspace::Runtime,
    preferences: Option<ui::preferences::PreferencesDraft>,
    board_silent: board_silent::State,
    device_settings: device_settings::Editor,
    control: platform::instance::Control,
    dock: egui_dock::DockState<ui::layout::Panel>,
    reveal_panels: Vec<ui::layout::Panel>,
    close_panels: Vec<ui::layout::Panel>,
    open_panels: Vec<ui::layout::Panel>,
    show_connection: bool,
    show_mapping: bool,
    show_workspaces: bool,
    show_about: bool,
    confirm_reset: bool,
    exiting: bool,
    workspace_name: String,
    workspace_icon: String,
    selected_cell: u8,
    cell_search: String,
    table: ui::cell_table::TableState,
    pixel_diagnostic: pixel_diagnostic::State,
    capture: ui::screenshot::State,
    screenshot_harness: bool,
    focus_cell_search: bool,
    cell_effect: String,
    event_log: diagnostics::EventLog,
    status_request_pending: bool,
    status_retry: diagnostics::Backoff,
    last_status_at: Option<Instant>,
    last_serial_status_at: Option<Instant>,
    presentation: capabilities::Presentation,
    configuration_loaded: bool,
    configuration_reads: configuration_reads::Queue,
    tray: Option<platform::windows_tray::TrayController>,
    notifications: std::cell::RefCell<platform::notifications::Delivery>,
    connection_notice: platform::notifications::ConnectionNotice,
    #[cfg(windows)]
    windows_desktop: Option<platform::windows_desktop::Desktop>,
    base: String,
    user: String,
    password: String,
    access_user: String,
    access_password: String,
    access_confirm: String,
    access_info: Option<serde_json::Value>,
    access_enabled: bool,
    // Public requirement metadata survives Disconnect so a protected peer's
    // credentials can be edited offline. It never grants operational access.
    access_requirement: Option<(String, bool)>,
    serial_port: String,
    transport: Transport,
    status: Option<DeviceStatus>,
    serial_status: Option<DeviceStatus>,
    serial_operations_pending: usize,
    serial_timing: Option<usb_table::Timing>,
    serial_capture: Option<usb_table::CaptureProgress>,
    serial_session: u64,
    serial_stamp: Option<connection::Stamp>,
    brightness: u8,
    pattern: String,
    display_value: i16,
    display_text: String,
    buzzer_frequency: u16,
    buzzer_duration: u16,
    output_mask: u8,
    device_name: Option<String>,
    board_display_name: Option<String>,
    identity_pending: bool,
    identity_saved: Option<(String, String)>,
    message: String,
    http_tx: peer_http::Queue,
    ws_tx: Sender<WsCommand>,
    serial_tx: Sender<SerialCommand>,
    event_rx: Receiver<Event>,
    last_poll: Instant,
    last_ws_endpoint: Option<Endpoint>,
    last_serial_port: String,
    websocket_live: bool,
    serial_live: bool,
    streaming: bool,
    sampling: bool,
    stream_sent_fps: Option<(f64, Instant)>,
    palette: ui::palette::Palette,
    mappings: Vec<CellMapping>,
    mapping_logical: u8,
    mapping_physical: u8,
    mapping_name: String,
    last_mapping_poll: Instant,
    mapping_request_pending: Option<MappingPending>,
    mapping_request_id: u64,
    mapping_peer: Option<peer_http::Pin>,
    mapping_error: Option<String>,
    mapping_retry: diagnostics::Backoff,
}

impl PakarControllerApp {
    fn new(context: &eframe::CreationContext<'_>) -> Self {
        let config = config::AppConfig::load().unwrap_or_else(|error| {
            eprintln!("{error}");
            config::AppConfig::default()
        });
        // apply resolves system/override locale and owns the initial font install.
        config::apply(&context.egui_ctx, &config);
        let dock = ui::layout::restore(&config.dock_layout);
        let screenshot_harness = std::env::var_os("PAKAR_SCREENSHOT_PATH").is_some()
            && std::env::var_os("PAKAR_CONFIG_FILE").is_some() && !config.single_instance;
        #[cfg(windows)]
        if !screenshot_harness && let Err(error) = platform::windows_jump_list::install() {
            eprintln!("Windows quick actions: {error}");
        }
        let (http_tx, http_rx) = mpsc::channel();
        let (ws_tx, ws_rx) = mpsc::channel();
        let (serial_tx, serial_rx) = mpsc::channel();
        let (event_tx, event_rx) = mpsc::channel();
        let connection_gate = connection::Gate::default();
        connection_gate.select(config.connection_enabled.then_some(config.connection_method));
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        let gate = connection_gate.clone();
        thread::Builder::new()
            .name("pakar-http".into())
            .spawn(move || http_worker(http_rx, events, ctx, gate))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        let gate = connection_gate.clone();
        thread::Builder::new()
            .name("pakar-websocket".into())
            .spawn(move || ws_worker(ws_rx, events, ctx, gate))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        let gate = connection_gate.clone();
        thread::Builder::new()
            .name("pakar-serial".into())
            .spawn(move || serial_worker(serial_rx, event_tx, ctx, gate))
            .unwrap();
        let tray = if screenshot_harness {Ok(None)} else {platform::windows_tray::TrayController::new().map(Some)};
        let mut event_log = diagnostics::EventLog::default();
        if let Err(error) = &tray {
            event_log.record("Windows tray unavailable", error);
        }
        #[cfg(windows)]
        let windows_desktop = if screenshot_harness {Ok(None)} else {platform::windows_desktop::Desktop::new(context).map(Some)};
        #[cfg(windows)]
        if let Err(error) = &windows_desktop {
            event_log.record("Windows desktop unavailable", error);
        }
        Self {
            firmware_feed: None,
            firmware_transfer: None,
            firmware_feed_message: String::new(),
            bridge: Default::default(),
            connection_gate,
            connection_enabled: config.connection_enabled,
            active_context: None,
            workspace: Default::default(),
            control: platform::instance::start(context.egui_ctx.clone(), !screenshot_harness),
            dock,
            reveal_panels: Vec::new(),
            close_panels: Vec::new(),
            open_panels: Vec::new(),
            preferences: None,
            board_silent: Default::default(),
            show_connection: false,
            show_mapping: false,
            device_settings: device_settings::Editor::default(),
            show_workspaces: false,
            show_about: false,
            confirm_reset: false,
            exiting: false,
            workspace_name: String::new(),
            workspace_icon: "hardware".into(),
            selected_cell: 1,
            cell_search: String::new(),
            table: Default::default(),
            pixel_diagnostic: Default::default(),
            capture: ui::screenshot::State::from_environment(),
            screenshot_harness,
            focus_cell_search: false,
            cell_effect: "fade".into(),
            event_log,
            status_request_pending: false,
            status_retry: diagnostics::Backoff::new(Duration::from_secs(2), Duration::from_secs(30)),
            last_status_at: None,
            last_serial_status_at: None,
            presentation: Default::default(),
            configuration_loaded: false,
            configuration_reads: Default::default(),
            tray: tray.ok().flatten(),
            notifications: Default::default(),
            connection_notice: Default::default(),
            #[cfg(windows)]
            windows_desktop: windows_desktop.ok().flatten(),
            base: std::env::var("PAKAR_ENDPOINT").unwrap_or_else(|_| config.endpoint.clone()),
            user: std::env::var("PAKAR_USER").unwrap_or_else(|_| config.user.clone()),
            password: std::env::var("PAKAR_PASSWORD").unwrap_or_default(),
            access_user: String::new(),
            access_password: String::new(),
            access_confirm: String::new(),
            access_info: None,
            access_enabled: false,
            access_requirement: None,
            serial_port: std::env::var("PAKAR_SERIAL")
                .unwrap_or_else(|_| config.serial_port.clone()),
            transport: config.connection_method,
            status: None,
            serial_status: None,
            serial_operations_pending: 0,
            serial_timing: None,
            serial_capture: None,
            serial_session: 0,
            serial_stamp: None,
            brightness: 255,
            pattern: "none".into(),
            display_value: 1,
            display_text: "PAKAR CONTROLLER".into(),
            buzzer_frequency: 1032,
            buzzer_duration: 100,
            output_mask: 0,
            device_name: None,
            board_display_name: None,
            identity_pending: false,
            identity_saved: None,
            message: "Connecting to PakarController".into(),
            http_tx: peer_http::Queue::new(http_tx),
            ws_tx,
            serial_tx,
            event_rx,
            last_poll: Instant::now() - Duration::from_secs(2),
            last_ws_endpoint: None,
            last_serial_port: String::new(),
            websocket_live: false,
            serial_live: false,
            streaming: false,
            sampling: false,
            stream_sent_fps: None,
            palette: ui::palette::Palette::Studio,
            mappings: Vec::new(),
            mapping_logical: 1,
            mapping_physical: 73,
            mapping_name: "1".into(),
            last_mapping_poll: Instant::now() - Duration::from_secs(30),
            mapping_request_pending: None,
            mapping_request_id: 0,
            mapping_peer: None,
            mapping_error: None,
            mapping_retry: diagnostics::Backoff::new(Duration::from_secs(2), Duration::from_secs(15)),
            config,
        }
    }
    fn endpoint(&self) -> Endpoint {
        Endpoint {
            base: self.base.clone(),
            user: self.user.clone(),
            password: self.password.clone(),
        }
    }
    fn http_writable(&self) -> bool {
        self.command_route() == Some(Transport::WiFi) && self.http_tx.writable()
            && self.status.as_ref().and_then(peer_http::Pin::from_status).is_some()
    }
    fn revoke_http_drafts(&mut self) {
        self.board_silent.revoke();
        self.status_request_pending = false;
        self.device_settings.invalidate_peer();
        self.configuration_loaded = false;
        self.table.stop_pending_edits();
        self.table.editing_order = false; self.table.manager_open = false;
        self.table.pixel_sample = None;
        self.pixel_diagnostic.revoke();
        self.confirm_reset = false; self.show_mapping = false;
        self.mappings.clear(); self.mapping_name.clear(); self.mapping_logical = 1; self.mapping_physical = 1;
        self.access_info = None; self.access_user.clear(); self.access_password.clear(); self.access_confirm.clear();
        self.device_name = None; self.board_display_name = None;
        self.identity_pending = false; self.identity_saved = None;
        self.streaming = false; self.sampling = false; self.stream_sent_fps = None;
        self.last_mapping_poll = Instant::now() - Duration::from_secs(16);
        self.mapping_request_pending = None; self.mapping_peer = None; self.mapping_error = None; self.mapping_retry.reset();
        let _ = self.ws_tx.send(WsCommand::Mapping(Vec::new()));
        let _ = self.ws_tx.send(WsCommand::Stream(false)); let _ = self.ws_tx.send(WsCommand::Sample(false));
    }
    fn request_board_silent(&mut self, desired: Option<bool>) {
        if self.live_status().is_some_and(board_silent::supported) {
            self.request_board_settings(desired.into());
        }
    }
    fn request_board_settings(&mut self, desired: board_silent::Change) -> Option<u64> {
        let status = self.live_status()?;
        if !desired.allowed(status) { return None; }
        let peer = peer_http::Pin::from_status(status)?;
        let route = self.command_route()?;
        let id = self.board_silent.begin(peer.clone(), route)?;
        let sent = match route {
            Transport::WiFi => self.http_tx.send(HttpCommand::BoardSilent(self.endpoint(), id, desired)).is_ok(),
            Transport::Serial => self.serial_admission().is_some_and(|admission|
                self.serial_tx.send(SerialCommand::BoardSilent(self.serial_port.clone(), admission, id, desired)).is_ok()),
        };
        if !sent { self.board_silent.finish(id, Some(&peer), Some(route), Err("Settings worker unavailable; nothing sent".into())); }
        else if !matches!(desired, board_silent::Change::Read) { self.board_silent.message = "Applying to board, then verifying EEPROM…".into(); }
        sent.then_some(id)
    }
    fn post(&mut self, path: &'static str, form: Vec<(String, String)>) {
        if !self.http_writable() || !self.peer_context().is_some_and(|c| c.allows(path, &form)) {
            self.message = "This connected board has not acknowledged this capability".into();
            return;
        }
        if self.transport != Transport::Serial {
            if self.status.as_ref().is_some_and(|s|realtime::supported(s,path)) {
                if let Err(error)=self.queue_realtime(path,form,None){self.message=error;}
                return;
            }
            let _ = self
                .http_tx
                .send(HttpCommand::Post(self.endpoint(), path, form));
        }
    }
    fn queue_realtime(&self,path:&'static str,form:Vec<(String,String)>,batch:Option<cell_batch::Correlation>)->Result<(),String> {
        let status=self.live_status().ok_or("No fresh board status")?;
        let request=realtime::Intent::new(self.endpoint(),status,path,form,self.http_tx.capture(),
            self.connection_gate.stamp(Transport::WiFi,self.connection_gate.epoch()),batch)?;
        self.ws_tx.send(WsCommand::Control(request)).map_err(|_|"Realtime worker closed; nothing sent".into())
    }
    fn serial_admission(&self)->Option<usb_table::Admission>{
        let stamp=self.serial_stamp.clone()?;
        (self.serial_session!=0 && stamp.current()).then(||usb_table::Admission{
            stamp,session:self.serial_session,peer:self.serial_status.as_ref().and_then(peer_http::Pin::from_status),at:Instant::now(),batch:None})
    }
    fn dispatch_cell_batch(&mut self,batch:cell_batch::Request)->Result<cell_batch::Correlation,String>{
        let route=self.command_route().ok_or("No selected live transport for atomic batch")?;
        let status=self.live_status().ok_or("No current board status for atomic batch")?;
        if !cell_batch::Request::supported(status){return Err("Atomic cell-batch capability is unavailable; nothing queued".into());}
        let peer=peer_http::Pin::from_status(status).ok_or("Atomic batch requires a verified current peer")?;
        let correlation=cell_batch::Correlation::new(batch,peer,route)?;
        match route {
            Transport::WiFi=>{
                if realtime::supported(status,"/api/cells") {
                    self.queue_realtime("/api/cells",correlation.request.form(),Some(correlation.clone()))?;
                } else {self.http_tx.send(HttpCommand::CellBatch(self.endpoint(),correlation.clone()))
                    .map_err(|_|"Atomic batch queue closed; nothing sent".to_string())?;}
            },
            Transport::Serial=>{
                let mut admission=self.serial_admission().ok_or("USB owner/session is not verified")?;
                admission.batch=Some(correlation.clone());
                self.serial_tx.send(SerialCommand::Send(self.serial_port.clone(),admission,cell_batch::OPCODE,correlation.request.payload()))
                    .map_err(|_|"USB batch queue closed; nothing sent".to_string())?;
            }
        }
        Ok(correlation)
    }
    fn serial(&mut self, op: u8, payload: Vec<u8>) {
        if self.connection_enabled && self.command_route() == Some(Transport::Serial) {
            if let Some(admission)=self.serial_admission(){
                let _=self.serial_tx.send(SerialCommand::Send(self.serial_port.clone(),admission,op,payload));
            }else{self.message="USB owner/session is not verified; nothing queued".into();}
        }
    }
    fn action(
        &mut self,
        path: &'static str,
        form: Vec<(String, String)>,
        op: u8,
        payload: Vec<u8>,
    ) {
        if !self.peer_context().is_some_and(|c| c.allows(path, &form)) {
            self.message = "This connected board has not acknowledged this capability".into();
            return;
        }
        match self.command_route() {
            Some(Transport::WiFi) => self.post(path, form),
            Some(Transport::Serial) => self.serial(op, payload),
            _ => self.notify(
                "Device offline",
                "No fresh status from the selected transport. Reconnect before sending a command.",
                messaging::Severity::Warning,
            ),
        }
    }
    fn command_route(&self) -> Option<Transport> {
        if !self.connection_enabled { return None; }
        command_route(
            self.transport,
            self.status.is_some()
                && self.last_ws_endpoint.as_ref() == Some(&self.endpoint())
                && self
                    .last_status_at
                    .is_some_and(|at| at.elapsed() < Duration::from_secs(8)),
            self.serial_live
                && self.last_serial_port == self.serial_port
                && self.serial_status.is_some()
                && self
                    .last_serial_status_at
                    .is_some_and(|at| at.elapsed() < Duration::from_secs(8)),
        )
    }
    fn provision_usb_network(
        &mut self,
        config: &device_settings::Network,
        password: &str,
        open: bool,
    ) -> Result<(), String> {
        if self.command_route() != Some(Transport::Serial) {
            return Err("Reconnect to the selected USB peer before provisioning".into());
        }
        for payload in device_settings::usb_payloads(config, password, open)? {
            self.serial(OP_NETWORK_CONFIG, payload);
        }
        self.message = "USB provisioning queued; this serial protocol has no saved-settings readback. Verify via authenticated HTTP after reconnecting.".into();
        Ok(())
    }
    fn restart_connection(&mut self) {
        self.connection_gate.select(self.connection_enabled.then_some(self.transport));
        self.http_tx.clear();
        self.revoke_http_drafts();
        self.status = None; self.serial_status = None;
        self.last_status_at = None; self.last_serial_status_at = None;
        self.websocket_live = false; self.serial_live = false; self.serial_timing=None;self.serial_capture=None;
        self.status_retry.reset(); self.connection_notice.reset();
        self.last_ws_endpoint = None; self.last_serial_port.clear();
        self.event_log.reset_sources("HTTP");
        self.event_log.reset_sources("WebSocket");
        self.event_log.reset_sources("USB");
        let _ = self.ws_tx.send(WsCommand::Disconnect);
        let _ = self.serial_tx.send(SerialCommand::Port(String::new(), self.connection_gate.epoch()));
        self.last_poll = Instant::now() - Duration::from_secs(30);
    }
    pub(crate) fn set_connection(&mut self, method: Transport, enabled: bool) {
        if self.connection_change_guard() { return; }
        if !enabled {self.presentation.update(None,None);}
        if enabled && method == Transport::Serial && self.serial_port.trim().is_empty() {
            self.message = "Select a USB serial port before connecting".into(); return;
        }
        self.transport = method;
        self.connection_enabled = enabled;
        self.config.connection_method = method;
        self.config.connection_enabled = enabled;
        self.config.endpoint = self.base.clone();
        self.config.user = self.user.clone();
        self.config.serial_port = self.serial_port.clone();
        self.restart_connection();
        self.save_workspace();
        self.message = if enabled {format!("Connecting through {}", method.label())}
            else {format!("{} disconnected", method.label())};
    }
    fn update_esp_from_file(&mut self, path: &str) -> Result<(), String> {
        if !self.http_writable() || !self.has_feature("ota") {
            return Err("Connect to the selected Wi-Fi board with OTA acknowledged first".into());
        }
        if self.connection_change_guard() {
            return Err("Save/discard drafts and finish pending operations before OTA".into());
        }
        let path = std::path::Path::new(path);
        if !path.is_absolute() { return Err("Select an absolute firmware file path".into()); }
        let bytes = maintenance::read_image_file(path, true)?;
        let operation=self.source_operation(firmware_source::Source::Local(bytes),true)?;
        if let maintenance::Operation::SourceImage{progress,..}=&operation {self.firmware_transfer=Some(progress.clone());}
        let id = self.device_settings.next_id();
        self.http_tx.send(HttpCommand::Maintenance(self.endpoint(), id, operation))
            .map_err(|_|"Firmware request queue closed")?;
        self.device_settings.maintenance_pending = Some(id);
        self.device_settings.message = "ESP Wi-Fi update queued once; do not disconnect its power".into();
        Ok(())
    }
    fn poll(&mut self) {
        let endpoint = self.endpoint();
        if self.connection_enabled && self.transport == Transport::WiFi && self.last_ws_endpoint.as_ref() != Some(&endpoint) {
            self.restart_connection();
            self.http_tx.clear();
            self.revoke_http_drafts();
            self.event_log.reset_sources("HTTP");
            self.event_log.reset_sources("WebSocket");
            self.status_request_pending = false;
            self.status_retry.reset();
            self.connection_notice.reset();
            self.device_settings.invalidate_peer();
            self.status = None;
            self.table.pixel_sample = None;
            self.last_status_at = None;
            self.websocket_live = false;
            self.stream_sent_fps = None;
            self.streaming = false;
            self.sampling = false;
            self.mappings.clear();
            self.access_info = None;
            self.device_name = None;
            self.board_display_name = None;
            self.identity_pending = false;
            self.identity_saved = None;
            let _ = self.ws_tx.send(WsCommand::Connect(endpoint.clone(), self.connection_gate.epoch()));
            let _ = self.ws_tx.send(WsCommand::Stream(false));
            let _ = self.ws_tx.send(WsCommand::Sample(false));
            let _ = self.ws_tx.send(WsCommand::Rate(self.config.stream_fps));
            self.last_ws_endpoint = Some(endpoint);
            self.configuration_loaded = false;
        }
        if self.connection_enabled && self.transport == Transport::Serial && self.last_serial_port != self.serial_port {
            self.restart_connection();
            self.event_log.reset_sources("USB");
            self.identity_pending = false;
            self.connection_notice.reset();
            self.serial_status = None;
            self.last_serial_status_at = None;
            self.serial_live = false;
            let _ = self
                .serial_tx
                .send(SerialCommand::Port(self.serial_port.clone(), self.connection_gate.epoch()));
            self.last_serial_port = self.serial_port.clone();
        }
        if self.connection_enabled && self.transport == Transport::WiFi && !self.status_request_pending
            && !(self.websocket_live && self.last_status_at.is_some_and(|at|at.elapsed()<Duration::from_secs(2)))
            && self.last_poll.elapsed() >= self.status_retry.delay() {
            self.last_poll = Instant::now();
            self.status_request_pending = self.http_tx.send(HttpCommand::Status(self.endpoint())).is_ok();
            // Firmware progress already arrives in this same status/WS stream,
            // including external OTA. Do not queue a redundant job poll.
        }
        // A retired/expired queue entry may never produce an event. Its request
        // token must expire independently, and a late result cannot settle a
        // newer request. Preserve confirmed same-peer mapping while retrying.
        if self.mapping_request_pending.as_ref().is_some_and(|pending| pending.started.elapsed() >= Duration::from_secs(24)) {
            self.mapping_request_pending = None;
            self.mapping_retry.failed();
            self.last_mapping_poll = Instant::now();
            self.mapping_error = Some("Mapping read timed out; retrying without changing the confirmed mapping".into());
        }
        let mapping_interval = if self.mapping_error.is_some() || self.mappings.is_empty() { self.mapping_retry.delay() } else { Duration::from_secs(15) };
        if self.command_route() == Some(Transport::WiFi) && self.has_table() && self.mapping_request_pending.is_none()
            && self.last_mapping_poll.elapsed() >= mapping_interval
            && let Some(peer) = self.status.as_ref().and_then(peer_http::Pin::from_status) {
            self.last_mapping_poll = Instant::now();
            self.mapping_request_id = self.mapping_request_id.wrapping_add(1);
            let id = self.mapping_request_id;
            if self.http_tx.send(HttpCommand::Mapping(self.endpoint(), id)).is_ok() {
                self.mapping_request_pending = Some(MappingPending { id, peer, started: Instant::now() });
            } else {
                self.mapping_retry.failed();
                self.mapping_error = Some("Mapping request queue closed; reconnect to retry".into());
            }
        }
        // Load mapping before optional settings: the four settings preflights
        // used to queue ahead of the small mapping read on first connection.
        // Never flood an offline/bootstrap queue with settings preflights.
        // Read-only legacy peers can still load after a fresh Wi-Fi status.
        if !self.configuration_loaded {
            self.configuration_reads.reset();
            self.configuration_loaded = true;
        }
        // Auth is already probed by the HTTP worker. Optional settings must not
        // queue four status-fenced transactions ahead of interactive controls.
        // Keep one read outstanding, and require the table's initial mapping first.
        if self.command_route() == Some(Transport::WiFi)
            && (!self.has_table() || self.mapping_peer == self.live_status().and_then(peer_http::Pin::from_status))
            && (self.device_settings.open || self.show_connection || self.dock.find_tab(&ui::layout::Panel::Network).is_some())
            && let Some(path) = self.configuration_reads.take_due() {
            if self.http_tx.send(HttpCommand::Configuration(self.endpoint(), path)).is_err() {
                self.configuration_reads.finish(path, false);
            }
        }
        self.pixel_diagnostic.expire();
        while let Ok(event) = self.event_rx.try_recv() {
            let event_stamp=match &event {Event::ForConnection(stamp,_) if stamp.current()=>Some(stamp.clone()),_=>None};
            if let Some(receipt) = peer_http::retired_envelope(&event) {
                self.record_event("Previous board session", &receipt);
                continue;
            }
            let event = match event {
                Event::ForConnection(stamp, inner) if stamp.current() => *inner,
                Event::ForConnection(_, inner) => {
                    if let Some(receipt) = peer_http::retired_envelope(&inner).or_else(||peer_http::retired_receipt(&inner)) {
                        self.record_event("Previous connection", &receipt);
                    }
                    continue;
                }
                other => other,
            };
            let Some(event) = current_event(event, &self.endpoint(), &self.serial_port) else {
                continue;
            };
            let event = match event {
                Event::ForScope(scope, inner) if scope.current() => *inner,
                Event::ForScope(_, inner) => {
                    if let Some(receipt) = peer_http::retired_receipt(&inner) { self.record_event("Previous board session", &receipt); }
                    continue;
                }
                other => other,
            };
            match event {
                Event::ForConnection(..) => {}
                Event::ForScope(..) => {} // Scope envelopes are unwrapped once.
                Event::ForEndpoint(..) => {} // Worker envelopes are unwrapped once.
                Event::StreamStats(value) => self.stream_sent_fps = Some((value, Instant::now())),
                Event::StatusRgb(rgb)=>{
                    if self.connection_enabled&&self.transport==Transport::WiFi&&status_observation_fresh(self.last_status_at){
                        if let Some(status)=self.status.as_mut(){status_rgb::observe(status,rgb);}
                    } // Never renew full status, capability, command-clock or workspace authority.
                }
                Event::UpgradeProgress(upgrade)=>{
                    if self.connection_enabled && self.transport==Transport::WiFi {
                        if let Some(status)=self.status.as_mut(){firmware_progress::observe_upgrade(status,upgrade);}
                    } // Progress alone never renews command/status authority.
                }
                Event::Status(mut s) => {
                    if realtime::older_than(&s,self.status.as_ref()) { continue; }
                    firmware_progress::retain_upgrade(&mut s,self.status.as_ref());
                    status_rgb::retain(&mut s,self.status.as_ref());
                    if !s.hostname.is_empty() {
                        if self.status.as_ref().is_some_and(|old|old.cells!=s.cells || old.effect!=s.effect ||
                            old.mode!=s.mode || old.brightness!=s.brightness || old.led_output["enabled"]!=s.led_output["enabled"]) {
                            self.table.pixel_sample=None; // Old RGB must not mask a new acknowledged semantic state.
                        }
                        if self.http_tx.observe(&s) {
                            // A resumed lease on this exact board/boot is not a
                            // new workspace. Queue generations still retire all
                            // old operations; only a genuine peer change may
                            // discard selection, text and settings drafts.
                            if self.status.as_ref().and_then(peer_http::Pin::from_status) != peer_http::Pin::from_status(&s) {
                                self.revoke_http_drafts();
                            }
                        }
                        self.device_settings.remote_job=maintenance::parse_job(s.remote_firmware.clone()).ok();
                        if s.pixel_preview_requested {
                            ui::cell_pixels::Sample::observe(&mut self.table.pixel_sample, s.pixel_frame.clone());
                        }
                        self.brightness = s.brightness;
                        if !pixel_diagnostic::ready(&s) { self.pixel_diagnostic.armed = None; }
                        self.pattern = s.effect.clone();
                        self.output_mask = s.active_outputs;
                        self.status = Some(s);
                        self.last_status_at = Some(Instant::now());
                        self.message.clear();
                    }
                }
                Event::Mapping(id, mappings, _) => {
                    let current_peer = self.status.as_ref().and_then(peer_http::Pin::from_status);
                    if !self.mapping_request_pending.as_ref().is_some_and(|pending| pending.id == id && Some(&pending.peer) == current_peer.as_ref()) {
                        continue;
                    }
                    self.mapping_request_pending = None; self.mapping_retry.reset();
                    self.mapping_peer = current_peer;
                    self.mapping_error = None;
                    self.last_mapping_poll = Instant::now();
                    if self.table.pixel_sample.as_ref().is_some_and(|sample|
                        mappings.iter().any(|m| sample.frame.physical.get(usize::from(m.logical_id - 1)) != Some(&m.physical_cell))) {
                        self.table.pixel_sample = None;
                    }
                    self.mappings = mappings;
                    let _ = self.ws_tx.send(WsCommand::Mapping(self.mappings.clone()));
                    if !streaming::valid_mapping(&self.mappings) {
                        self.sampling = false;
                    }
                }
                Event::MappingFailed(id, error) => {
                    if !self.mapping_request_pending.as_ref().is_some_and(|pending| pending.id == id) { continue; }
                    self.mapping_request_pending = None;
                    self.mapping_retry.failed(); self.last_mapping_poll = Instant::now();
                    self.mapping_error = Some(error.clone());
                    self.event_log.failure("HTTP GET /api/cells/mapping", &error);
                }
                Event::Configuration("/api/auth/info", value) => {
                    self.access_enabled = value.get("enabled").and_then(|v|v.as_bool()) == Some(true);
                    self.access_requirement = value.get("enabled").and_then(|v|v.as_bool())
                        .map(|enabled| (self.base.clone(), enabled));
                    self.access_user = value
                        .get("username")
                        .and_then(|v| v.as_str())
                        .unwrap_or_default()
                        .into();
                    self.access_info = Some(value);
                }
                Event::Configuration(path, value) => {
                    self.configuration_reads.finish(path, true);
                    if path != "/api/device/name" || self.command_route() != Some(Transport::Serial) {
                        self.receive_configuration(path, value);
                    }
                }
                Event::MaintenanceFinished(id, result) => {
                    if self.device_settings.maintenance_pending == Some(id) {
                        if let Some(progress)=self.firmware_transfer.as_ref() {progress.finish(&result);}
                        let severity = maintenance::result_severity(&result);
                        self.device_settings.maintenance_pending = None;
                        self.device_settings.message = result.unwrap_or_else(|e| e);
                        self.record_event("Maintenance", &self.device_settings.message.clone());
                        self.notify_id(
                            "pakar.maintenance",
                            "Maintenance result",
                            &self.device_settings.message,
                            severity,
                        );
                        self.configuration_loaded = false;
                    }
                }
                Event::SettingsFinished(id, group, result) => {
                    if self.device_settings.pending != Some((id, group)) {
                        continue;
                    }
                    let severity = if result.is_ok() {
                        messaging::Severity::Success
                    } else {
                        messaging::Severity::Error
                    };
                    self.device_settings.finish(id, group, result);
                    self.record_event("Settings", &self.device_settings.message.clone());
                    self.notify_id(
                        "pakar.settings",
                        "Device settings result",
                        &self.device_settings.message,
                        severity,
                    );
                }
                Event::CredentialsUpdated(endpoint) => {
                    self.access_requirement = Some((endpoint.base.clone(), !endpoint.password.is_empty()));
                    self.user = endpoint.user;
                    self.password = endpoint.password;
                    self.config.user = self.user.clone();
                    self.access_password.clear();
                    self.access_confirm.clear();
                    self.configuration_loaded = false;
                    self.last_ws_endpoint = None;
                    self.notify("Credentials saved", "All device sessions were signed out. This client will reconnect with the new credentials.", messaging::Severity::Success);
                }
                Event::NameUpdated(endpoint, pending) => {
                    self.identity_pending = false;
                    self.identity_saved = None;
                    self.base = endpoint.base;
                    self.config.endpoint = self.base.clone();
                    self.configuration_loaded = false;
                    self.last_ws_endpoint = None;
                    self.notify("Board identity saved", &if pending {format!("Reconnect at {} after Wi-Fi reassociates. DNS caches may need time to expire.", self.base)} else {"Board name verified. Wi-Fi was not restarted.".into()}, messaging::Severity::Success);
                }
                Event::SerialIdentity(port, value) => {
                    if port == self.serial_port && self.command_route() == Some(Transport::Serial) {
                        self.receive_configuration("/api/device/name",value);
                    }
                }
                Event::SerialSnapshot(_, mut snapshot) => {
                    firmware_progress::retain_upgrade(&mut snapshot.status,self.serial_status.as_ref());
                    self.device_settings.remote_job=maintenance::parse_job(snapshot.status.remote_firmware.clone()).ok();
                    let peer=peer_http::Pin::from_status(&snapshot.status);
                    let previous=self.serial_status.as_ref().and_then(peer_http::Pin::from_status);
                    if peer!=previous {
                        self.board_silent.revoke();
                        self.table.stop_pending_edits();
                        // Revoked wire ownership/boot must cancel operations,
                        // not discard this same physical board's user workspace.
                        // The presentation workspace owns selection/find across
                        // sessions; fresh peer admission below never replays it.
                        self.table.pixel_sample=None;self.pixel_diagnostic.revoke();
                        self.show_mapping=false;self.mapping_request_pending=None;
                    }
                    self.mappings=snapshot.mappings;self.mapping_peer=peer;self.mapping_error=None;
                    self.table.pixel_sample=snapshot.status.pixel_frame.as_ref().filter(|frame|frame.valid())
                        .map(|frame|ui::cell_pixels::Sample{frame:frame.clone(),at:snapshot.observed_at});
                    self.pattern=snapshot.status.effect.clone();
                    self.serial_status=Some(snapshot.status);self.last_serial_status_at=Some(snapshot.observed_at);self.serial_live=true;
                    if matches!(self.message.as_str(),"Connecting through USB"|"Connecting to PakarController") {
                        self.message="USB connected · 80 cells confirmed".into();
                    }
                    self.event_log.recover(&format!("USB {}",self.serial_port));
                }
                Event::CellBatchConfirmed(correlation) | Event::SerialBatchConfirmed(_,correlation) => {
                    if self.command_route()==Some(correlation.route) && self.live_status().and_then(peer_http::Pin::from_status).as_ref()==Some(&correlation.peer) {
                        self.table.confirm_batch_ack(&correlation);
                        self.record_event("Atomic cell batch",&format!("Acknowledged and same-board readback verified for {} cells",correlation.request.ids.len()));
                    }else{self.record_event("Previous board session","Atomic batch result belongs to another peer/route; no current UI batch completed");}
                }
                Event::SerialOperation(_, result) => {
                    match result {
                        Ok(message)=>{self.message=message.clone();self.record_event("USB command",&message);}
                        Err(error)=>{self.message=error.clone();self.record_event("USB command unverified",&error);
                            self.table.fail_pending_edits(error);}
                    }
                }
                Event::SerialCommandState(_, mut status) => {
                    firmware_progress::retain_upgrade(&mut status,self.serial_status.as_ref());
                    if peer_http::Pin::from_status(&status)==self.serial_status.as_ref().and_then(peer_http::Pin::from_status) {
                        self.pattern=status.effect.clone();self.serial_status=Some(status);
                        self.table.pixel_sample=None;
                    }
                }
                Event::SerialTiming(_, timing) => self.serial_timing=Some(timing),
                Event::SerialCapture(_, progress) => self.serial_capture=Some(progress),
                Event::SerialQueueDepth(_, depth) => self.serial_operations_pending=depth,
                Event::SerialOwner(_, session) => {
                    self.board_silent.revoke();
                    self.serial_session=session;self.serial_stamp=event_stamp.clone();
                    self.serial_status=None;self.last_serial_status_at=None;self.serial_operations_pending=0;self.serial_timing=None;
                    self.mappings.clear();self.mapping_peer=None;self.table.pixel_sample=None;self.table.stop_pending_edits();
                }
                Event::BoardSilentFinished(id, result) => {
                    let peer = self.live_status().and_then(peer_http::Pin::from_status);
                    let route = self.command_route();
                    self.device_settings.finish_avr(id, peer.as_ref(), route, &result);
                    self.board_silent.finish(id, peer.as_ref(), route, result);
                }
                Event::SerialIdentitySaved(port, result) => {
                    if port == self.serial_port {
                        self.identity_pending = false;
                        if result.is_ok() { self.identity_saved = None; }
                        match result {
                            Ok(()) => self.notify("Board identity saved", "USB ACK and identity readback verified.", messaging::Severity::Success),
                            Err(error) => self.notify("Board identity unverified", &error, messaging::Severity::Warning),
                        }
                    }
                }
                Event::PixelModeFinished(id, result) => {
                    if self.pixel_diagnostic.matches(id) {
                        match result {
                            Ok(status) if peer_http::Pin::from_status(&status) == self.live_status().and_then(peer_http::Pin::from_status) => {
                                self.pixel_diagnostic.armed = peer_http::Pin::from_status(&status);
                                self.status = Some(status); self.last_status_at = Some(Instant::now());
                                self.pixel_diagnostic.finish(id, "Effects mode acknowledged and read back. Select a physical pixel, then Show/Previous/Next.".into());
                            }
                            Ok(_) => self.pixel_diagnostic.finish(id, "Mode result belongs to another board; no frame sent".into()),
                            Err(error) => self.pixel_diagnostic.finish(id, error),
                        }
                    }
                }
                Event::PixelSent(request, result) => {
                    if self.pixel_diagnostic.matches(request.id) {
                        match result {
                            Ok(()) if Some(request.peer.clone()) == self.live_status().and_then(peer_http::Pin::from_status) => {
                                self.pixel_diagnostic.message = format!("Pixel {} frame written once; checking RGB readback…", request.index + 1);
                                if self.http_tx.send(HttpCommand::PixelReadback(self.endpoint(), request.id, request.index, request.peer)).is_err() {
                                    self.pixel_diagnostic.finish(request.id, "Frame written; pixel readback unavailable. No replay.".into());
                                }
                            }
                            Ok(()) => self.pixel_diagnostic.finish(request.id, "Frame write completed for a retired peer; inspect it before retrying".into()),
                            Err(error) => self.pixel_diagnostic.finish(request.id, error),
                        }
                    }
                }
                Event::PixelReadbackFinished(id, result) => {
                    self.pixel_diagnostic.finish(id, result.unwrap_or_else(|error|
                        format!("Frame written; pixel readback unavailable: {error}. Nothing replayed.")));
                }
                Event::Accepted(m) => {
                    self.record_event("Command", &m);
                    self.notify("Command accepted", &m, messaging::Severity::Success);
                }
                Event::OperationFailed(error) => {
                    self.identity_pending = false;
                    self.table.fail_pending_edits(error.clone());
                    self.record_event("Operation failed", &error);
                    self.notify_id(
                        "pakar.operation-failed",
                        "Operation failed",
                        &error,
                        messaging::Severity::Error,
                    );
                    self.message = error;
                }
                Event::DiagnosticFailure(source, e) => {
                    self.configuration_reads.failed_source(&source);
                    if source == "HTTP GET /api/status" {
                        // A recoverable HTTP read failure is not a peer change.
                        // Fresh WS/HTTP observations retain the same bound view;
                        // writes still require fresh preflight and pinned headers.
                        if self.http_tx.writable() && !status_observation_fresh(self.last_status_at) {
                            self.http_tx.clear();
                            self.table.stop_pending_edits();
                            self.pixel_diagnostic.revoke();
                            self.streaming = false; self.sampling = false;
                            let _ = self.ws_tx.send(WsCommand::Stream(false));
                            let _ = self.ws_tx.send(WsCommand::Sample(false));
                            // Retain the old observation only as peer ownership
                            // context. last_status_at stays expired, so neither
                            // live_status nor command_route grants authority.
                            self.last_status_at = None;
                        }
                        self.status_request_pending = false;
                        self.status_retry.failed();
                        self.last_poll = Instant::now();
                    }
                    if self.device_settings.scan_pending {
                        self.device_settings.scan_pending = false;
                        self.device_settings.message =
                            "Network scan did not complete. Reconnect and scan again.".into();
                    }
                    self.event_log.failure(&source, &e);
                    self.message = e;
                }
                Event::DiagnosticRecovered(source) => {
                    if source == "HTTP GET /api/status" {
                        self.status_request_pending = false;
                        self.status_retry.reset();
                    }
                    self.event_log.recover(&source);
                }
                Event::SerialFailure(port, error) => self.event_log.failure(&format!("USB {port}"), &error),
                Event::WebSocket(v) => {
                    if self.websocket_live && !v { self.record_event("Connection", "WebSocket connection lost; reconnecting with bounded backoff."); }
                    self.websocket_live = v;
                    if !v {
                        self.serial_session=0;self.serial_stamp=None;
                        self.serial_operations_pending=0;self.serial_timing=None;
                        self.pixel_diagnostic.revoke();
                        self.stream_sent_fps = None;
                        self.streaming = false;
                        self.sampling = false;
                    }
                }
                Event::Serial(_, v, s) => {
                    if self.serial_live && !v { self.record_event("Connection", "USB connection lost; reconnecting."); }
                    if v { self.event_log.recover(&format!("USB {}", self.serial_port)); }
                    self.serial_live = v;
                    if !v {
                        self.serial_status = None;
                        self.last_serial_status_at = None;
                        if self.transport==Transport::Serial {
                            self.mappings.clear();self.mapping_peer=None;self.table.pixel_sample=None;
                            self.table.stop_pending_edits();
                        }
                    }
                    if let Some(s) = s {
                        if self.serial_status.as_ref().and_then(peer_http::Pin::from_status)!=peer_http::Pin::from_status(&s){
                            self.mappings.clear();self.mapping_peer=None;self.table.pixel_sample=None;
                            self.table.stop_pending_edits();self.pixel_diagnostic.revoke();
                        }
                        self.serial_status = Some(s);
                        self.last_serial_status_at = Some(Instant::now());
                    }
                }
            }
        }
        self.poll_firmware_feed();
        let state = self.live_status().map(|s| s.avr_online);
        if let Some((message, severity)) = self.connection_notice.observe(state, Instant::now()) {
            self.record_event("Connection", message);
            self.notify_id("pakar.connection", "Connection changed", message, severity);
        }
        let tip = match self.live_status() {
            Some(s) => format!(
                "{} — AVR {}",
                s.capabilities.as_ref().map(|c|c.title()).unwrap_or_else(||"PakarController".into()),
                if s.avr_online { "online" } else { "offline" }
            ),
            None => "PakarController — device offline".into(),
        };
        if let Some(tray) = &mut self.tray {
            match tray.refresh_tooltip(&tip, Instant::now()) {
                Ok(true) => self.event_log.recover("Windows tray tooltip"),
                Ok(false) => {},
                Err(error) => self.event_log.failure("Windows tray tooltip", &error),
            }
        }
    }
    fn deliver_notifications(&mut self) {
        loop {
            let request = self
                .notifications
                .borrow_mut()
                .next(&self.config.notifications, messaging::now_ms());
            let Some(request) = request else {
                break;
            };
            let result = self
                .tray
                .as_ref()
                .ok_or_else(|| "Native notification unavailable: tray registration failed".into())
                .and_then(|tray| tray.notify(&request, self.config.notifications.sound));
            if let Err(error) = &result {
                self.record_event("Windows notification", error);
            }
            self.notifications.borrow_mut().result(result);
        }
    }
    fn notification_snapshot(&self) -> serde_json::Value {
        let delivery = self.notifications.borrow();
        serde_json::json!({"backend":"Windows Shell", "policy":self.config.notifications.system, "sound":self.config.notifications.sound, "in_app":self.config.notifications.in_app, "accepted":delivery.accepted, "suppressed":delivery.suppressed, "failed":delivery.failed, "last_result":delivery.last_result, "shell_events":self.tray.as_ref().map(|tray| tray.notification_snapshot())})
    }
    #[cfg(test)]
    fn next_cell_state(s: &str) -> (&'static str, u8) {
        match s {
            "white" => ("red", 1),
            "red" => ("off", 2),
            _ => ("white", 0),
        }
    }
}

impl eframe::App for PakarControllerApp {
    fn logic(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        // Adapted from Pealayer: eframe runs logic while minimized/occluded.
        // IPC/tray, device results and status must not depend on painting.
        self.capture.receive(ctx);
        self.capture.tick(ctx);
        self.native_logic(ctx);
    }
    fn ui(&mut self, ui: &mut egui::Ui, _: &mut eframe::Frame) {
        self.draw_shell(ui);
        self.capture.after_frame(ui.ctx());
    }
    fn save(&mut self, _: &mut dyn eframe::Storage) {
        self.save_workspace();
    }
}

fn environment_endpoint() -> Endpoint {
    let saved = config::AppConfig::load().unwrap_or_default();
    Endpoint {
        base: std::env::var("PAKAR_ENDPOINT").unwrap_or(saved.endpoint),
        user: std::env::var("PAKAR_USER").unwrap_or(saved.user),
        password: std::env::var("PAKAR_PASSWORD").unwrap_or_default(),
    }
}

fn blocking_status(client: &Client, endpoint: &Endpoint) -> Result<DeviceStatus, String> {
    let response = client
        .get(format!(
            "{}/api/status",
            endpoint.base.trim_end_matches('/')
        ))
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .send()
        .map_err(|error|{
            // A body/status timeout does not prove that cached DNS is wrong.
            // Only a dial/resolution failure asks a later read to re-resolve;
            // the previous verified hint is retained until replacement.
            if error.is_connect(){peer_http::request_relookup(&endpoint.base);}
            diagnostics::describe_error(error)
        })?;
    if !response.status().is_success() {
        return Err(format!(
            "HTTP {}: {}",
            response.status(),
            response.text().unwrap_or_default()
        ));
    }
    let remote=response.remote_addr();
    let status=response.json().map_err(diagnostics::describe_error)?;
    peer_http::remember_address(&endpoint.base,&status,remote);
    Ok(status)
}

fn format_text_status(status: &DeviceStatus, table: bool) -> String {
    let mut output = String::new();
    macro_rules! println { ($($arg:tt)*) => {{ use std::fmt::Write as _; let _ = writeln!(output, $($arg)*); }}; }
    macro_rules! print { ($($arg:tt)*) => {{ use std::fmt::Write as _; let _ = write!(output, $($arg)*); }}; }
    println!(
        "{} {} | AVR {} | TM1637 {} | {} | MQTT {} | NTP {} | brightness {} | effect {} | LED output {}",
        status.hostname,
        status.station_ip,
        if status.avr_online {
            "online"
        } else {
            "offline"
        },
        if status.tm1637_ack {
            "ready"
        } else {
            "missing"
        },
        status.mode,
        if status.mqtt_connected {
            "connected"
        } else {
            "offline"
        },
        if status.ntp_synced {
            "synced"
        } else {
            "waiting"
        },
        status.brightness,
        status.effect,
        match status.led_output.get("enabled").and_then(|v| v.as_bool()) {
            Some(true) => "On", Some(false) => "Off", None => "Unknown",
        }
    );
    println!(
        "inputs {:02X}+{} outputs {:02X} winner {} link {} UART {}/{}/{}",
        status.active_inputs,
        u8::from(status.input_9),
        status.active_outputs,
        status.winner,
        status.link_mode,
        status.esp_uart_errors,
        status.avr_uart_errors,
        status.usb_serial_errors
    );
    if !table || !status.capabilities.as_ref().is_some_and(|m| m.table()) {
        return output;
    }
    for row in 0..8_i32 {
        for column in 0..10_i32 {
            let id = row * 10 + column + 1;
            let mark = match status.cells.get((id - 1) as usize).map(String::as_str) {
                Some("white") => 'W',
                Some("red") => 'R',
                _ => '.',
            };
            print!("{id:02}{mark}  ");
        }
        println!();
    }
    output
}

fn text_command(client: &Client, endpoint: &Endpoint, words: &[&str]) -> Result<(), String> {
    let mut output = String::new();
    text_command_output(client, endpoint, words, &mut output)?;
    print!("{output}");
    Ok(())
}

// One command parser for CLI and the TUI command palette. Capturing output must
// never replace a peer operation with a second invocation or stdout redirection.
fn text_command_output(client: &Client, endpoint: &Endpoint, words: &[&str], output: &mut String) -> Result<(), String> {
    macro_rules! println {
        ($($arg:tt)*) => {{
            use std::fmt::Write as _;
            writeln!(output, $($arg)*).map_err(|e| e.to_string())?;
        }};
    }
    if let ["cells",ids,state,rest @ ..]=words {
        let effect=match rest {[]=>"immediate",[effect]=>effect,_=>return Err("cells IDS white|red|off [immediate|fade|pulse]".into())};
        let request=cell_batch::Request::new(&ui::cell_search::parse(ids)?,state,effect)?;
        println!("{}",cell_batch::send(client,endpoint,&request,None,None,None)?);
        return Ok(());
    }
    if words.first() == Some(&"firmware") {
        use maintenance::Operation;
        let operation=match words {
            ["firmware","status"]=>Operation::RemoteStatus,
            ["firmware","fetch",url,sha] | ["firmware","fetch",url,sha,"all"]=>Operation::RemoteStage{url:(*url).into(),sha256:(*sha).into(),targets:3,bearer:std::env::var("PAKAR_FIRMWARE_TOKEN").unwrap_or_default()},
            ["firmware","fetch",url,sha,target]=>Operation::RemoteStage{url:(*url).into(),sha256:(*sha).into(),targets:match *target{"esp"=>1,"avr"=>2,_=>return Err("Target must be all, esp or avr".into())},bearer:std::env::var("PAKAR_FIRMWARE_TOKEN").unwrap_or_default()},
            ["firmware",action,id,boot,sha] if *action=="apply" || *action=="cancel"=>{
                // Read-only preflight. Never fetch replacement identity for an
                // unpinned mutation; the caller must supply the advertised triple.
                let current=maintenance::run(client,endpoint,Operation::RemoteStatus)?;
                let job=maintenance::parse_job(serde_json::from_str(&current).map_err(|_|"Invalid job JSON")?)?;
                if job.job_id!=id.parse::<u32>().map_err(|_|"Invalid job ID")? || job.boot_id!=*boot || job.manifest_sha256!=*sha {
                    return Err("Stale or incorrect remote job identity; nothing applied".into());
                }
                if *action=="apply"{Operation::RemoteApply(job)}else{Operation::RemoteCancel(job)}
            }
            _=>return Err("firmware status | fetch HTTPS_URL MANIFEST_SHA256 [all|esp|avr] | apply JOB BOOT SHA256 | cancel JOB BOOT SHA256".into()),
        };
        println!("{}", maintenance::run(client, endpoint, operation)?);
        return Ok(());
    }
    if words == ["info"] || words == ["telemetry"] {
        let value = pc_contract::get(
            client,
            &endpoint.base,
            &endpoint.user,
            &endpoint.password,
            "/api/telemetry",
        )?;
        println!(
            "{}",
            serde_json::to_string_pretty(&value).map_err(|e| e.to_string())?
        );
        return Ok(());
    }
    if words.first() == Some(&"avr") {
        let fetch = |op, payload: &[u8]| {
            pc_contract::request(
                client,
                &endpoint.base,
                &endpoint.user,
                &endpoint.password,
                op,
                payload,
            )
        };
        let value = match words {
            ["avr","contracts"] => pc_contract::get(client,&endpoint.base,&endpoint.user,&endpoint.password,"/api/avr/contracts")?,
            ["avr","settings"] => {
                let receipt=fetch(4,&[])?;
                pc_contract::Settings::parse(receipt["payload_hex"].as_str().ok_or("Missing settings payload")?)?.json()
            }
            ["avr","set",key,input] => {
                let current=fetch(4,&[])?;
                let mut next=pc_contract::Settings::parse(current["payload_hex"].as_str().ok_or("Missing settings payload")?)?;
                next.set(key,input)?;
                let saved=fetch(5,&next.bytes)?;
                if saved["response_opcode"].as_u64()!=Some(0x80) ||
                    saved["payload_hex"].as_str()!=Some("0500") {
                    return Err("AVR did not acknowledge settings; operation outcome is unknown".into());
                }
                let readback=fetch(4,&[])?;
                let verified=pc_contract::Settings::parse(readback["payload_hex"].as_str().ok_or("Missing settings readback")?)?;
                if !verified.persisted || verified.bytes!=next.bytes {
                    return Err("AVR settings readback did not confirm the requested persistent values".into());
                }
                verified.json()
            }
            ["avr","status"] => fetch(2,&[])?,
            ["avr","panel"] => fetch(0x3c,&[])?,
            ["avr","command",op] => fetch(pc_contract::opcode(op)?,&[])?,
            ["avr","command",op,payload] => fetch(pc_contract::opcode(op)?,&pc_contract::decode_hex(payload)?)?,
            _ => return Err("avr contracts | settings | set PARAMETER VALUE | status | panel | command OPCODE [HEX]".into())
        };
        println!(
            "{}",
            serde_json::to_string_pretty(&value).map_err(|e| e.to_string())?
        );
        return Ok(());
    }
    if words == ["device", "name"] || words == ["device", "identity"] {
        if let Event::Configuration(_, value) = http_request(
            client,
            HttpCommand::Configuration(endpoint.clone(), "/api/device/name"),
        )? {
            println!(
                "{}",
                serde_json::to_string_pretty(&value).map_err(|e| e.to_string())?
            );
        }
        return Ok(());
    }
    if let ["device", "label", rest @ ..] = words {
        let label = rest.join(" ");
        if !identity::valid_display_name(&label) { return Err("Board name must be 1..63 UTF-8 bytes without controls or edge spaces".into()); }
        let Event::Configuration(_, value) = http_request(client,HttpCommand::Configuration(endpoint.clone(),"/api/device/name"))? else { return Err("Identity unavailable".into()) };
        let host = value["hostname"].as_str().ok_or("Hostname unavailable")?;
        let _ = http_request(client,HttpCommand::NameChange(endpoint.clone(),host.into(),Some(label)))?;
        println!("Board display name saved and verified; Wi-Fi was not restarted.");
        return Ok(());
    }
    if let ["device", "configure", host, rest @ ..] = words {
        let label = rest.join(" ");
        if !identity::valid_display_name(&label) { return Err("Invalid board display name".into()); }
        if let Event::NameUpdated(next,pending) = http_request(client,HttpCommand::NameChange(endpoint.clone(),(*host).into(),Some(label)))? {
            println!("Board identity saved. Endpoint: {}. Network apply pending: {pending}",next.base);
        }
        return Ok(());
    }
    if let ["device", "rename", name] = words {
        if let Event::NameUpdated(next, pending) = http_request(
            client,
            HttpCommand::NameChange(endpoint.clone(), (*name).into(), None),
        )? {
            println!(
                "Device confirmed persistent name saved. Reconnect at {} after Wi-Fi reassociates. Set PAKAR_ENDPOINT to this URL for subsequent CLI calls.",
                next.base
            );
            if !pending { println!("No Wi-Fi restart was required."); }
        }
        return Ok(());
    }
    if words == ["auth", "info"] {
        let value: serde_json::Value = client
            .get(format!(
                "{}/api/auth/info",
                endpoint.base.trim_end_matches('/')
            ))
            .send()
            .map_err(|e| e.to_string())?
            .error_for_status()
            .map_err(|e| e.to_string())?
            .json()
            .map_err(|e| e.to_string())?;
        println!(
            "{}",
            serde_json::to_string_pretty(&value).map_err(|e| e.to_string())?
        );
        return Ok(());
    }
    if let ["auth", "change", user] = words {
        let password = std::env::var("PAKAR_NEW_PASSWORD").map_err(
            |_| "Set PAKAR_NEW_PASSWORD privately; do not put passwords in command arguments",
        )?;
        access::validate(user, &password, &password)?;
        let next = Endpoint {
            base: endpoint.base.clone(),
            user: (*user).into(),
            password,
        };
        http_request(client, HttpCommand::AccessChange(endpoint.clone(), next, true))?;
        println!("Credentials saved. Update PAKAR_USER/PAKAR_PASSWORD before further commands.");
        return Ok(());
    }
    if words == ["auth", "disable"] {
        let next = Endpoint { base: endpoint.base.clone(), user: String::new(), password: String::new() };
        http_request(client, HttpCommand::AccessChange(endpoint.clone(), next, false))?;
        println!("Device confirmed authentication disabled and saved.");
        return Ok(());
    }
    if words.is_empty() || words[0] == "status" {
        let status = blocking_status(client, endpoint)?;
        output.push_str(&format_text_status(&status, true));
        return Ok(());
    }
    if matches!(words, ["display", "segments", ..] | ["display", "stop"]) {
        let form = if words == ["display", "stop"] {
            vec![("release".into(), "true".into())]
        } else {
            let (settings, text) = display::parse_command(&words[2..])?;
            settings.form(&text)
        };
        let response = client
            .post(format!(
                "{}/api/display",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .form(&form)
            .send()
            .map_err(|e| e.to_string())?;
        if !response.status().is_success() {
            return Err(format!(
                "HTTP {}: {}",
                response.status(),
                response.text().unwrap_or_default()
            ));
        }
        println!("accepted");
        return Ok(());
    }
    let (path, form): (&str, Vec<(&str, &str)>) =
        match words {
            ["mode", value] => ("/api/mode", vec![("value", value)]),
            ["cell", id, state] => (
                "/api/cell",
                vec![("id", id), ("state", state), ("effect", "fade")],
            ),
            ["cell", id, state, effect] => (
                "/api/cell",
                vec![("id", id), ("state", state), ("effect", effect)],
            ),
            ["brightness", value] => ("/api/brightness", vec![("value", value)]),
            ["led-output", "on"] => ("/api/led/output", vec![("enabled", "true")]),
            ["led-output", "off"] => ("/api/led/output", vec![("enabled", "false")]),
            ["led-output", mode @ ("static" | "effects")] => ("/api/led/output", vec![("mode", mode)]),
            ["effect", value] => ("/api/effect", vec![("value", value)]),
            ["display", value] => ("/api/display", vec![("value", value)]),
            ["audio", cue] => ("/api/audio", vec![("cue", cue)]),
            ["snake", value] => ("/api/snake/direction", vec![("value", value)]),
            ["reset"] => ("/api/game/reset", vec![]),
            _ => return Err(
                "commands: device identity, device rename HOST[.local], device label DISPLAY NAME, device configure HOST DISPLAY NAME, auth info, auth change USER, status, mode, cell, cells IDS STATE [EFFECT], brightness, led-output on|off|static|effects, effect, display, audio, reset"
                    .into(),
            ),
        };
    let response = client
        .post(format!("{}{}", endpoint.base.trim_end_matches('/'), path))
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .form(&form)
        .send()
        .map_err(|error| error.to_string())?;
    if !response.status().is_success() {
        return Err(format!(
            "HTTP {}: {}",
            response.status(),
            response.text().unwrap_or_default()
        ));
    }
    println!("accepted");
    Ok(())
}

fn run_text_interface(arguments: &[String]) -> Result<bool, String> {
    if !matches!(
        arguments.get(1).map(String::as_str),
        Some("--status" | "--command" | "--tui")
    ) {
        return Ok(false);
    }
    let endpoint = environment_endpoint();
    if arguments[1] == "--tui" {
        terminal_ui::run(endpoint)?;
        return Ok(true);
    }
    let client = Client::builder()
        .no_proxy()
        .timeout(Duration::from_secs(15))
        .build()
        .map_err(|error| error.to_string())?;
    match arguments[1].as_str() {
        "--status" => text_command(&client, &endpoint, &["status"]),
        "--command" => {
            let words = arguments[2..]
                .iter()
                .map(String::as_str)
                .collect::<Vec<_>>();
            text_command(&client, &endpoint, &words)
        }
        _ => unreachable!(),
    }?;
    Ok(true)
}

fn main() -> eframe::Result {
    let arguments = std::env::args().collect::<Vec<_>>();
    if let Some(result)=client_management::cli(&arguments){
        match result{Ok(())=>println!("Client management operation completed"),Err(error)=>{eprintln!("PakarController: {error}");std::process::exit(2);}}
        return Ok(());
    }
    if arguments.get(1).is_some_and(|arg| matches!(arg.as_str(), "--client-update-init" | "--apply-client-update")) {
        let result = match arguments.get(1).map(String::as_str) {
            Some("--client-update-init") if arguments.len() == 2 => client_install::initialize(),
            Some("--apply-client-update") if arguments.len() == 3 => client_install::helper(std::path::Path::new(&arguments[2])),
            _ => Err("Invalid managed app update arguments".into()),
        };
        match result { Ok(()) => println!("Managed app update operation completed"),
            Err(error) => { eprintln!("PakarController: {error}"); std::process::exit(2); } }
        return Ok(());
    }
    if let Some(output) = cli_output::parse_cli_output(&arguments) {
        println!("{output}");
        return Ok(());
    }
    match run_text_interface(&arguments) {
        Ok(true) => return Ok(()),
        Ok(false) => {}
        Err(error) => {
            eprintln!("PakarController: {error}");
            std::process::exit(2);
        }
    }
    let config = config::AppConfig::load().unwrap_or_default();
    let command = ui::shell::launch_command(&arguments);
    let control_only = arguments
        .iter()
        .any(|arg| matches!(arg.as_str(), "--quit" | "--app-status" | "--wifi" | "--usb" | "--disconnect"
            | "--led-effect" | "--led-mode" | "--led-cell" | "--led-cells" | "--led-output" | "--update-esp" | "--pixel-arm" | "--pixel-show"));
    if control_only {
        match platform::instance::send(&command, Duration::from_secs(2)) {
            Ok(reply) => {
                println!("{reply}");
                return Ok(());
            }
            Err(error) => {
                eprintln!("{error}");
                std::process::exit(3);
            }
        }
    }
    let _ownership = if config.single_instance {
        match platform::instance::acquire_gui_ownership() {
            Ok(platform::instance::GuiOwnership::Primary(owner)) => Some(owner),
            Ok(platform::instance::GuiOwnership::Existing) => {
                let deadline = Instant::now() + Duration::from_secs(5);
                loop {
                    if let Ok(reply) =
                        platform::instance::send(&command, Duration::from_millis(300))
                    {
                        if reply["accepted"] == true {
                            println!("Forwarded to active PakarController");
                            return Ok(());
                        }
                    }
                    if Instant::now() >= deadline {
                        eprintln!("The active instance did not accept the launch request");
                        std::process::exit(3);
                    }
                    thread::sleep(Duration::from_millis(100));
                }
            }
            Err(error) => {
                eprintln!("{error}");
                std::process::exit(3);
            }
        }
    } else {
        None
    };
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_active(std::env::var_os("PAKAR_SCREENSHOT_PATH").is_none())
            .with_inner_size([1180.0, 760.0])
            .with_maximized(true)
            .with_icon(branding::window_icon())
            .with_min_inner_size([900.0, 600.0]),
        ..Default::default()
    };
    eframe::run_native(
        "PakarController",
        options,
        Box::new(move |c| {
            let mut app = PakarControllerApp::new(c);
            app.dispatch(command, &c.egui_ctx);
            Ok(Box::new(app))
        }),
    )
}

#[cfg(test)]
mod tests {
    pub(super) fn mock_device(
        responses: Vec<(u16, Vec<u8>)>,
    ) -> (super::Endpoint, std::thread::JoinHandle<Vec<String>>) {
        mock_device_with_hook(responses, Box::new(|_| {}))
    }
    pub(super) fn mock_device_with_hook(
        responses: Vec<(u16, Vec<u8>)>,
        mut on_request: Box<dyn FnMut(usize) + Send>,
    ) -> (super::Endpoint, std::thread::JoinHandle<Vec<String>>) {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = super::Endpoint {
            base: format!("http://{}", listener.local_addr().unwrap()),
            user: "fixture".into(),
            password: "fixture-secret".into(),
        };
        let worker = std::thread::spawn(move || {
            let mut requests = Vec::new();
            for (code, body) in responses {
                let (mut stream, _) = listener.accept().unwrap();
                stream
                    .set_read_timeout(Some(std::time::Duration::from_secs(3)))
                    .unwrap();
                let mut bytes = Vec::new();
                let mut buffer = [0; 4096];
                loop {
                    let read = stream.read(&mut buffer).unwrap();
                    assert!(read > 0);
                    bytes.extend_from_slice(&buffer[..read]);
                    if let Some(end) = bytes.windows(4).position(|w| w == b"\r\n\r\n") {
                        let header = String::from_utf8_lossy(&bytes[..end]);
                        let length = header
                            .lines()
                            .find_map(|line| {
                                line.to_lowercase()
                                    .strip_prefix("content-length:")
                                    .and_then(|n| n.trim().parse::<usize>().ok())
                            })
                            .unwrap_or(0);
                        if bytes.len() >= end + 4 + length {
                            break;
                        }
                    }
                    assert!(bytes.len() < 3 * 1024 * 1024);
                }
                requests.push(String::from_utf8_lossy(&bytes).into_owned());
                on_request(requests.len());
                write!(stream,"HTTP/1.1 {code} Fixture\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",body.len()).unwrap();
                stream.write_all(&body).unwrap();
            }
            requests
        });
        (endpoint, worker)
    }
    pub(super) fn fixture_network() -> super::device_settings::Network {
        super::device_settings::Network {
            station_enabled: true,
            ap_enabled: true,
            station_ssid: "Fixture".into(),
            addressing: Default::default(),
        }
    }
    #[test]
    fn settings_worker_requires_ack_and_exact_readback() {
        let n = fixture_network();
        let (endpoint, server) = mock_device(vec![
            (202, br#"{"accepted":true}"#.to_vec()),
            (200, serde_json::to_vec(&n).unwrap()),
        ]);
        let client = super::Client::builder()
            .no_proxy()
            .timeout(std::time::Duration::from_secs(3))
            .build()
            .unwrap();
        let command =
            super::device_settings::Write::network(41, &n, "fixture-pass", false).unwrap();
        assert!(matches!(
            super::http_request(&client, super::HttpCommand::Settings(endpoint, command)).unwrap(),
            super::Event::SettingsFinished(41, _, Ok(_))
        ));
        let requests = server.join().unwrap();
        assert_eq!(requests.len(), 2);
        assert!(requests[0].starts_with("POST /api/network "));
        assert!(requests[1].starts_with("GET /api/network "));
        assert!(requests[0].to_lowercase().contains("authorization: basic "));
    }
    #[test]
    fn mismatched_readback_and_negative_ack_never_replay_write() {
        let client = super::Client::builder()
            .no_proxy()
            .timeout(std::time::Duration::from_secs(3))
            .build()
            .unwrap();
        for negative_ack in [false, true] {
            let mut changed = fixture_network();
            changed.ap_enabled = false;
            let responses = if negative_ack {
                vec![(200, br#"{"accepted":false}"#.to_vec())]
            } else {
                vec![
                    (202, br#"{"accepted":true}"#.to_vec()),
                    (200, serde_json::to_vec(&changed).unwrap()),
                ]
            };
            let (endpoint, server) = mock_device(responses);
            let write = super::device_settings::Write::network(
                9,
                &fixture_network(),
                "fixture-pass",
                false,
            )
            .unwrap();
            assert!(matches!(
                super::http_request(&client, super::HttpCommand::Settings(endpoint, write))
                    .unwrap(),
                super::Event::SettingsFinished(9, _, Err(_))
            ));
            let requests = server.join().unwrap();
            assert_eq!(
                requests.iter().filter(|r| r.starts_with("POST ")).count(),
                1
            );
        }
    }
    #[test]
    fn maintenance_checks_positive_device_confirmation() {
        let client = super::Client::builder().no_proxy().build().unwrap();
        let (endpoint, server) = mock_device(vec![(
            200,
            br#"{"programmed":true,"verified":false}"#.to_vec(),
        )]);
        assert!(
            super::maintenance::run(
                &client,
                &endpoint,
                super::maintenance::Operation::ProgramAvr
            )
            .is_err()
        );
        assert_eq!(server.join().unwrap().len(), 1);
        let (endpoint, server) = mock_device(vec![(201, b"AVR image stored".to_vec())]);
        assert!(
            super::maintenance::run(
                &client,
                &endpoint,
                super::maintenance::Operation::StageAvr(vec![0; 32768])
            )
            .unwrap()
            .contains("NOT programmed")
        );
        let requests = server.join().unwrap();
        assert!(requests[0].starts_with("POST /api/avr/image "));
        assert!(requests[0].contains("multipart/form-data"));
    }
    #[test]
    fn backup_export_rejects_wrong_length_and_preserves_existing_files() {
        let destination =
            std::env::temp_dir().join(format!("pakar-fixture-{}.bin", uuid::Uuid::new_v4()));
        let client = super::Client::builder().no_proxy().build().unwrap();
        let operation = super::maintenance::Operation::Download {
            eeprom: true,
            destination: destination.clone(),
        };
        let (endpoint, server) = mock_device(vec![(200, vec![0; 1023])]);
        assert!(super::maintenance::run(&client, &endpoint, operation.clone()).is_err());
        server.join().unwrap();
        assert!(!destination.exists());
        let (endpoint, server) = mock_device(vec![(200, vec![7; 1024])]);
        assert!(super::maintenance::run(&client, &endpoint, operation.clone()).is_ok());
        server.join().unwrap();
        let (endpoint, server) = mock_device(vec![(200, vec![8; 1024])]);
        assert!(super::maintenance::run(&client, &endpoint, operation).is_err());
        server.join().unwrap();
        assert_eq!(std::fs::read(&destination).unwrap(), vec![7; 1024]);
        std::fs::remove_file(destination).unwrap();
    }
    #[test]
    fn late_observations_are_bound_to_their_requested_peer() {
        let current = super::Endpoint {
            base: "http://ledboard.local".into(),
            user: "test".into(),
            password: "old".into(),
        };
        let mut other = current.clone();
        assert!(super::current_event(super::Event::SerialFailure("COM8".into(), "removed".into()), &current, "COM9").is_none());
        other.base = "http://other.local".into();
        assert!(
            super::current_event(
                super::for_endpoint(&other, super::Event::Status(super::DeviceStatus::default())),
                &current,
                "COM9"
            )
            .is_none()
        );
        other = current.clone();
        other.password = "new".into();
        for event in [super::Event::DiagnosticFailure("HTTP".into(), "offline".into()), super::Event::DiagnosticRecovered("HTTP".into())] {
            assert!(super::current_event(super::for_endpoint(&other, event), &current, "COM9").is_none());
        }
        assert!(
            super::current_event(
                super::for_endpoint(&other, super::Event::WebSocket(false)),
                &current,
                "COM9"
            )
            .is_none()
        );
        assert!(
            super::current_event(
                super::for_endpoint(&current, super::Event::WebSocket(true)),
                &current,
                "COM9"
            )
            .is_some()
        );
        assert!(
            super::current_event(
                super::Event::Serial("COM8".into(), true, None),
                &current,
                "COM9"
            )
            .is_none()
        );
        assert!(
            super::current_event(
                super::Event::Serial("COM9".into(), true, None),
                &current,
                "COM9"
            )
            .is_some()
        );
    }
    use super::*;
    #[test]
    fn diagnostic_sources_separate_reads_and_never_coalesce_explicit_writes() {
        let peer = Endpoint { base: "http://ledboard.local".into(), user: "fixture".into(), password: "secret".into() };
        assert_eq!(HttpCommand::Status(peer.clone()).diagnostic_source().as_deref(), Some("HTTP GET /api/status"));
        assert_ne!(HttpCommand::Mapping(peer.clone(), 1).diagnostic_source(), HttpCommand::Status(peer.clone()).diagnostic_source());
        assert!(HttpCommand::Post(peer.clone(), "/api/cell", vec![]).diagnostic_source().is_none());
        assert!(HttpCommand::NameChange(peer, "board".into(), None).diagnostic_source().is_none());
    }
    #[test]
    fn selected_transport_never_falls_back_to_the_other_method() {
        for wifi in [false, true] {
            for usb in [false, true] {
                assert_eq!(
                    command_route(Transport::WiFi, wifi, usb),
                    wifi.then_some(Transport::WiFi)
                );
                assert_eq!(
                    command_route(Transport::Serial, wifi, usb),
                    usb.then_some(Transport::Serial)
                );
            }
        }
    }
    #[test]
    fn usb_selected_http_worker_never_contacts_even_a_live_network_endpoint() {
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let endpoint = Endpoint { base: format!("http://{}", listener.local_addr().unwrap()),
            user: "fixture".into(), password: "fixture".into() };
        let gate = connection::Gate::default();
        gate.select(Some(Transport::Serial));
        let (commands, rx) = mpsc::channel();
        let (events, output) = mpsc::channel();
        let worker = thread::spawn(move || http_worker(rx, events, egui::Context::default(), gate));
        commands.send(HttpCommand::Status(endpoint)).unwrap();
        drop(commands);
        worker.join().unwrap();
        assert!(matches!(listener.accept(), Err(error) if error.kind()==ErrorKind::WouldBlock));
        assert!(output.try_recv().is_err(), "inactive network must not emit failure noise");
    }
    #[test]
    fn offline_usb_preferences_do_not_need_a_valid_network_endpoint() {
        let config: config::AppConfig = serde_json::from_value(serde_json::json!({
            "connection_method":"Serial", "connection_enabled":true, "serial_port":"COM22",
            "endpoint":"not a network URL"})).unwrap();
        assert_eq!(config.connection_method, Transport::Serial);
        assert!(config.connection_enabled);
        let roundtrip: config::AppConfig = serde_json::from_slice(&serde_json::to_vec(&config).unwrap()).unwrap();
        assert_eq!(roundtrip.connection_method, Transport::Serial);
        assert_eq!(roundtrip.serial_port, "COM22");
        assert_eq!(roundtrip.endpoint, "not a network URL");
    }
    #[test]
    fn wire_round_trip() {
        let encoded = wire_frame(OP_CELL_ACTION, 7, &[80, 2, 1]);
        let (op, payload) = decode_frame(&encoded[..encoded.len() - 1]).unwrap();
        assert_eq!(op, OP_CELL_ACTION);
        assert_eq!(payload, vec![80, 2, 1]);
    }
    #[test]
    fn streaming_packet_bounds() {
        let p = rainbow_packet(240, 240, true, 20);
        assert_eq!(p.len(), 730);
        assert_eq!(&p[..4], b"PKAR");
        assert_eq!(p[9], 1);
    }
    #[test]
    fn cell_states_cycle() {
        assert_eq!(PakarControllerApp::next_cell_state("white"), ("red", 1));
        assert_eq!(PakarControllerApp::next_cell_state("red"), ("off", 2));
    }
    #[test]
    fn serial_display_state_extension_preserves_old_status() {
        let mut payload = vec![0; 19];
        payload[0] = 3;
        let old = serial_status(&payload).unwrap();
        assert_eq!(old.mode, "snake");
        assert!(old.tm1637_segments.is_empty());
        payload.extend([1, 2, 4, 8, 5, 1]);
        let current = serial_status(&payload).unwrap();
        assert_eq!(current.tm1637_segments, [1, 2, 4, 8]);
        assert_eq!(current.tm1637_brightness, 5);
        assert!(current.tm1637_host_active);
        payload[24] = 0;
        assert!(!serial_status(&payload).unwrap().tm1637_host_active);
        assert!(serial_status(&payload[..18]).is_none());
        assert!(current.audio_cues.is_empty() && current.avr_version.is_none());
        payload.extend([0, 2, 5, 14]);
        payload[5] = 1;
        let catalog = serial_status(&payload).unwrap();
        assert_eq!(catalog.avr_version.as_deref(), Some("0.2.5"));
        assert_eq!(catalog.audio_cues.len(), 14);
        assert_eq!(catalog.audio_cues[13], "welcome-40led");
        payload[28] = 255;
        assert!(serial_status(&payload).unwrap().audio_cues.is_empty());
        payload[28] = 14;
        payload[5] = 0;
        assert!(serial_status(&payload).unwrap().audio_cues.is_empty());
        assert!(serial_status(&payload).unwrap().avr_version.is_none());
    }
    #[test]
    fn led_output_status_never_invents_enabled_for_missing_or_invalid_usb_extension() {
        let mut payload = vec![0; 42];
        assert!(serial_status(&payload).unwrap().led_output.is_null());
        for (byte, expected) in [(0, false), (1, true)] {
            payload.resize(43, byte);
            payload[42] = byte;
            assert_eq!(serial_status(&payload).unwrap().led_output["enabled"], expected);
        }
        payload[42] = 255;
        assert!(serial_status(&payload).unwrap().led_output.is_null());
        payload[42] = 1;
        assert!(led_output_mode(&serial_status(&payload).unwrap()).is_none());
        payload.push(0);
        assert_eq!(led_output_mode(&serial_status(&payload).unwrap()), Some("static"));
        payload[43] = 1;
        assert_eq!(led_output_mode(&serial_status(&payload).unwrap()), Some("effects"));
        payload[43] = 255;
        assert!(led_output_mode(&serial_status(&payload).unwrap()).is_none());
        let unknown: DeviceStatus = serde_json::from_value(serde_json::json!({"led_output":{"mode":"future"}})).unwrap();
        assert!(led_output_mode(&unknown).is_none());
        let status = DeviceStatus::default();
        assert!(format_text_status(&status, false).contains("LED output Unknown"));
    }
    #[test]
    fn a_transport_failure_does_not_retire_a_fresh_peer_observation() {
        assert!(status_observation_fresh(Some(Instant::now())));
        assert!(!status_observation_fresh(None));
        assert!(!status_observation_fresh(Some(Instant::now() - Duration::from_secs(9))));
        assert!(!websocket_status_ready(&DeviceStatus::default()));
        let ready: DeviceStatus = serde_json::from_value(serde_json::json!({"hostname":"ledboard",
            "capabilities":{"board_kind":"table","acknowledged_features":["table"]}})).unwrap();
        assert!(websocket_status_ready(&ready));
        for non_status in [r#"{"accepted":true,"message":"ok"}"#, r#"{"type":"pong"}"#, r#"{"error":"not a board status"}"#] {
            assert!(websocket_status(non_status).is_none());
        }
        assert!(websocket_status(r#"{"hostname":"ledboard","capabilities":{"board_kind":"table","acknowledged_features":["table"]}}"#).is_some());
    }
    #[test]
    fn mapping_requires_all_unique_cells_but_optional_labels_can_be_absent() {
        let valid: CellMappingResponse = serde_json::from_value(serde_json::json!({"physical":(1..=80).collect::<Vec<_>>() })).unwrap();
        let decoded = valid.decode().unwrap();
        assert_eq!(decoded.len(), 80); assert_eq!(decoded[79].name, "80");
        let duplicate: CellMappingResponse = serde_json::from_value(serde_json::json!({"physical":vec![1;80]})).unwrap();
        assert!(duplicate.decode().is_err());
        assert!(CellMappingResponse::default().decode().is_err());
    }
}
