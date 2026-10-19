//! Actual RayanLamp internal/tui/run.go, text.go and terminal/policy.go
//! adapted to Rust/crossterm; PCController tui/table.go owns viewport math.
//! Shared Pakar capabilities/selection/diagnostics remain the authorities.
use crate::{blocking_status, diagnostics, ui, DeviceStatus, Endpoint};
use crossterm::{
    cursor,
    event::{self, Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers},
    execute, queue,
    style::{Color, Print, ResetColor, SetForegroundColor},
    terminal::{self, Clear, ClearType, EnterAlternateScreen, LeaveAlternateScreen},
};
use std::{
    io::{self, IsTerminal, Write},
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc, Arc,
    },
    thread,
    time::{Duration, Instant},
};
use unicode_width::UnicodeWidthChar;

/// Adapt sanitizeTerminalLine/unsafeBidiControl. Never let a peer write OSC,
/// CSI, cursor controls or bidi overrides; terminal styling is exclusively ours.
pub fn safe_line(value: &str) -> String {
    #[derive(Clone, Copy)]
    enum Escape {
        None,
        Start,
        Csi,
        String,
        StringEnd,
    }
    let mut escape = Escape::None;
    let mut result = String::new();
    for c in value.chars().take(16384) {
        match escape {
            Escape::Start => {
                escape = match c {
                    '[' => Escape::Csi,
                    ']' | 'P' | 'X' | '^' | '_' => Escape::String,
                    '\u{20}'..='\u{2f}' => Escape::Start,
                    _ => Escape::None,
                };
                continue;
            }
            Escape::Csi => {
                if ('\u{40}'..='\u{7e}').contains(&c) {
                    escape = Escape::None;
                }
                continue;
            }
            Escape::String => {
                if c == '\u{7}' || c == '\u{9c}' {
                    escape = Escape::None;
                } else if c == '\u{1b}' {
                    escape = Escape::StringEnd;
                }
                continue;
            }
            Escape::StringEnd => {
                escape = if c == '\\' {
                    Escape::None
                } else {
                    Escape::String
                };
                continue;
            }
            Escape::None => {}
        }
        match c {
            '\u{1b}' => escape = Escape::Start,
            '\u{9b}' => escape = Escape::Csi,
            '\u{90}' | '\u{98}' | '\u{9d}' | '\u{9e}' | '\u{9f}' => escape = Escape::String,
            '\r' | '\n' | '\t' => result.push(' '),
            _ if c.is_control()
                || matches!(c, '\u{61c}' | '\u{200e}' | '\u{200f}' | '\u{feff}'
                | '\u{202a}'..='\u{202e}' | '\u{2066}'..='\u{2069}') =>
            {
                result.push('�')
            }
            _ => result.push(c),
        }
    }
    result.split_whitespace().collect::<Vec<_>>().join(" ")
}
fn fit(value: &str, width: usize) -> String {
    let text = safe_line(value);
    if text.chars().map(|c| c.width().unwrap_or(0)).sum::<usize>() <= width {
        return text;
    }
    if width == 0 {
        return String::new();
    }
    let mut used = 0;
    let mut result = String::new();
    for c in text.chars() {
        let size = c.width().unwrap_or(0);
        if used + size > width - 1 {
            break;
        }
        result.push(c);
        used += size;
    }
    result.push('…');
    result
}
fn color_allowed() -> bool {
    // Donor policy: explicit opt-outs always win over forced styling.
    !std::env::var("NO_COLOR").is_ok_and(|v| !v.is_empty())
        && !std::env::var("TERM").is_ok_and(|v| v.trim().eq_ignore_ascii_case("dumb"))
        && !["CLICOLOR", "FORCE_COLOR"]
            .iter()
            .any(|key| std::env::var(key).is_ok_and(|v| v.trim() == "0"))
}
struct TerminalGuard;
impl TerminalGuard {
    fn enter() -> io::Result<Self> {
        // Actual donor Run rejects redirected streams BEFORE any controls/network.
        if !io::stdin().is_terminal() || !io::stdout().is_terminal() {
            return Err(io::Error::other(
                "TUI requires an interactive terminal on stdin and stdout; use --status or --command for scripts",
            ));
        }
        terminal::enable_raw_mode()?;
        let owner = Self;
        execute!(io::stdout(), EnterAlternateScreen, cursor::Hide)?;
        Ok(owner) // On entry failure, Drop still restores raw mode and cursor.
    }
}
impl Drop for TerminalGuard {
    fn drop(&mut self) {
        if color_allowed() {
            let _ = execute!(io::stdout(), ResetColor);
        }
        let _ = execute!(io::stdout(), cursor::Show, LeaveAlternateScreen);
        let _ = terminal::disable_raw_mode();
    }
}
#[derive(Clone, Copy, PartialEq, Eq)]
enum Page {
    Overview,
    Table,
    Events,
    Command,
    Help,
}
#[derive(Clone, Debug)]
enum Work {
    Status,
    Cells {
        ids: Vec<u8>,
        state: &'static str,
        peer: String,
    },
    Command {
        words: Vec<String>,
        peer: String,
    },
}
struct Reply {
    status: Result<DeviceStatus, String>,
    operation: Option<Result<String, String>>,
}
pub struct Model {
    status: Option<DeviceStatus>,
    live: bool,
    busy: bool,
    incompatible: bool,
    page: Page,
    selection: ui::cell_table::Selection,
    cursor: u8,
    find: Option<String>,
    find_error: String,
    command: Option<String>,
    output: Vec<String>,
    scroll: usize,
    notice: String,
    log: diagnostics::EventLog,
}
impl Default for Model {
    fn default() -> Self {
        Self {
            status: None,
            live: false,
            busy: false,
            incompatible: false,
            page: Page::Overview,
            selection: Default::default(),
            cursor: 1,
            find: None,
            find_error: String::new(),
            command: None,
            output: Vec::new(),
            scroll: 0,
            notice: String::new(),
            log: Default::default(),
        }
    }
}
fn peer_key(s: &DeviceStatus) -> String {
    // Actual board/boot changes revoke drafts even if hostname/manifest match.
    format!(
        "{}:{}:{}:{}:{}",
        s.board_id,
        s.boot_id,
        s.http_peer_fencing,
        s.hostname,
        serde_json::to_string(&s.capabilities).unwrap_or_default()
    )
}
fn fenced(s: &DeviceStatus) -> bool {
    crate::peer_http::Pin::from_status(s).is_some()
}
fn pinned_client(s: &DeviceStatus) -> Result<reqwest::blocking::Client, String> {
    crate::peer_http::pinned_client(s)
}
fn read_only_command(words: &[&str]) -> bool {
    matches!(
        words,
        ["status" | "info" | "telemetry"]
            | ["device", "name" | "identity"]
            | ["auth", "info"]
            | ["firmware", "status"]
            | ["avr", "contracts" | "settings"]
    )
    // AVR status/panel use POST to the raw contract. They are not HTTP reads.
}
impl Model {
    fn table(&self) -> bool {
        self.live
            && self
                .status
                .as_ref()
                .and_then(|s| s.capabilities.as_ref())
                .is_some_and(|m| m.table())
    }
    fn receive(&mut self, reply: Reply) {
        self.busy = false;
        let mut peer_changed = false;
        match reply.status {
            Ok(status) => {
                self.incompatible=false;
                if self
                    .status
                    .as_ref()
                    .is_some_and(|s| peer_key(s) != peer_key(&status))
                {
                    peer_changed = true;
                    self.selection.clear();
                    self.cursor = 1;
                    self.find = None;
                    self.find_error.clear();
                    self.command = None;
                    self.output.clear();
                    self.scroll = 0;
                    self.log.reset_sources("TUI GET /api/status"); // Different peer is not recovery.
                }
                self.status = Some(status);
                self.live = true;
                self.log.recover("TUI GET /api/status");
            }
            Err(error) => {
                self.live = false;
                self.incompatible=crate::peer_identity::unsupported(&error);
                self.log.failure("TUI GET /api/status", &error);
            }
        }
        if !self.table() && self.page == Page::Table {
            self.page = Page::Overview;
        }
        if let Some(outcome) = reply.operation {
            if peer_changed {
                // Keep evidence, but never present an old receipt as the new
                // peer's confirmed state or reapply the old command draft.
                self.notice = "Peer changed during operation; previous-session outcome is in Events. Nothing replayed.".into();
                self.log.record(
                    "Previous-session outcome",
                    &match outcome {
                        Ok(message) | Err(message) => message,
                    },
                );
                return;
            }
            match outcome {
                Ok(message) => {
                    self.output = message.lines().take(256).map(safe_line).collect();
                    self.scroll = 0;
                    self.notice =
                        "Command finished; output is on the Commands page. Readback is separate."
                            .into();
                    self.log.record("Operation", &message);
                }
                Err(error) => {
                    self.notice = error.clone();
                    self.log.record("Operation failed", &error);
                }
            }
        }
    }
    fn key(&mut self, key: KeyEvent) -> (bool, Option<Work>) {
        if key.kind != KeyEventKind::Press {
            return (false, None);
        } // Never repeat a mutation.
        let ctrl = key.modifiers.contains(KeyModifiers::CONTROL);
        if ctrl && key.code == KeyCode::Char('c') {
            return (true, None);
        }
        if let Some(draft) = self.command.as_mut() {
            match key.code {
                KeyCode::Esc => self.command = None,
                KeyCode::Backspace => {
                    draft.pop();
                }
                KeyCode::Char(c)
                    if !ctrl && !c.is_control() && draft.len() + c.len_utf8() <= 1024 =>
                {
                    draft.push(c)
                }
                KeyCode::Enter if !self.busy && self.live => {
                    let words: Vec<String> = draft.split_whitespace().map(String::from).collect();
                    if !words.is_empty() {
                        self.command = None;
                        self.page = Page::Command;
                        return (
                            false,
                            Some(Work::Command {
                                words,
                                peer: peer_key(self.status.as_ref().unwrap()),
                            }),
                        );
                    }
                }
                _ => {}
            }
            return (false, None);
        }
        if let Some(draft) = self.find.as_mut() {
            match key.code {
                KeyCode::Esc => {
                    self.find = None;
                    self.find_error.clear();
                }
                KeyCode::Enter => match self.selection.replace_query(draft) {
                    Ok(()) => {
                        if let Some(id) = self.selection.ids.first() {
                            self.cursor = *id;
                        }
                        self.find = None;
                        self.find_error.clear();
                    }
                    Err(error) => self.find_error = error,
                },
                KeyCode::Backspace => {
                    draft.pop();
                    self.find_error.clear();
                }
                KeyCode::Char(c)
                    if !ctrl && !c.is_control() && draft.len() + c.len_utf8() <= 256 =>
                {
                    draft.push(c);
                }
                _ => {}
            }
            return (false, None);
        }
        match key.code {
            KeyCode::Char('q') => return (true, None),
            KeyCode::Char('?') => self.page = Page::Help,
            KeyCode::Char('d') => self.page = Page::Events,
            KeyCode::Char(':') if self.live && !self.busy => {
                self.command = Some(String::new());
                self.page = Page::Command;
            }
            KeyCode::Tab | KeyCode::BackTab => {
                let pages = if self.table() {
                    vec![
                        Page::Overview,
                        Page::Table,
                        Page::Events,
                        Page::Command,
                        Page::Help,
                    ]
                } else {
                    vec![Page::Overview, Page::Events, Page::Command, Page::Help]
                };
                let at = pages.iter().position(|p| *p == self.page).unwrap_or(0);
                let delta = if key.code == KeyCode::BackTab {
                    pages.len() - 1
                } else {
                    1
                };
                self.page = pages[(at + delta) % pages.len()];
            }
            KeyCode::Char('g') if !self.busy => return (false, Some(Work::Status)),
            KeyCode::Char('/') if self.table() && self.page == Page::Table => {
                self.find = Some(ui::cell_search::format_selection(&self.selection.ids));
            }
            KeyCode::Char('a') if ctrl && self.table() && self.page == Page::Table => {
                self.selection.group(1..=80, false)
            }
            KeyCode::Esc => {
                self.selection.clear();
                self.notice.clear();
            }
            KeyCode::Up | KeyCode::PageUp if self.page == Page::Command => {
                self.scroll = self.scroll.saturating_sub(1)
            }
            KeyCode::Down | KeyCode::PageDown if self.page == Page::Command => {
                self.scroll = (self.scroll + 1).min(self.output.len().saturating_sub(1))
            }
            KeyCode::Char(' ') if self.table() && self.page == Page::Table => {
                self.selection.click(self.cursor, true, false)
            }
            KeyCode::Char(c @ ('w' | 'r' | 'o'))
                if self.table() && !self.busy && self.page == Page::Table =>
            {
                if !self.status.as_ref().is_some_and(fenced) {
                    self.notice =
                        "Read-only: firmware must advertise board/boot identity and HTTP fencing"
                            .into();
                    return (false, None);
                }
                if !self.status.as_ref().is_some_and(crate::cell_batch::Request::supported){
                    self.notice="Atomic cell-batch support is not acknowledged; no per-cell fallback".into();
                    return (false,None);
                }
                if self.selection.ids.is_empty() {
                    self.notice = "Select cells first; selection never changes output".into();
                } else {
                    return (
                        false,
                        Some(Work::Cells {
                            ids: self.selection.cells(),
                            state: match c {
                                'w' => "white",
                                'r' => "red",
                                _ => "off",
                            },
                            peer: peer_key(self.status.as_ref().unwrap()),
                        }),
                    );
                }
            }
            code if self.table() && self.page == Page::Table => {
                let navigation = match code {
                    KeyCode::Left => Some(eframe::egui::Key::ArrowLeft),
                    KeyCode::Right => Some(eframe::egui::Key::ArrowRight),
                    KeyCode::Up => Some(eframe::egui::Key::ArrowUp),
                    KeyCode::Down => Some(eframe::egui::Key::ArrowDown),
                    KeyCode::Home => Some(eframe::egui::Key::Home),
                    KeyCode::End => Some(eframe::egui::Key::End),
                    _ => None,
                };
                if let Some(navigation) = navigation {
                    if key.modifiers.contains(KeyModifiers::SHIFT) && self.selection.ids.is_empty()
                    {
                        self.selection.click(self.cursor, false, false);
                    }
                    self.cursor = ui::cell_table::navigate(self.cursor, navigation, ctrl);
                    if key.modifiers.contains(KeyModifiers::SHIFT) {
                        self.selection.click(self.cursor, false, true);
                    }
                }
            }
            _ => {}
        }
        (false, None)
    }
    /// Actual PCController tableWindow: stable, cursor-centered viewport.
    fn window(total: usize, visible: usize, cursor: usize) -> (usize, usize) {
        if total == 0 || visible == 0 {
            return (0, 0);
        }
        if visible >= total {
            return (0, total);
        }
        let start = cursor
            .min(total - 1)
            .saturating_sub(visible / 2)
            .min(total - visible);
        (start, start + visible)
    }
    pub fn lines(&self, width: usize, height: usize) -> Vec<String> {
        if width < 34 || height < 9 {
            return [
                "Resize terminal to at least 34 x 9".into(),
                "q / Ctrl+C: quit".into(),
            ]
            .into_iter()
            .take(height)
            .map(|line: String| fit(&line, width))
            .collect();
        }
        let title = if self.live {
            self.status
                .as_ref()
                .and_then(|s| s.capabilities.as_ref())
                .map(|m| m.title())
                .unwrap_or_else(|| "PakarController · Awaiting capabilities".into())
        } else {
            "PakarController · Device offline".into()
        };
        let active = match self.page {
            Page::Overview => "Overview",
            Page::Table => "Table",
            Page::Events => "Events",
            Page::Command => "Commands",
            Page::Help => "Help",
        };
        let mut lines = vec![
            title,
            format!(
                "{}  | {active}",
                if self.table() {
                    "Overview / Table / Events / Commands / Help"
                } else {
                    "Overview / Events / Commands / Help"
                }
            ),
        ];
        let content_rows = height.saturating_sub(6);
        match self.page {
            Page::Overview => {
                if !self.live {
                    lines.push(
                        "No live controls. Waiting for device status/capability ACKs.".into(),
                    );
                } else if let Some(s) = &self.status {
                    lines.push(format!("Host: {}  Firmware: {}", s.hostname, s.version));
                    if let Some(progress)=crate::firmware_progress::observed(s) {
                        lines.push(progress.label());
                        if let Some(total)=progress.bytes_total {lines.push(format!("{} / {} bytes · this update stage",progress.bytes_done,total));}
                    }
                    if let Some(m) = &s.capabilities {
                        lines.push(format!("Board: {}  Name: {}", m.board_kind, m.name));
                        lines.push(format!(
                            "Acknowledged: {}",
                            m.acknowledged_features.join(", ")
                        ));
                        if m.table() {
                            lines.push(format!(
                                "Mode: {}  Effect: {}  Brightness: {}",
                                s.mode, s.effect, s.brightness
                            ));
                        }
                        if m.has("leds") {
                            lines.push(format!("LED output: {}", match s.led_output.get("enabled").and_then(|v| v.as_bool()) {
                                Some(true) => "On", Some(false) => "Off", None => "Unknown",
                            }));
                            lines.push(format!("Output mode: {}", crate::led_output_mode(s).unwrap_or("Unknown")));
                        }
                        if m.has("display") && s.tm1637_ack {
                            lines.push(format!(
                                "TM1637: {:02X?}  Brightness: {}",
                                s.tm1637_segments, s.tm1637_brightness
                            ));
                        }
                        if m.has("inputs") {
                            lines.push(format!(
                                "Inputs: {:02X} + {}",
                                s.active_inputs,
                                u8::from(s.input_9)
                            ));
                        }
                        if m.has("outputs") {
                            lines.push(format!("Outputs: {:02X}", s.active_outputs));
                        }
                        if m.has("network") {
                            lines.push(format!(
                                "Wi-Fi: {}  RSSI: {}  AP: {}",
                                s.station_connected, s.station_rssi, s.ap_started
                            ));
                        }
                    }
                }
            }
            Page::Table if self.table() => {
                let status = self.status.as_ref().unwrap();
                lines.push(format!(
                    "Find/selection: {} | cursor {}",
                    ui::cell_search::format_selection(&self.selection.ids),
                    self.cursor
                ));
                let (start, end) = Self::window(
                    8,
                    content_rows.saturating_sub(2).min(8),
                    usize::from((self.cursor - 1) / 10),
                );
                if width >= 64 {
                    for row in start..end {
                        let mut line = String::new();
                        for col in 0..10 {
                            let id = (row * 10 + col + 1) as u8;
                            let marker = if self.selection.ids.contains(&id) {
                                '*'
                            } else {
                                ' '
                            };
                            let value =
                                match status.cells.get(usize::from(id - 1)).map(String::as_str) {
                                    Some("white") => 'W',
                                    Some("red") => 'R',
                                    Some("off") => '.',
                                    _ => '?',
                                };
                            line.push_str(&format!(
                                "{}{id:02}{value}{marker} ",
                                if id == self.cursor { '>' } else { ' ' }
                            ));
                        }
                        lines.push(line);
                    }
                } else {
                    let (start, end) = Self::window(
                        80,
                        content_rows.saturating_sub(2),
                        usize::from(self.cursor - 1),
                    );
                    for cell in start..end {
                        let id = (cell + 1) as u8;
                        lines.push(format!(
                            "{} {:02} {} {}",
                            if id == self.cursor { '>' } else { ' ' },
                            id,
                            if self.selection.ids.contains(&id) {
                                '*'
                            } else {
                                ' '
                            },
                            status
                                .cells
                                .get(cell)
                                .map(String::as_str)
                                .unwrap_or("not reported")
                        ));
                    }
                }
                lines.push("Arrows: cursor  Space: toggle  Shift: range  Ctrl+A: all".into());
            }
            Page::Events => {
                lines.push(format!(
                    "{} events / {} repeats / {} active causes",
                    self.log.len(),
                    self.log.repeated_count(),
                    self.log.active_count()
                ));
                for row in self.log.iter().rev().take(content_rows.saturating_sub(1)) {
                    lines.push(format!(
                        "{} x{}: {}",
                        row.kind, row.occurrences, row.message
                    ));
                }
            }
            Page::Command => {
                lines.push(": command palette | Up/Down: output | --help lists commands".into());
                lines.extend(
                    self.output
                        .iter()
                        .skip(self.scroll)
                        .take(content_rows.saturating_sub(1))
                        .cloned(),
                );
            }
            Page::Help => lines.extend(
                [
                    "Tab / Shift+Tab: switch contextual pages",
                    "g: refresh read-only status; d: incidents",
                    "/: Find cell IDs/ranges, Enter: apply selection",
                    "w / r / o: selected cells white / red / off",
                    ": command palette uses the real shared CLI parser",
                    ":led-output on|off|static|effects: output service and mode",
                    "Selection does not write; actions send once, no replay",
                    "q / Ctrl+C: exit, stop unsent work",
                    "Rich device-settings forms remain pending",
                ]
                .map(String::from),
            ),
            _ => {}
        }
        lines.truncate(height - 4);
        while lines.len() < height - 4 {
            lines.push(String::new());
        }
        lines.push(
            self.command
                .as_ref()
                .map(|s| format!(": {s}  (Enter / Esc)"))
                .or_else(|| {
                    self.find
                        .as_ref()
                        .map(|s| format!("Find cells: {s}  (Enter / Esc)"))
                })
                .unwrap_or_else(|| self.notice.clone()),
        );
        lines.push(self.find_error.clone());
        lines.push(if self.busy {
            "Request in flight; no queued commands. Quit stops unsent work.".into()
        } else if self.table() && self.status.as_ref().is_some_and(fenced) {
            "/ Find  w White  r Red  o Off  : Commands  ? Help  q Quit".into()
        } else if self.table() {
            "/ Find  READ-ONLY: board/boot HTTP fencing unavailable  q Quit".into()
        } else {
            "g Refresh  d Events  : Commands  ? Help  q Quit".into()
        });
        lines
            .iter()
            .map(|s| fit(s, width.saturating_sub(1)))
            .collect()
    }
}

fn cell_request(
    client: &reqwest::blocking::Client,
    endpoint: &Endpoint,
    ids: &[u8],
    state: &str,
    peer: &str,
    stop: &AtomicBool,
) -> Result<String, String> {
    let current=blocking_status(client,endpoint)?;
    if peer_key(&current)!=peer{return Err("Selection belongs to a retired peer; nothing sent".into());}
    let pin=crate::peer_http::Pin::from_status(&current).ok_or("Verified peer required")?;
    let request=crate::cell_batch::Request::new(ids,state,"fade")?;
    crate::cell_batch::send(client,endpoint,&request,Some(&pin),Some(stop),None)
}
fn command_allowed(s: &DeviceStatus, words: &[&str]) -> bool {
    if !read_only_command(words) && !fenced(s) {
        return false;
    }
    let Some(m) = s.capabilities.as_ref().filter(|m| m.known()) else {
        return words == ["status"];
    };
    let feature = match words {
        ["status"] => return true,
        ["info" | "telemetry"] => "telemetry",
        ["device", "name" | "identity"] | ["device", "rename" | "label" | "configure", ..] => {
            "identity"
        }
        ["auth", "info" | "change" | "disable", ..] => "network",
        ["avr", ..] => "isp",
        ["firmware", ..] => "ota",
        ["mode", mode] => return !s.programming_latch && m.modes().any(|v| v.id == *mode),
        ["cell", ..] | ["reset"] => return !s.programming_latch && m.table(),
        ["cells", ..] => return crate::cell_batch::Request::supported(s),
        ["snake", ..] => {
            return !s.programming_latch
                && m.table()
                && s.mode == "snake"
                && m.modes().any(|v| v.id == "snake");
        }
        ["brightness" | "effect", ..] => "leds",
        ["led-output", "on" | "off"] => return !s.programming_latch
            && m.has("leds") && s.led_output.get("enabled").and_then(|v| v.as_bool()).is_some(),
        ["led-output", "static" | "effects"] => return !s.programming_latch
            && m.has("leds") && crate::led_output_mode(s).is_some(),
        ["display", "segments", ..] => "text",
        ["display", ..] => "display",
        ["audio", cue] => {
            return !s.programming_latch
                && m.has("buzzer")
                && s.audio_cues.iter().any(|v| v == *cue);
        }
        _ => return false,
    };
    m.has(feature)
        && (!s.programming_latch
            || matches!(
                feature,
                "network" | "identity" | "telemetry" | "isp" | "ota"
            ))
}
fn command_request(
    client: &reqwest::blocking::Client,
    endpoint: &Endpoint,
    words: &[String],
    peer: &str,
    stop: &AtomicBool,
) -> Result<String, String> {
    let current = blocking_status(client, endpoint)?;
    let words: Vec<&str> = words.iter().map(String::as_str).collect();
    if peer_key(&current) != peer || !command_allowed(&current, &words) {
        return Err("Command unavailable: fresh peer capabilities/latch do not allow it; nothing dispatched".into());
    }
    let mut output = String::new();
    if stop.load(Ordering::Acquire) {
        return Err("Stopped after read-only preflight; command not dispatched".into());
    }
    if let ["cells",ids,state,rest @ ..]=words.as_slice(){
        let effect=match rest{[]=>"immediate",[effect]=>effect,_=>return Err("cells IDS STATE [EFFECT]".into())};
        let request=crate::cell_batch::Request::new(&ui::cell_search::parse(ids)?,state,effect)?;
        let pin=crate::peer_http::Pin::from_status(&current).ok_or("Verified peer required")?;
        return crate::cell_batch::send(client,endpoint,&request,Some(&pin),Some(stop),None);
    }
    if read_only_command(&words) {
        crate::text_command_output(client, endpoint, &words, &mut output)?;
    } else {
        // Shared CLI nested requests inherit the SAME epoch; never rebind them
        // after reboot. Admission cancellation cannot retract a dispatched POST.
        crate::text_command_output(&pinned_client(&current)?, endpoint, &words, &mut output)?;
    }
    Ok(output)
}
pub fn run(endpoint: Endpoint) -> Result<(), String> {
    let _terminal = TerminalGuard::enter().map_err(|e| e.to_string())?;
    let stop = Arc::new(AtomicBool::new(false));
    let (send, requests) = mpsc::sync_channel::<Work>(1);
    let (results, receive) = mpsc::sync_channel::<Reply>(1);
    let stopped = stop.clone();
    let worker = thread::spawn(move || {
        let mut endpoint = endpoint;
        let client = match crate::peer_identity::client_builder()
            .no_proxy()
            .timeout(Duration::from_secs(4))
            .build()
        {
            Ok(client) => client,
            Err(error) => {
                let _ = results.send(Reply {
                    status: Err(error.to_string()),
                    operation: None,
                });
                return;
            }
        };
        let mut peer_reporter=crate::client_telemetry::Reporter::new();
        while let Ok(work) = requests.recv() {
            if stopped.load(Ordering::Acquire) {
                break;
            }
            let operation = match work {
                Work::Status => None,
                Work::Cells { ids, state, peer } => Some(cell_request(
                    &client, &endpoint, &ids, state, &peer, &stopped,
                )),
                Work::Command { words, peer } => {
                    let mut result = if stopped.load(Ordering::Acquire) {
                        Err("Stopped before command dispatch".into())
                    } else {
                        command_request(&client, &endpoint, &words, &peer, &stopped)
                    };
                    if result.is_ok() {
                        match words.iter().map(String::as_str).collect::<Vec<_>>().as_slice() {
                            ["device", "rename" | "configure", name, ..] => {
                                if let Ok(name) = crate::identity::normalize_name(name) { endpoint.base = format!("http://{name}.local"); }
                            }
                            ["auth", "change", user] => match std::env::var("PAKAR_NEW_PASSWORD") {
                                Ok(password) => { endpoint.user = (*user).into(); endpoint.password = password; }
                                Err(_) => result = Err("Credentials may have changed; refresh private authentication before continuing".into()),
                            },
                            ["auth", "disable"] => { endpoint.user.clear(); endpoint.password.clear(); },
                            _ => {}
                        }
                    }
                    Some(result)
                }
            };
            let status = if stopped.load(Ordering::Acquire) {
                Err("Terminal stopped; inspect any in-flight operation outcome".into())
            } else {
                blocking_status(&client, &endpoint)
            };
            if !stopped.load(Ordering::Acquire) && peer_reporter.due() && let Ok(status)=&status
                && status.capabilities.as_ref().is_some_and(|c|c.has("telemetry")) && let Ok(pinned)=crate::peer_http::pinned_client(status) {
                peer_reporter.report(&pinned,&endpoint);
            }
            if results.send(Reply { status, operation }).is_err() {
                break;
            }
        }
    });
    let mut model = Model::default();
    let mut next_read = Instant::now();
    let mut backoff = diagnostics::Backoff::new(Duration::from_secs(2), Duration::from_secs(30));
    let mut previous = Vec::new();
    let mut mutation_in_flight = false;
    let color = color_allowed();
    let result = (|| -> io::Result<()> {
        loop {
            if let Ok(reply) = receive.try_recv() {
                mutation_in_flight = false;
                model.receive(reply);
                if model.live {
                    backoff.reset();
                } else {
                    backoff.failed();
                }
                next_read = Instant::now() + backoff.delay();
            }
            if !model.busy && !model.incompatible && Instant::now() >= next_read {
                send.try_send(Work::Status).map_err(io::Error::other)?;
                model.busy = true;
            }
            let (width, height) = terminal::size()?;
            let lines = model.lines(usize::from(width), usize::from(height));
            if lines != previous {
                let mut out = io::stdout();
                queue!(out, cursor::MoveTo(0, 0), Clear(ClearType::All))?;
                for (row, line) in lines.iter().enumerate() {
                    queue!(out, cursor::MoveTo(0, row as u16))?;
                    if color {
                        queue!(
                            out,
                            SetForegroundColor(if row == 0 {
                                Color::Cyan
                            } else if row == lines.len() - 1 {
                                Color::DarkGrey
                            } else {
                                Color::Reset
                            })
                        )?;
                    }
                    queue!(out, Print(line))?;
                    if color {
                        queue!(out, ResetColor)?;
                    }
                }
                out.flush()?;
                previous = lines;
            }
            if event::poll(Duration::from_millis(80))? {
                match event::read()? {
                    Event::Key(key) => {
                        let (quit, work) = model.key(key);
                        if quit {
                            break;
                        }
                        if let Some(work) = work {
                            mutation_in_flight = !matches!(work, Work::Status);
                            send.try_send(work).map_err(io::Error::other)?;
                            model.busy = true;
                        }
                    }
                    Event::Resize(..) => previous.clear(),
                    _ => {}
                }
            }
        }
        Ok(())
    })();
    stop.store(true, Ordering::Release);
    drop(send);
    drop(receive);
    // An in-flight request cannot be unsent. Bounded four-second request timeout;
    // restore terminal first and never await/retry a potentially applied write.
    drop(worker);
    drop(_terminal);
    if mutation_in_flight {
        cli_eprintln!(
            "Terminal closed during an operation: unsent work stopped. An in-flight write may have applied; refresh before retrying."
        );
    }
    result.map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    fn table() -> DeviceStatus {
        serde_json::from_value(serde_json::json!({"version":crate::peer_identity::VERSION,"hostname":"ledboard","cells":vec!["white";80],
        "board_id":"esp8266-aabbccddeeff","boot_id":"123456789abcdef0123456789abcdef0","http_peer_fencing":true,
        "capabilities":{"board_kind":"table","name":"Table","acknowledged_features":["table","cell-batch"],"table":{"columns":10,"rows":8,"pixels_per_cell":6}}})).unwrap()
    }
    fn press(code: KeyCode) -> KeyEvent {
        KeyEvent::new(code, KeyModifiers::NONE)
    }
    #[test]
    fn donor_text_safety_preserves_unicode_not_peer_terminal_commands() {
        assert_eq!(safe_line("\x1b[31mسلام\x1b[0m\r\n board\t1"), "سلام board 1");
        assert_eq!(
            safe_line("a\x1b]52;c;clipboard\x07b\x1b]0;title\x1b\\c"),
            "abc"
        );
        assert_eq!(safe_line("a\u{9b}2Jb\u{202e}c\u{7}d"), "ab�c�d");
        assert_eq!(safe_line("a\x1bPsecret\x1b\\b"), "ab");
        assert_eq!(fit("你好123", 4), "你…");
        assert_eq!(fit("a", 0), "");
    }
    #[test]
    fn offline_and_unacknowledged_peers_have_no_table_actions() {
        let mut m = Model::default();
        assert!(!m.table());
        assert!(!m.lines(80, 24).join("\n").contains("/ Table /"));
        m.receive(Reply {
            status: Ok(Default::default()),
            operation: None,
        });
        assert!(!m.table());
        m.page = Page::Table;
        assert!(m.key(press(KeyCode::Char('r'))).1.is_none());
    }
    #[test]
    fn output_service_command_requires_fresh_advertised_known_state() {
        let mut s = table();
        s.capabilities.as_mut().unwrap().acknowledged_features.push("leds".into());
        assert!(!command_allowed(&s, &["led-output", "on"]));
        for enabled in [true, false] {
            s.led_output = serde_json::json!({"enabled":enabled});
            assert!(command_allowed(&s, &["led-output", "on"]));
            assert!(command_allowed(&s, &["led-output", "off"]));
        }
        assert!(!command_allowed(&s, &["led-output", "toggle"]));
        assert!(!command_allowed(&s, &["led-output", "static"]));
        for mode in ["static", "effects"] {
            s.led_output["mode"] = mode.into();
            assert!(command_allowed(&s, &["led-output", "static"]));
            assert!(command_allowed(&s, &["led-output", "effects"]));
        }
        s.led_output["mode"] = "future".into();
        assert!(!command_allowed(&s, &["led-output", "effects"]));
        s.programming_latch = true;
        assert!(!command_allowed(&s, &["led-output", "on"]));
        s.programming_latch = false;
        s.boot_id.clear();
        assert!(!command_allowed(&s, &["led-output", "off"]));
    }
    #[test]
    fn shared_find_selection_and_arrow_navigation_never_write() {
        let mut m = Model::default();
        m.receive(Reply {
            status: Ok(table()),
            operation: None,
        });
        m.page = Page::Table;
        m.find = Some("۱،٢,۳".into());
        assert!(m.key(press(KeyCode::Enter)).1.is_none());
        assert_eq!(ui::cell_search::format_selection(&m.selection.ids), "1–3");
        assert!(m.key(press(KeyCode::Down)).1.is_none());
        // Shared visual navigation keeps the column: the next row pair is21,
        // while11 is the adjacent half-staggered column to the right of1.
        assert_eq!(m.cursor, 21);
        assert_eq!(m.selection.cells(), [1, 2, 3]);
        if let Some(Work::Cells { ids, state, .. }) = m.key(press(KeyCode::Char('r'))).1 {
            assert_eq!(ids, [1, 2, 3]);
            assert_eq!(state, "red");
        } else {
            panic!("missing explicit action")
        }
        m.busy = true;
        assert!(m.key(press(KeyCode::Char('w'))).1.is_none());
        m.busy = false;
        let mut repeat = press(KeyCode::Char('w'));
        repeat.kind = KeyEventKind::Repeat;
        assert!(m.key(repeat).1.is_none());
        m.status.as_mut().unwrap().capabilities.as_mut().unwrap().acknowledged_features.retain(|feature|feature!="cell-batch");
        assert!(m.key(press(KeyCode::Char('r'))).1.is_none());
        assert!(!command_allowed(m.status.as_ref().unwrap(),&["cells","1-3","red"]));
    }
    #[test]
    fn loss_and_peer_change_revoke_controls_without_false_recovery() {
        let mut m = Model::default();
        m.receive(Reply {
            status: Ok(table()),
            operation: None,
        });
        m.page = Page::Table;
        m.selection.click(1, false, false);
        for _ in 0..100 {
            m.receive(Reply {
                status: Err("DNS failed".into()),
                operation: None,
            });
        }
        assert!(!m.table());
        assert_eq!(m.log.len(), 1);
        assert_eq!(m.log.repeated_count(), 99);
        assert!(m.key(press(KeyCode::Char('r'))).1.is_none());
        let mut next = table();
        next.hostname = "other".into();
        m.receive(Reply {
            status: Ok(next),
            operation: None,
        });
        assert!(m.selection.ids.is_empty());
    }
    #[test]
    fn same_hostname_reboot_revokes_drafts_and_old_receipts() {
        let mut m = Model::default();
        m.receive(Reply {
            status: Ok(table()),
            operation: None,
        });
        m.selection.click(1, false, false);
        m.command = Some("cell 1 red".into());
        m.find = Some("1".into());
        m.output = vec!["old output".into()];
        let mut reboot = table();
        reboot.boot_id = "223456789abcdef0123456789abcdef0".into();
        assert_ne!(peer_key(&table()), peer_key(&reboot));
        m.receive(Reply {
            status: Ok(reboot),
            operation: Some(Ok("accepted by old session".into())),
        });
        assert!(
            m.selection.ids.is_empty()
                && m.command.is_none()
                && m.find.is_none()
                && m.output.is_empty()
        );
        assert!(m.notice.contains("Previous-session") || m.notice.contains("previous-session"));
        let mut replacement = table();
        replacement.board_id = "esp8266-aabbccddee00".into();
        assert_ne!(peer_key(&table()), peer_key(&replacement));
    }
    #[test]
    fn missing_invalid_or_unacknowledged_fences_allow_only_reads() {
        for field in ["board", "boot", "zero", "uppercase", "header", "ack"] {
            let mut s = table();
            match field {
                "board" => s.board_id.clear(),
                "boot" => s.boot_id.clear(),
                "zero" => s.boot_id = "0".repeat(32),
                "uppercase" => s.boot_id = "A".repeat(32),
                "header" => s.board_id = "bad\r\nheader".into(),
                _ => s.http_peer_fencing = false,
            }
            assert!(!fenced(&s));
            assert!(command_allowed(&s, &["status"]));
            assert!(!command_allowed(&s, &["cell", "1", "red"]));
            let mut m = Model::default();
            m.receive(Reply {
                status: Ok(s),
                operation: None,
            });
            m.page = Page::Table;
            m.selection.click(1, false, false);
            assert!(m.key(press(KeyCode::Char('r'))).1.is_none());
        }
    }
    #[test]
    fn actual_donor_table_window_keeps_cursor_in_resized_viewport() {
        for width in 0..34 {
            for height in 0..9 {
                let lines = Model::default().lines(width, height);
                assert!(lines.len() <= height);
                assert!(lines.iter().all(|line| {
                    line.chars().map(|c| c.width().unwrap_or(0)).sum::<usize>() <= width
                }));
            }
        }
        for total in 1..=80 {
            for visible in 1..=80 {
                for cursor in 0..total {
                    let (start, end) = Model::window(total, visible, cursor);
                    assert!(
                        start <= cursor && cursor < end && end <= total && end - start <= visible
                    );
                }
            }
        }
        let mut m = Model::default();
        m.receive(Reply {
            status: Ok(table()),
            operation: None,
        });
        m.page = Page::Table;
        m.cursor = 80;
        for (width, height) in [(34, 9), (60, 16), (80, 24), (140, 50)] {
            let lines = m.lines(width, height);
            assert!(lines.len() <= height);
            assert!(lines
                .iter()
                .all(|s| s.chars().map(|c| c.width().unwrap_or(0)).sum::<usize>() < width));
        }
    }
    #[test]
    fn command_palette_uses_individual_ack_and_current_modes() {
        let mut s = table();
        assert!(command_allowed(&s, &["cell", "1", "red"]));
        assert!(!command_allowed(&s, &["display", "1"]));
        assert!(!command_allowed(&s, &["mode", "snake"]));
        assert!(!command_allowed(&s, &["audio", "welcome"]));
        s.capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .push("buzzer".into());
        s.audio_cues.push("welcome".into());
        assert!(command_allowed(&s, &["audio", "welcome"]));
        s.programming_latch = true;
        assert!(!command_allowed(&s, &["audio", "welcome"]));
        assert!(!command_allowed(&s, &["cell", "1", "red"]));
        let mut m = Model::default();
        assert!(m.key(press(KeyCode::Char(':'))).1.is_none());
        assert!(m.command.is_none());
        m.receive(Reply {
            status: Ok(table()),
            operation: None,
        });
        m.key(press(KeyCode::Char(':')));
        for c in "device name".chars() {
            m.key(press(KeyCode::Char(c)));
        }
        if let Some(Work::Command { words, .. }) = m.key(press(KeyCode::Enter)).1 {
            assert_eq!(words, ["device", "name"]);
        } else {
            panic!("missing command")
        }
    }
    fn fixture_blocking_stream(stream: &std::net::TcpStream) {
        // Winsock accept inherits the listener's nonblocking mode. A read
        // timeout alone does not restore blocking reads, so delayed request
        // bytes otherwise make the fixture panic with WSAEWOULDBLOCK.
        stream.set_nonblocking(false).unwrap();
        stream.set_read_timeout(Some(Duration::from_secs(2))).unwrap();
        stream.set_write_timeout(Some(Duration::from_secs(2))).unwrap();
    }
    #[test]
    fn fixture_reads_delayed_input_after_nonblocking_accept() {
        use std::io::{BufRead, BufReader};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let (ready, wait) = mpsc::channel();
        let worker = thread::spawn(move || {
            let mut client = std::net::TcpStream::connect(address).unwrap();
            wait.recv_timeout(Duration::from_secs(2)).unwrap();
            thread::sleep(Duration::from_millis(20));
            client.write_all(b"GET /api/status HTTP/1.1\r\n").unwrap();
        });
        let (stream, _) = listener.accept().unwrap();
        // Force the Windows-inherited mode on every host so this regression
        // also exercises a real WouldBlock boundary on Linux/macOS runners.
        stream.set_nonblocking(true).unwrap();
        fixture_blocking_stream(&stream);
        ready.send(()).unwrap();
        let mut line = String::new();
        BufReader::new(stream).read_line(&mut line).unwrap();
        assert_eq!(line, "GET /api/status HTTP/1.1\r\n");
        worker.join().unwrap();
    }
    fn fixture(
        replies: Vec<serde_json::Value>,
    ) -> (Endpoint, mpsc::Receiver<String>, thread::JoinHandle<()>) {
        use std::io::{BufRead, BufReader, Read};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        listener.set_nonblocking(true).unwrap();
        let endpoint = Endpoint {
            base: format!("http://{}", listener.local_addr().unwrap()),
            user: "fixture".into(),
            password: "private-fixture".into(),
        };
        let (send, receive) = mpsc::channel();
        let worker = thread::spawn(move || {
            for reply in replies {
                let until = Instant::now() + Duration::from_secs(6);
                let mut stream = loop {
                    match listener.accept() {
                        Ok((s, _)) => break s,
                        Err(e)
                            if e.kind() == io::ErrorKind::WouldBlock && Instant::now() < until =>
                        {
                            thread::sleep(Duration::from_millis(5))
                        }
                        Err(e) => panic!("fixture accept: {e}"),
                    }
                };
                fixture_blocking_stream(&stream);
                let mut reader = BufReader::new(stream.try_clone().unwrap());
                let mut first = String::new();
                reader.read_line(&mut first).unwrap();
                let mut length = 0;
                let mut headers = String::new();
                loop {
                    let mut line = String::new();
                    reader.read_line(&mut line).unwrap();
                    if line == "\r\n" {
                        break;
                    }
                    headers.push_str(&line);
                    if let Some((key, value)) = line.split_once(':') {
                        if key.eq_ignore_ascii_case("content-length") {
                            length = value.trim().parse::<usize>().unwrap();
                        }
                    }
                }
                let mut body = vec![0; length];
                reader.read_exact(&mut body).unwrap();
                send.send(
                    first.trim().to_owned()
                        + " "
                        + std::str::from_utf8(&body).unwrap()
                        + "\n"
                        + &headers,
                )
                .unwrap();
                let body = reply.to_string();
                let code = reply["_http_status"].as_u64().unwrap_or(200);
                write!(stream,"HTTP/1.1 {code} Fixture\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",body.len()).unwrap();
            }
        });
        (endpoint, receive, worker)
    }
    #[test]
    fn actual_http_batch_stops_on_changed_peer_and_false_receipt_without_replay() {
        let s = table();
        let mut changed = s.clone();
        changed.boot_id = "223456789abcdef0123456789abcdef0".into();
        let (endpoint, requests, worker) = fixture(vec![
            serde_json::to_value(&s).unwrap(),
            serde_json::to_value(&s).unwrap(),
            serde_json::json!({"accepted":true,"count":2}),
            serde_json::to_value(changed).unwrap(),
        ]);
        let client = reqwest::blocking::Client::builder()
            .no_proxy()
            .timeout(Duration::from_secs(2))
            .build()
            .unwrap();
        let result = cell_request(
            &client,
            &endpoint,
            &[1, 2],
            "red",
            &peer_key(&s),
            &AtomicBool::new(false),
        );
        worker.join().unwrap();
        let requests = requests.iter().collect::<Vec<_>>();
        assert_eq!(requests.len(), 4);
        assert!(requests[0].starts_with("GET /api/status "));
        assert!(requests[2].starts_with("POST /api/cells "));
        assert!(requests[2].contains("ids=1%2C2&state=red&effect=fade"));
        assert!(requests[2].contains("x-pakar-board: esp8266-aabbccddeeff\r\n"));
        assert!(requests[2].contains("x-pakar-boot: 123456789abcdef0123456789abcdef0\r\n"));
        assert!(result.unwrap_err().contains("same-board readback"));
        let (endpoint, requests, worker) = fixture(vec![
            serde_json::to_value(&s).unwrap(),
            serde_json::to_value(&s).unwrap(),
            serde_json::json!({"accepted":false}),
        ]);
        assert!(cell_request(
            &client,
            &endpoint,
            &[1, 2],
            "red",
            &peer_key(&s),
            &AtomicBool::new(false)
        )
        .unwrap_err()
        .contains("not acknowledged"));
        worker.join().unwrap();
        assert_eq!(requests.iter().count(), 3);
    }
    #[test]
    fn server_reboot_between_preflight_and_post_stops_without_replay() {
        // Native HTTP wire path plus production C++ accepts() tests: a server
        // that rebooted after GET rejects the old pinned POST with 409.
        let s = table();
        let (endpoint, requests, worker) = fixture(vec![
            serde_json::to_value(&s).unwrap(),
            serde_json::to_value(&s).unwrap(),
            serde_json::json!({"_http_status":409,"error":"peer_changed","accepted":false}),
        ]);
        let client = reqwest::blocking::Client::builder()
            .no_proxy()
            .build()
            .unwrap();
        let error = cell_request(
            &client,
            &endpoint,
            &[1, 2],
            "red",
            &peer_key(&s),
            &AtomicBool::new(false),
        )
        .unwrap_err();
        worker.join().unwrap();
        assert!(error.contains("HTTP 409"));
        let requests: Vec<_> = requests.iter().collect();
        assert_eq!(requests.len(), 3);
        assert!(requests[2].contains("x-pakar-boot: "));
    }
    #[test]
    fn captured_shared_cli_command_is_read_only_and_has_no_output_side_effects() {
        let mut s = table();
        s.capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .push("identity".into());
        let (endpoint, requests, worker) = fixture(vec![
            serde_json::to_value(&s).unwrap(),
            serde_json::json!({"hostname":"ledboard","display_name":"Fixture"}),
        ]);
        let client = reqwest::blocking::Client::builder()
            .no_proxy()
            .timeout(Duration::from_secs(2))
            .build()
            .unwrap();
        let result = command_request(
            &client,
            &endpoint,
            &["device".into(), "name".into()],
            &peer_key(&s),
            &AtomicBool::new(false),
        )
        .unwrap();
        worker.join().unwrap();
        assert!(result.contains("ledboard"));
        assert!(requests.iter().all(|r| r.starts_with("GET ")));
        let (endpoint, requests, worker) = fixture(vec![serde_json::to_value(&s).unwrap()]);
        assert!(command_request(
            &client,
            &endpoint,
            &["device".into(), "name".into()],
            &peer_key(&s),
            &AtomicBool::new(true)
        )
        .unwrap_err()
        .contains("not dispatched"));
        worker.join().unwrap();
        assert_eq!(requests.iter().count(), 1);
    }
    #[test]
    fn shared_cli_mutation_inherits_pinned_epoch_headers() {
        let s = table();
        let (endpoint, requests, worker) = fixture(vec![
            serde_json::to_value(&s).unwrap(),
            serde_json::json!({"accepted":true}),
        ]);
        let client = reqwest::blocking::Client::builder()
            .no_proxy()
            .build()
            .unwrap();
        command_request(
            &client,
            &endpoint,
            &["cell".into(), "1".into(), "red".into()],
            &peer_key(&s),
            &AtomicBool::new(false),
        )
        .unwrap();
        worker.join().unwrap();
        let requests: Vec<_> = requests.iter().collect();
        assert_eq!(requests.len(), 2);
        assert!(requests[1].starts_with("POST /api/cell "));
        assert!(requests[1].contains("x-pakar-board: esp8266-aabbccddeeff\r\n"));
        assert!(requests[1].contains("x-pakar-boot: 123456789abcdef0123456789abcdef0\r\n"));
    }
}
