use std::{
    sync::mpsc::{self, Receiver, Sender},
    thread,
    time::{Duration, Instant},
};

use eframe::egui::{self, Color32, RichText, Theme};
use reqwest::blocking::Client;
use serde::Deserialize;

#[derive(Clone)]
struct Endpoint {
    base: String,
    user: String,
    password: String,
}

#[derive(Debug, Clone, Deserialize)]
struct DeviceStatus {
    hostname: String,
    ip: String,
    mode: String,
    avr_online: bool,
    winner: u8,
    selected_cell: u8,
    brightness: u8,
    pixels: u16,
    #[serde(default)]
    esp_uart_errors: u16,
    #[serde(default)]
    avr_uart_errors: u16,
    cells: Vec<String>,
}

enum Command {
    Status(Endpoint),
    Mode(Endpoint, &'static str),
    Cell(Endpoint, u8, &'static str),
    Reset(Endpoint),
    Brightness(Endpoint, u8),
}

enum Event {
    Status(DeviceStatus),
    Accepted(String),
    Failed(String),
}

fn request(client: &Client, command: Command) -> Result<Event, String> {
    let (endpoint, request) = match command {
        Command::Status(endpoint) => {
            let url = format!("{}/api/v1/status", endpoint.base.trim_end_matches('/'));
            let request = client.get(url);
            (endpoint, request)
        }
        Command::Mode(endpoint, mode) => {
            let url = format!("{}/api/v1/mode", endpoint.base.trim_end_matches('/'));
            let request = client.post(url).form(&[("value", mode)]);
            (endpoint, request)
        }
        Command::Cell(endpoint, id, state) => {
            let url = format!("{}/api/v1/cell", endpoint.base.trim_end_matches('/'));
            let id = id.to_string();
            let request =
                client
                    .post(url)
                    .form(&[("id", id.as_str()), ("state", state), ("effect", "fade")]);
            (endpoint, request)
        }
        Command::Reset(endpoint) => {
            let url = format!("{}/api/v1/game/reset", endpoint.base.trim_end_matches('/'));
            let request = client.post(url);
            (endpoint, request)
        }
        Command::Brightness(endpoint, brightness) => {
            let url = format!("{}/api/v1/brightness", endpoint.base.trim_end_matches('/'));
            let value = brightness.to_string();
            let request = client.post(url).form(&[("value", value.as_str())]);
            (endpoint, request)
        }
    };
    let response = request
        .basic_auth(endpoint.user, Some(endpoint.password))
        .send()
        .map_err(|error| error.to_string())?;
    let status = response.status();
    if !status.is_success() {
        let detail = response.text().unwrap_or_default();
        return Err(format!("HTTP {status}: {detail}"));
    }
    if status.as_u16() == 200 && response.url().path().ends_with("/status") {
        return response
            .json::<DeviceStatus>()
            .map(Event::Status)
            .map_err(|error| error.to_string());
    }
    Ok(Event::Accepted(format!(
        "Device accepted the command ({status})"
    )))
}

fn worker(commands: Receiver<Command>, events: Sender<Event>, context: egui::Context) {
    let client = Client::builder()
        .timeout(Duration::from_secs(3))
        .build()
        .expect("HTTP client");
    while let Ok(command) = commands.recv() {
        let event = request(&client, command).unwrap_or_else(Event::Failed);
        if events.send(event).is_err() {
            break;
        }
        context.request_repaint();
    }
}

struct PakarControllerApp {
    base: String,
    user: String,
    password: String,
    status: Option<DeviceStatus>,
    brightness: u8,
    message: String,
    command_tx: Sender<Command>,
    event_rx: Receiver<Event>,
    last_poll: Instant,
}

impl PakarControllerApp {
    fn new(context: &eframe::CreationContext<'_>) -> Self {
        install_pealayer_style(&context.egui_ctx);
        let (command_tx, command_rx) = mpsc::channel();
        let (event_tx, event_rx) = mpsc::channel();
        let egui_context = context.egui_ctx.clone();
        thread::Builder::new()
            .name("pakarcontroller-http".into())
            .spawn(move || worker(command_rx, event_tx, egui_context))
            .expect("controller worker");
        Self {
            base: "http://192.168.4.1".into(),
            user: "admin".into(),
            password: "pakarcontroller".into(),
            status: None,
            brightness: 48,
            message: "Waiting for the controller".into(),
            command_tx,
            event_rx,
            last_poll: Instant::now() - Duration::from_secs(2),
        }
    }

    fn endpoint(&self) -> Endpoint {
        Endpoint {
            base: self.base.clone(),
            user: self.user.clone(),
            password: self.password.clone(),
        }
    }

    fn send(&mut self, command: Command) {
        if self.command_tx.send(command).is_err() {
            self.message = "Controller worker stopped".into();
        }
    }

    fn poll(&mut self) {
        if self.last_poll.elapsed() >= Duration::from_secs(1) {
            self.last_poll = Instant::now();
            self.send(Command::Status(self.endpoint()));
        }
        while let Ok(event) = self.event_rx.try_recv() {
            match event {
                Event::Status(status) => {
                    self.brightness = status.brightness;
                    self.message = "Live state received".into();
                    self.status = Some(status);
                }
                Event::Accepted(message) => {
                    self.message = message;
                    self.last_poll = Instant::now() - Duration::from_secs(2);
                }
                Event::Failed(error) => self.message = error,
            }
        }
    }

    fn cell_color(state: &str) -> Color32 {
        match state {
            "red" => Color32::from_rgb(218, 54, 58),
            "off" => Color32::from_rgb(45, 48, 54),
            _ => Color32::from_rgb(235, 235, 225),
        }
    }

    fn next_cell_state(state: &str) -> &'static str {
        match state {
            "white" => "red",
            "red" => "off",
            _ => "white",
        }
    }
}

impl eframe::App for PakarControllerApp {
    fn ui(&mut self, ui: &mut egui::Ui, _frame: &mut eframe::Frame) {
        self.poll();
        ui.ctx().request_repaint_after(Duration::from_millis(250));
        ui.horizontal_wrapped(|ui| {
            ui.heading("PakarController");
            ui.separator();
            ui.label("Device");
            ui.text_edit_singleline(&mut self.base);
            ui.label("User");
            ui.text_edit_singleline(&mut self.user);
            ui.label("Password");
            ui.add(egui::TextEdit::singleline(&mut self.password).password(true));
            if ui.button("Refresh").clicked() {
                self.last_poll = Instant::now() - Duration::from_secs(2);
            }
        });
        ui.separator();

        let snapshot = self.status.clone();
        ui.horizontal_wrapped(|ui| {
            if ui.button("80-cell mode").clicked() {
                self.send(Command::Mode(self.endpoint(), "cell"));
            }
            if ui.button("First button wins").clicked() {
                self.send(Command::Mode(self.endpoint(), "first-wins"));
            }
            if ui.button("Reset game").clicked() {
                self.send(Command::Reset(self.endpoint()));
            }
            if ui
                .add(egui::Slider::new(&mut self.brightness, 1..=255).text("Brightness"))
                .changed()
            {
                self.send(Command::Brightness(self.endpoint(), self.brightness));
            }
        });
        ui.separator();

        if let Some(status) = snapshot {
            ui.horizontal_wrapped(|ui| {
                ui.label(RichText::new(&status.hostname).strong());
                ui.label(format!(
                    "{} • {} pixels • mode {}",
                    status.ip, status.pixels, status.mode
                ));
                ui.label(format!(
                    "UART errors ESP {} / AVR {}",
                    status.esp_uart_errors, status.avr_uart_errors
                ));
                ui.colored_label(
                    if status.avr_online {
                        Color32::LIGHT_GREEN
                    } else {
                        Color32::LIGHT_RED
                    },
                    if status.avr_online {
                        "AVR online"
                    } else {
                        "AVR offline"
                    },
                );
                if status.winner != 0 {
                    ui.colored_label(Color32::GOLD, format!("Winner: {}", status.winner));
                }
            });
            ui.label(format!("Selected cell: {}", status.selected_cell));
            ui.add_space(8.0);
            egui::Grid::new("cells").spacing([5.0, 5.0]).show(ui, |ui| {
                for index in 0..80_usize {
                    let state = status.cells.get(index).map(String::as_str).unwrap_or("off");
                    let text_color = if state == "white" {
                        Color32::BLACK
                    } else {
                        Color32::WHITE
                    };
                    let response = ui.add_sized(
                        [48.0, 34.0],
                        egui::Button::new(RichText::new((index + 1).to_string()).color(text_color))
                            .fill(Self::cell_color(state)),
                    );
                    if response.clicked() {
                        self.send(Command::Cell(
                            self.endpoint(),
                            (index + 1) as u8,
                            Self::next_cell_state(state),
                        ));
                    }
                    if (index + 1) % 10 == 0 {
                        ui.end_row();
                    }
                }
            });
        } else {
            ui.label("Connect to the PakarController access point, then refresh.");
        }
        ui.separator();
        ui.label(&self.message);
    }
}

// Adapted from Pealayer's MIT-licensed native theme strategy: install both
// palettes up front and let egui follow the Windows system theme live.
fn install_pealayer_style(context: &egui::Context) {
    context.set_theme(egui::ThemePreference::System);
    let accent = Color32::from_rgb(51, 132, 255);
    let mut dark = egui::Visuals::dark();
    dark.panel_fill = Color32::from_rgb(32, 32, 32);
    dark.window_fill = Color32::from_rgb(36, 36, 36);
    dark.selection.bg_fill = accent;
    let mut light = egui::Visuals::light();
    light.panel_fill = Color32::from_rgb(243, 243, 243);
    light.window_fill = Color32::from_rgb(250, 250, 250);
    light.selection.bg_fill = accent;
    context.set_visuals_of(Theme::Dark, dark);
    context.set_visuals_of(Theme::Light, light);
}

fn main() -> eframe::Result {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([980.0, 720.0]),
        ..Default::default()
    };
    eframe::run_native(
        "PakarController",
        options,
        Box::new(|context| Ok(Box::new(PakarControllerApp::new(context)))),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cell_states_cycle_without_inventing_device_state() {
        assert_eq!(PakarControllerApp::next_cell_state("white"), "red");
        assert_eq!(PakarControllerApp::next_cell_state("red"), "off");
        assert_eq!(PakarControllerApp::next_cell_state("off"), "white");
    }
}
