use std::{
    io::{ErrorKind, Read, Write},
    sync::mpsc::{self, Receiver, Sender, TryRecvError},
    thread,
    time::{Duration, Instant},
};

mod access;
mod audio;
mod brand_image;
mod branding;
mod capabilities;
mod client_telemetry;
mod cli_output;
mod config;
mod device_settings;
mod diagnostics;
mod display;
mod identity;
mod maintenance;
mod messaging;
mod pc_contract;
mod peer_http;
mod platform;
mod streaming;
mod terminal_ui;
mod ui;

use base64::{Engine, engine::general_purpose::STANDARD as BASE64};
use eframe::egui;
use reqwest::blocking::Client;
use serde::Deserialize;
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
    board_id: String,
    #[serde(default)]
    boot_id: String,
    #[serde(default)]
    http_peer_fencing: bool,
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

#[derive(Clone)]
enum HttpCommand {
    Scoped(peer_http::Scope, Box<HttpCommand>),
    Status(Endpoint),
    Mapping(Endpoint),
    Configuration(Endpoint, &'static str),
    Post(Endpoint, &'static str, Vec<(String, String)>),
    AccessChange(Endpoint, Endpoint),
    NameChange(Endpoint, String, Option<String>),
    Settings(Endpoint, device_settings::Write),
    Maintenance(Endpoint, u64, maintenance::Operation),
}

impl HttpCommand {
    fn is_status(&self) -> bool { match self {Self::Scoped(_, command) => command.is_status(),Self::Status(_) => true,_ => false} }
    fn discoverable_read(&self) -> bool { match self {Self::Scoped(_, command) => command.discoverable_read(),Self::Status(_) | Self::Mapping(_) | Self::Configuration(..) => true,_ => false} }
    fn failure(&self, error: String) -> Event {
        match self {
            Self::Scoped(scope, command) => Event::ForScope(scope.clone(), Box::new(command.failure(error))),
            Self::Settings(_, write) => Event::SettingsFinished(write.id, write.group, Err(error)),
            Self::Maintenance(_, id, _) => Event::MaintenanceFinished(*id, Err(error)),
            _ if self.diagnostic_source().is_some() => Event::DiagnosticFailure(self.diagnostic_source().unwrap(), error),
            _ => Event::OperationFailed(error),
        }
    }
    fn diagnostic_source(&self) -> Option<String> {
        match self {
            Self::Scoped(_, command) => command.diagnostic_source(),
            Self::Status(_) => Some("HTTP GET /api/status".into()),
            Self::Mapping(_) => Some("HTTP GET /api/cells/mapping".into()),
            Self::Configuration(_, path) => Some(format!("HTTP GET {path}")),
            _ => None, // Explicit operations keep their own correlated outcomes.
        }
    }
    fn endpoint(&self) -> &Endpoint {
        match self {
            Self::Scoped(_, command) => command.endpoint(),
            Self::Status(endpoint)
            | Self::Mapping(endpoint)
            | Self::Configuration(endpoint, ..)
            | Self::AccessChange(endpoint, ..)
            | Self::NameChange(endpoint, ..)
            | Self::Settings(endpoint, ..)
            | Self::Maintenance(endpoint, ..)
            | Self::Post(endpoint, ..) => endpoint,
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
            Self::Mapping(endpoint) => Self::Mapping(replace(endpoint)),
            Self::Configuration(endpoint, path) => Self::Configuration(replace(endpoint), path),
            Self::Post(endpoint, path, form) => Self::Post(replace(endpoint), path, form.clone()),
            Self::AccessChange(endpoint, next) => {
                Self::AccessChange(replace(endpoint), replace(next))
            }
            Self::NameChange(endpoint, name, label) => Self::NameChange(replace(endpoint), name.clone(), label.clone()),
            Self::Settings(endpoint, write) => Self::Settings(replace(endpoint), write.clone()),
            Self::Maintenance(endpoint, id, operation) => {
                Self::Maintenance(replace(endpoint), *id, operation.clone())
            }
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
    physical: Vec<u8>,
    #[serde(default)]
    names: Vec<String>,
}
enum WsCommand {
    Connect(Endpoint),
    Rate(u16),
    Mapping(Vec<CellMapping>),
    Stream(bool),
    Sample(bool),
}
enum SerialCommand {
    Port(String),
    Send(String, u8, Vec<u8>),
    Identity(String, String, String),
}
enum Event {
    ForScope(peer_http::Scope, Box<Event>),
    ForEndpoint(Endpoint, Box<Event>),
    StreamStats(f64),
    Status(DeviceStatus),
    Mapping(Vec<CellMapping>),
    Configuration(&'static str, serde_json::Value),
    Accepted(String),
    CredentialsUpdated(Endpoint),
    NameUpdated(Endpoint, bool),
    SerialIdentity(String, serde_json::Value),
    SerialIdentitySaved(String, Result<(), String>),
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
    WebSocket(bool),
    Serial(String, bool, Option<DeviceStatus>),
}

impl Event {
    fn http_status(&self) -> Option<&DeviceStatus> { match self { Self::ForScope(_, inner) => inner.http_status(),Self::Status(status) => Some(status),_ => None } }
    fn http_succeeded(&self) -> bool { match self { Self::ForScope(_, inner) => inner.http_succeeded(),Self::DiagnosticFailure(..) | Self::OperationFailed(_) | Self::SettingsFinished(_, _, Err(_)) | Self::MaintenanceFinished(_, Err(_)) => false,_ => true } }
}

fn for_endpoint(endpoint: &Endpoint, event: Event) -> Event {
    Event::ForEndpoint(endpoint.clone(), Box::new(event))
}

fn current_event(event: Event, endpoint: &Endpoint, serial_port: &str) -> Option<Event> {
    match event {
        Event::ForEndpoint(origin, inner) if origin == *endpoint => Some(*inner),
        Event::ForEndpoint(..) => None,
        Event::Serial(ref port, ..) if port != serial_port => None,
        Event::SerialFailure(ref port, ..) if port != serial_port => None,
        other => Some(other),
    }
}

fn http_request(client: &Client, command: HttpCommand) -> Result<Event, String> {
    if let HttpCommand::Scoped(scope, command) = command {
        return scoped_http_request(client, &scope, &command).map(|result| Event::ForScope(scope, Box::new(result)));
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
    if let HttpCommand::AccessChange(endpoint, next) = &command {
        let response = client
            .post(format!(
                "{}/api/auth",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .form(&[
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
        return Ok(Event::CredentialsUpdated(next.clone()));
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
        HttpCommand::Mapping(endpoint) => {
            let url = format!(
                "{}/api/cells/mapping",
                endpoint.base.trim_end_matches('/')
            );
            let request = client.get(url);
            (endpoint, request, 2_u8)
        }
        HttpCommand::Configuration(..)
        | HttpCommand::AccessChange(..)
        | HttpCommand::NameChange(..) => unreachable!(),
        HttpCommand::Settings(..) => unreachable!(),
        HttpCommand::Maintenance(..) => unreachable!(),
        HttpCommand::Post(endpoint, path, form) => {
            let url = format!("{}{}", endpoint.base.trim_end_matches('/'), path);
            let request = client.post(url).form(&form);
            (endpoint, request, 0_u8)
        }
    };
    let response = request
        .basic_auth(endpoint.user, Some(endpoint.password))
        .send()
        .map_err(diagnostics::describe_error)?;
    let code = response.status();
    if !code.is_success() {
        return Err(format!(
            "HTTP {code}: {}",
            response.text().unwrap_or_default()
        ));
    }
    match response_kind {
        1 => response
            .json::<DeviceStatus>()
            .map(Event::Status)
            .map_err(diagnostics::describe_error),
        2 => response
            .json::<CellMappingResponse>()
            .map(|value| {
                Event::Mapping(
                    value
                        .physical
                        .into_iter()
                        .enumerate()
                        .map(|(index, physical_cell)| CellMapping {
                            logical_id: (index + 1) as u8,
                            physical_cell,
                            name: value.names.get(index).cloned().unwrap_or_default(),
                        })
                        .collect(),
                )
            })
            .map_err(diagnostics::describe_error),
        _ => Ok(Event::Accepted(format!(
            "Device accepted the command ({code})"
        ))),
    }
}

fn scoped_http_request(client: &Client, scope: &peer_http::Scope, command: &HttpCommand) -> Result<Event, String> {
    // Adapt donor acquireWrite's checks before AND after waiting/admission.
    scope.admit()?;
    if matches!(command, HttpCommand::Status(_)) { return http_request(client, command.clone()); }
    let mutating = !matches!(command, HttpCommand::Mapping(_) | HttpCommand::Configuration(..)
        | HttpCommand::Maintenance(_, _, maintenance::Operation::RemoteStatus | maintenance::Operation::Download {..}));
    if mutating && scope.pin.is_none() { return Err("Read-only: peer has not advertised valid board/boot identity and HTTP fencing".into()); }
    let before = blocking_status(client, command.endpoint())?;
    let observed = peer_http::Pin::from_status(&before);
    if scope.pin != observed { return Err("Board/boot identity changed before dispatch; nothing was sent".into()); }
    scope.admit()?;
    if mutating {
        let allowed = before.capabilities.as_ref().is_some_and(|c| match command {
            HttpCommand::Post(_, path, form) => c.allows(path, form),
            HttpCommand::Settings(_, write) => c.allows(write.group.path(), &write.form),
            HttpCommand::NameChange(..) => c.has("identity"),
            HttpCommand::AccessChange(..) => c.has("network"),
            HttpCommand::Maintenance(_, _, operation) => c.has(match operation {
                maintenance::Operation::ReconnectCloud => "network",
                maintenance::Operation::Probe | maintenance::Operation::Backup | maintenance::Operation::StageAvr(_)
                | maintenance::Operation::ProgramAvr => "isp",
                _ => "ota",
            }),
            _ => false,
        });
        if !allowed { return Err("This board no longer acknowledges the requested capability; nothing was sent".into()); }
    }
    let pinned = scope.pin.as_ref().map(peer_http::Pin::client).transpose()?;
    let result = http_request(pinned.as_ref().unwrap_or(client), command.clone())?;
    // Credential/hostname/self-update results may intentionally retire the endpoint.
    // Settings already performed readback; validate its observed epoch separately.
    let verify_read = matches!(command, HttpCommand::Mapping(_) | HttpCommand::Configuration(..))
        || matches!(&result, Event::SettingsFinished(_, _, Ok(_)) | Event::NameUpdated(_, false));
    if verify_read {
        let after = blocking_status(client, command.endpoint()).map_err(|_| "Result received but board/boot readback unavailable; inspect before retrying".to_string())?;
        if scope.pin != peer_http::Pin::from_status(&after) { return Err("Board/boot changed during readback; result not applied to this peer".into()); }
    }
    if !scope.current() { return Err("Board session revoked after dispatch; outcome unknown, no replay".into()); }
    Ok(result)
}

fn http_worker(commands: Receiver<HttpCommand>, events: Sender<Event>, context: egui::Context) {
    let mut reporter = client_telemetry::Reporter::new();
    let client = Client::builder()
        .no_proxy()
        .redirect(reqwest::redirect::Policy::none())
        .timeout(Duration::from_secs(4))
        .build()
        .expect("HTTP client");
    let mut discovered_base: Option<String> = None;
    let mut requested_base = String::new();
    while let Ok(command) = commands.recv() {
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
            && requested_name.is_some()
            && command.discoverable_read()
        {
            discovered_base = None;
            for base in platform::discovery::ssdp_locations(Duration::from_millis(1200)) {
                let mut candidate = command.endpoint().clone();
                candidate.base = base.clone();
                // A rename must not make fallback select some other controller
                // merely because it shares credentials. Read/verify its label first.
                let Ok(status) = blocking_status(&client, &candidate) else {
                    continue;
                };
                if Some(&status.hostname) != requested_name.as_ref() {
                    continue;
                }
                if let Ok(event) = http_request(&client, command.with_base(base.clone())) {
                    discovered_base = Some(base);
                    result = Ok(event);
                    break;
                }
            }
        }
        if command.is_status() && reporter.due() && let Some(status) = result.as_ref().ok().and_then(Event::http_status)
            && status.capabilities.as_ref().is_some_and(|c| c.has("telemetry"))
            && let Ok(pinned) = peer_http::pinned_client(status)
            && match &command {HttpCommand::Scoped(scope, _) => scope.current(),_ => false} {
            let mut endpoint = command.endpoint().clone();
            if let Some(base) = &discovered_base {
                endpoint.base = base.clone();
            }
            reporter.report(&pinned, &endpoint);
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

fn ws_worker(commands: Receiver<WsCommand>, events: Sender<Event>, context: egui::Context) {
    let mut retry = diagnostics::Backoff::new(Duration::from_millis(600), Duration::from_secs(15));
    let mut endpoint: Option<Endpoint> = None;
    let mut streaming = false;
    let mut sampling = false;
    let mut fps = streaming::DEFAULT_FPS;
    let mut mapping = Vec::new();
    loop {
        if endpoint.is_none() {
            match commands.recv() {
                Ok(WsCommand::Connect(e)) => endpoint = Some(e),
                Ok(WsCommand::Rate(value)) => fps = value.min(streaming::MAX_FPS),
                Ok(WsCommand::Mapping(value)) => mapping = value,
                Ok(WsCommand::Stream(v)) => streaming = v,
                Ok(WsCommand::Sample(v)) => sampling = v,
                Err(_) => return,
            }
        }
        while let Ok(command) = commands.try_recv() {
            match command {
                WsCommand::Connect(e) => {
                    if endpoint.as_ref() != Some(&e) {
                        mapping.clear();
                        sampling = false;
                        streaming = false;
                    }
                    endpoint = Some(e);
                }
                WsCommand::Rate(value) => fps = value.min(streaming::MAX_FPS),
                WsCommand::Mapping(value) => mapping = value,
                WsCommand::Stream(v) => streaming = v,
                WsCommand::Sample(v) => sampling = v,
            }
        }
        let Some(current) = endpoint.clone() else {
            continue;
        };
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
            let (mut socket, _) = tungstenite::connect(request).map_err(|e| e.to_string())?;
            if let tungstenite::stream::MaybeTlsStream::Plain(stream) = socket.get_mut() {
                stream
                    .set_read_timeout(Some(Duration::from_millis(40)))
                    .map_err(|e| e.to_string())?;
            }
            let _ = events.send(for_endpoint(&current, Event::WebSocket(true)));
            let _ = events.send(for_endpoint(&current, Event::DiagnosticRecovered("WebSocket".into())));
            retry.reset();
            context.request_repaint();
            let mut hue = 0_u16;
            let mut last_stream = Instant::now() - Duration::from_millis(50);
            let mut stats_at = Instant::now();
            let mut sent_frames = 0_u32;
            loop {
                match commands.try_recv() {
                    Ok(WsCommand::Connect(new_endpoint)) => {
                        mapping.clear();
                        sampling = false;
                        streaming = false;
                        endpoint = Some(new_endpoint);
                        break;
                    }
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
                if let tungstenite::stream::MaybeTlsStream::Plain(stream) = socket.get_mut() {
                    stream
                        .set_read_timeout(Some(streaming::read_budget(
                            streaming || sampling,
                            fps,
                            last_stream.elapsed(),
                        )))
                        .map_err(|e| e.to_string())?;
                }
                match socket.read() {
                    Ok(Message::Text(text)) => {
                        if let Ok(status) = serde_json::from_str::<DeviceStatus>(&text) {
                            let _ = events.send(for_endpoint(&current, Event::Status(status)));
                            context.request_repaint();
                        }
                    }
                    Ok(Message::Close(_)) => return Err("WebSocket closed".into()),
                    Ok(_) => {}
                    Err(tungstenite::Error::Io(e))
                        if matches!(e.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) => {}
                    Err(e) => return Err(e.to_string()),
                }
            }
            Ok(())
        })();
        let _ = events.send(for_endpoint(&current, Event::WebSocket(false)));
        context.request_repaint();
        if let Err(error) = result {
            streaming = false;
            sampling = false; // Reconnect must not silently restart a stream.
            let _ = events.send(for_endpoint(
                &current,
                Event::DiagnosticFailure("WebSocket".into(), error),
            ));
            // Interruptible: changing peer or stopping streaming need not wait
            // for the retry deadline. Mutations are never replayed here.
            match commands.recv_timeout(retry.delay()) {
                Ok(WsCommand::Connect(e)) => { endpoint = Some(e); mapping.clear(); streaming = false; sampling = false; retry.reset(); }
                Ok(WsCommand::Rate(value)) => fps = value.min(streaming::MAX_FPS),
                Ok(WsCommand::Mapping(value)) => mapping = value,
                Ok(WsCommand::Stream(value)) => streaming = value,
                Ok(WsCommand::Sample(value)) => sampling = value,
                Err(mpsc::RecvTimeoutError::Disconnected) => return,
                Err(mpsc::RecvTimeoutError::Timeout) => retry.failed(),
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
        mode: ["idle", "cell", "first-wins", "snake"]
            .get(payload[0] as usize)
            .unwrap_or(&"unknown")
            .to_string(),
        winner: payload[1],
        selected_cell: payload[2],
        brightness: payload[3],
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

fn serial_worker(commands: Receiver<SerialCommand>, events: Sender<Event>, context: egui::Context) {
    let mut port_name = String::new();
    let mut sequence = 1_u8;
    let mut nonce = 0_u8;
    loop {
        if port_name.is_empty() {
            match commands.recv() {
                Ok(SerialCommand::Port(p)) => port_name = p,
                Ok(_) => {}
                Err(_) => return,
            }
        }
        if port_name.is_empty() { continue; }
        let mut port = match serialport::new(&port_name,115_200).timeout(Duration::from_millis(60)).open() {
            Ok(port) => port,
            Err(error) => {
                let _ = events.send(Event::SerialFailure(port_name.clone(), error.to_string()));
                let _ = events.send(Event::Serial(port_name.clone(),false,None));
                context.request_repaint();
                thread::sleep(Duration::from_secs(1));
                while let Ok(c) = commands.try_recv() { if let SerialCommand::Port(p)=c {port_name=p} }
                continue;
            }
        };
        let connected_port = port_name.clone();
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
        'connected: loop {
            while let Ok(c)=commands.try_recv() {
                match c {
                    SerialCommand::Port(p)=>{port_name=p;break 'connected}
                    SerialCommand::Send(target,op,payload) if target==connected_port=>{
                        let _=port.write_all(&wire_frame(op,sequence,&payload));sequence=sequence.wrapping_add(1);
                    }
                    SerialCommand::Identity(target,host,label) if target==connected_port=>{
                        if expected.is_some() {
                            let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("An identity write is already pending".into())));
                        } else {
                            match identity::usb_write(&host,&label) {
                                Ok(next)=>{packets=next;expected=Some((identity::normalize_name(&host).unwrap(),label));read=None;read_wait=None;}
                                Err(error)=>{let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err(error)));}
                            }
                        }
                    }
                    _=>{}
                }
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
            if write_wait.is_none() {
                if let Some(payload)=packets.pop_front() {
                    if port.write_all(&wire_frame(0x55,sequence,&payload)).is_err(){break}
                    write_wait=Some((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                } else if let Some(reader)=&read {
                    if read_wait.is_none() {
                        if port.write_all(&wire_frame(0x59,sequence,&reader.request())).is_err(){break}
                        read_wait=Some((sequence,Instant::now()));sequence=sequence.wrapping_add(1);
                    }
                }
            }
            if last_poll.elapsed()>=Duration::from_millis(750) {
                let _=port.write_all(&wire_frame(OP_GET_STATUS,sequence,&[]));sequence=sequence.wrapping_add(1);last_poll=Instant::now();
            }
            let mut bytes=[0_u8;128];
            match port.read(&mut bytes) {
                Ok(n)=>for byte in &bytes[..n] {
                    if *byte==0 {
                        if let Some((op,response_sequence,payload))=decode_envelope(&encoded) {
                            if op==OP_STATUS {
                                if payload.len()>=38 && payload[31]&(1<<5)!=0 && packets.is_empty() && write_wait.is_none()
                                    && read.is_none() && (stamp!=Some(payload[37]) || last_identity.elapsed()>Duration::from_secs(10)) {
                                    if stamp!=Some(payload[37]) { identity=None; }
                                    stamp=Some(payload[37]);nonce=nonce.wrapping_add(1);
                                    read=Some(identity::UsbRead{nonce,..Default::default()});last_identity=Instant::now();
                                }
                                if let Some(mut status)=serial_status(&payload) {
                                    if let (Some(value),Some(c))=(&identity,&mut status.capabilities) {
                                        c.name=value["display_name"].as_str().unwrap_or_default().into();
                                        c.hostname=value["hostname"].as_str().unwrap_or_default().into();
                                        c.application=serde_json::from_value(value["application"].clone()).ok();
                                        status.hostname=c.hostname.clone();
                                    }
                                    let _=events.send(Event::Serial(connected_port.clone(),true,Some(status)));
                                }
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
        if expected.is_some(){let _=events.send(Event::SerialIdentitySaved(connected_port.clone(),Err("USB disconnected during identity write; reconnect and read before retrying".into())));}
        let _=events.send(Event::Serial(connected_port,false,None));context.request_repaint();
    }
}
#[derive(Clone, Copy, Debug, PartialEq)]
enum Transport {
    WiFi,
    Serial,
    Both,
}

// "Both" maintains both connections, not two copies of each mutation. A route
// is selected before dispatch; uncertain outcomes must never trigger replay.
fn command_route(selected: Transport, wifi_fresh: bool, usb_fresh: bool) -> Option<Transport> {
    match selected {
        Transport::WiFi if wifi_fresh => Some(Transport::WiFi),
        Transport::Serial if usb_fresh => Some(Transport::Serial),
        Transport::Both if wifi_fresh => Some(Transport::WiFi),
        Transport::Both if usb_fresh => Some(Transport::Serial),
        _ => None,
    }
}
struct PakarControllerApp {
    context_panels: Vec<ui::layout::Panel>,
    active_context: Option<String>,
    context_layouts: std::collections::HashMap<String, egui_dock::DockState<ui::layout::Panel>>,
    config: config::AppConfig,
    preferences: Option<ui::preferences::PreferencesDraft>,
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
    capture: ui::screenshot::State,
    screenshot_harness: bool,
    focus_cell_search: bool,
    cell_effect: String,
    event_log: diagnostics::EventLog,
    status_request_pending: bool,
    status_retry: diagnostics::Backoff,
    last_status_at: Option<Instant>,
    last_serial_status_at: Option<Instant>,
    configuration_loaded: bool,
    tray: Option<platform::windows_tray::TrayController>,
    notifications: std::cell::RefCell<platform::notifications::Delivery>,
    connection_notice: platform::notifications::ConnectionNotice,
    tray_tooltip: String,
    #[cfg(windows)]
    windows_desktop: Option<platform::windows_desktop::Desktop>,
    base: String,
    user: String,
    password: String,
    access_user: String,
    access_password: String,
    access_confirm: String,
    access_info: Option<serde_json::Value>,
    serial_port: String,
    transport: Transport,
    status: Option<DeviceStatus>,
    serial_status: Option<DeviceStatus>,
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
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        thread::Builder::new()
            .name("pakar-http".into())
            .spawn(move || http_worker(http_rx, events, ctx))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        thread::Builder::new()
            .name("pakar-websocket".into())
            .spawn(move || ws_worker(ws_rx, events, ctx))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        thread::Builder::new()
            .name("pakar-serial".into())
            .spawn(move || serial_worker(serial_rx, event_tx, ctx))
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
            context_panels: Vec::new(),
            active_context: None,
            context_layouts: Default::default(),
            control: platform::instance::start(context.egui_ctx.clone(), !screenshot_harness),
            dock,
            reveal_panels: Vec::new(),
            close_panels: Vec::new(),
            open_panels: Vec::new(),
            preferences: None,
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
            capture: ui::screenshot::State::from_environment(),
            screenshot_harness,
            focus_cell_search: false,
            cell_effect: "fade".into(),
            event_log,
            status_request_pending: false,
            status_retry: diagnostics::Backoff::new(Duration::from_secs(2), Duration::from_secs(30)),
            last_status_at: None,
            last_serial_status_at: None,
            configuration_loaded: false,
            tray: tray.ok().flatten(),
            notifications: Default::default(),
            connection_notice: Default::default(),
            tray_tooltip: String::new(),
            #[cfg(windows)]
            windows_desktop: windows_desktop.ok().flatten(),
            base: std::env::var("PAKAR_ENDPOINT").unwrap_or_else(|_| config.endpoint.clone()),
            user: std::env::var("PAKAR_USER").unwrap_or_else(|_| config.user.clone()),
            password: std::env::var("PAKAR_PASSWORD").unwrap_or_else(|_| "pakarcontroller".into()),
            access_user: String::new(),
            access_password: String::new(),
            access_confirm: String::new(),
            access_info: None,
            serial_port: std::env::var("PAKAR_SERIAL")
                .unwrap_or_else(|_| config.serial_port.clone()),
            transport: Transport::Both,
            status: None,
            serial_status: None,
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
        self.status_request_pending = false;
        self.device_settings.invalidate_peer();
        self.configuration_loaded = false;
        self.table.stop_pending_edits(); self.table.selection.ids.clear();
        self.table.editing_order = false; self.table.manager_open = false;
        self.table.pixel_sample = None; self.cell_search.clear();
        self.confirm_reset = false; self.show_mapping = false;
        self.mappings.clear(); self.mapping_name.clear(); self.mapping_logical = 1; self.mapping_physical = 1;
        self.access_info = None; self.access_user.clear(); self.access_password.clear(); self.access_confirm.clear();
        self.display_text.clear(); self.device_name = None; self.board_display_name = None;
        self.identity_pending = false; self.identity_saved = None;
        self.streaming = false; self.sampling = false; self.stream_sent_fps = None;
        self.last_mapping_poll = Instant::now() - Duration::from_secs(16);
        let _ = self.ws_tx.send(WsCommand::Mapping(Vec::new()));
        let _ = self.ws_tx.send(WsCommand::Stream(false)); let _ = self.ws_tx.send(WsCommand::Sample(false));
    }
    fn post(&mut self, path: &'static str, form: Vec<(String, String)>) {
        if !self.http_writable() || !self.peer_context().is_some_and(|c| c.allows(path, &form)) {
            self.message = "This connected board has not acknowledged this capability".into();
            return;
        }
        if self.transport != Transport::Serial {
            let _ = self
                .http_tx
                .send(HttpCommand::Post(self.endpoint(), path, form));
        }
    }
    fn serial(&self, op: u8, payload: Vec<u8>) {
        if self.transport != Transport::WiFi {
            let _ = self
                .serial_tx
                .send(SerialCommand::Send(self.serial_port.clone(), op, payload));
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
    fn poll(&mut self) {
        let endpoint = self.endpoint();
        if self.last_ws_endpoint.as_ref() != Some(&endpoint) {
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
            let _ = self.ws_tx.send(WsCommand::Connect(endpoint.clone()));
            let _ = self.ws_tx.send(WsCommand::Stream(false));
            let _ = self.ws_tx.send(WsCommand::Sample(false));
            let _ = self.ws_tx.send(WsCommand::Rate(self.config.stream_fps));
            self.last_ws_endpoint = Some(endpoint);
            self.configuration_loaded = false;
        }
        if self.last_serial_port != self.serial_port {
            self.event_log.reset_sources("USB");
            self.identity_pending = false;
            self.connection_notice.reset();
            self.serial_status = None;
            self.last_serial_status_at = None;
            self.serial_live = false;
            let _ = self
                .serial_tx
                .send(SerialCommand::Port(self.serial_port.clone()));
            self.last_serial_port = self.serial_port.clone();
        }
        if !self.status_request_pending && self.last_poll.elapsed() >= self.status_retry.delay() {
            self.last_poll = Instant::now();
            self.status_request_pending = self.http_tx.send(HttpCommand::Status(self.endpoint())).is_ok();
            if self.device_settings.open && self.device_settings.page == 9
                && self.command_route() == Some(Transport::WiFi) {
                let _ = self.http_tx.send(HttpCommand::Configuration(
                    self.endpoint(),
                    "/api/firmware/remote",
                ));
            }
        }
        // Never flood an offline/bootstrap queue with settings preflights.
        // Read-only legacy peers can still load after a fresh Wi-Fi status.
        if !self.configuration_loaded && self.command_route() == Some(Transport::WiFi) {
            let _ = self.http_tx.send(HttpCommand::Configuration(
                self.endpoint(),
                "/api/network",
            ));
            let _ = self.http_tx.send(HttpCommand::Configuration(
                self.endpoint(),
                "/api/device/name",
            ));
            let _ = self
                .http_tx
                .send(HttpCommand::Configuration(self.endpoint(), "/api/cloud"));
            self.configuration_loaded = true;
            let _ = self.http_tx.send(HttpCommand::Configuration(
                self.endpoint(),
                "/api/auth/info",
            ));
        }
        if self.command_route() == Some(Transport::WiFi) && self.has_table() && self.last_mapping_poll.elapsed() >= Duration::from_secs(15) {
            self.last_mapping_poll = Instant::now();
            let _ = self.http_tx.send(HttpCommand::Mapping(self.endpoint()));
        }
        while let Ok(event) = self.event_rx.try_recv() {
            if let Some(receipt) = peer_http::retired_envelope(&event) {
                self.record_event("Previous board session", &receipt);
                continue;
            }
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
                Event::ForScope(..) => {} // Scope envelopes are unwrapped once.
                Event::ForEndpoint(..) => {} // Worker envelopes are unwrapped once.
                Event::StreamStats(value) => self.stream_sent_fps = Some((value, Instant::now())),
                Event::Status(s) => {
                    if !s.hostname.is_empty() {
                        if self.http_tx.observe(&s) { self.revoke_http_drafts(); }
                        if s.pixel_preview_requested {
                            self.table.pixel_sample =
                                s.pixel_frame.clone().filter(|f| f.valid()).map(|frame| {
                                    ui::cell_pixels::Sample {
                                        frame,
                                        at: Instant::now(),
                                    }
                                });
                        }
                        self.brightness = s.brightness;
                        self.pattern = s.effect.clone();
                        self.output_mask = s.active_outputs;
                        self.status = Some(s);
                        self.last_status_at = Some(Instant::now());
                        self.message.clear();
                    }
                }
                Event::Mapping(mappings) => {
                    self.mappings = mappings;
                    let _ = self.ws_tx.send(WsCommand::Mapping(self.mappings.clone()));
                    if !streaming::valid_mapping(&self.mappings) {
                        self.sampling = false;
                    }
                }
                Event::Configuration("/api/auth/info", value) => {
                    self.access_user = value
                        .get("username")
                        .and_then(|v| v.as_str())
                        .unwrap_or_default()
                        .into();
                    self.access_info = Some(value);
                }
                Event::Configuration(path, value) => {
                    if path != "/api/device/name" || self.command_route() != Some(Transport::Serial) {
                        self.receive_configuration(path, value);
                    }
                }
                Event::MaintenanceFinished(id, result) => {
                    if self.device_settings.maintenance_pending == Some(id) {
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
                Event::Accepted(m) => {
                    self.record_event("Command", &m);
                    self.notify("Command accepted", &m, messaging::Severity::Success);
                }
                Event::OperationFailed(error) => {
                    self.identity_pending = false;
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
                    if source == "HTTP GET /api/status" {
                        if self.http_tx.writable() {
                            self.http_tx.clear();
                            self.revoke_http_drafts();
                            self.status = None;
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
                    }
                    if let Some(s) = s {
                        self.serial_status = Some(s);
                        self.last_serial_status_at = Some(Instant::now());
                    }
                }
            }
        }
        let state = self.live_status().map(|s| s.avr_online);
        if let Some((message, severity)) = self.connection_notice.observe(state) {
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
        if tip != self.tray_tooltip {
            if let Some(tray) = &self.tray {
                if let Err(error) = tray.set_tooltip(&tip) {
                    self.record_event("Windows tray", &error);
                }
            }
            self.tray_tooltip = tip;
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
        password: std::env::var("PAKAR_PASSWORD").unwrap_or_else(|_| "pakarcontroller".into()),
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
        .map_err(diagnostics::describe_error)?;
    if !response.status().is_success() {
        return Err(format!(
            "HTTP {}: {}",
            response.status(),
            response.text().unwrap_or_default()
        ));
    }
    response.json().map_err(diagnostics::describe_error)
}

fn format_text_status(status: &DeviceStatus, table: bool) -> String {
    let mut output = String::new();
    macro_rules! println { ($($arg:tt)*) => {{ use std::fmt::Write as _; let _ = writeln!(output, $($arg)*); }}; }
    macro_rules! print { ($($arg:tt)*) => {{ use std::fmt::Write as _; let _ = write!(output, $($arg)*); }}; }
    println!(
        "{} {} | AVR {} | TM1637 {} | {} | MQTT {} | NTP {} | brightness {} | effect {}",
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
        status.effect
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
        http_request(client, HttpCommand::AccessChange(endpoint.clone(), next))?;
        println!("Credentials saved. Update PAKAR_USER/PAKAR_PASSWORD before further commands.");
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
            ["effect", value] => ("/api/effect", vec![("value", value)]),
            ["display", value] => ("/api/display", vec![("value", value)]),
            ["audio", cue] => ("/api/audio", vec![("cue", cue)]),
            ["snake", value] => ("/api/snake/direction", vec![("value", value)]),
            ["reset"] => ("/api/game/reset", vec![]),
            _ => return Err(
                "commands: device identity, device rename HOST[.local], device label DISPLAY NAME, device configure HOST DISPLAY NAME, auth info, auth change USER, status, mode, cell, brightness, effect, display, audio, snake, reset"
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
        .any(|arg| matches!(arg.as_str(), "--quit" | "--app-status"));
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
        assert_ne!(HttpCommand::Mapping(peer.clone()).diagnostic_source(), HttpCommand::Status(peer.clone()).diagnostic_source());
        assert!(HttpCommand::Post(peer.clone(), "/api/cell", vec![]).diagnostic_source().is_none());
        assert!(HttpCommand::NameChange(peer, "board".into(), None).diagnostic_source().is_none());
    }
    #[test]
    fn dual_transport_selects_one_route_without_replay() {
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
                assert_eq!(
                    command_route(Transport::Both, wifi, usb),
                    if wifi {
                        Some(Transport::WiFi)
                    } else {
                        usb.then_some(Transport::Serial)
                    }
                );
            }
        }
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
}
