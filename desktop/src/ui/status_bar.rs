//! Pealayer's ordered status widgets, fixed slots, contextual actions and
//! authoritative output mirror, adapted to Pakar's acknowledged board features.
use super::{cell_table::compact_menu, dropdown, i18n, icons};
use eframe::egui;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Items {
    pub connection: bool,
    pub telemetry: bool,
    pub message: bool,
    pub status_rgb: bool,
    pub events: bool,
    pub progress: bool,
    pub network: bool,
    pub preferences: bool,
    pub order: Vec<String>,
    pub trailing: Vec<String>,
}
impl Default for Items {
    fn default() -> Self {
        Self {
            connection: true,
            telemetry: true,
            message: true,
            status_rgb: true,
            events: true,
            progress: true,
            network: true,
            preferences: true,
            order: Widget::ALL.iter().map(|widget| widget.key().into()).collect(),
            trailing: vec!["progress".into(), "events".into(), "message".into(), "preferences".into()],
        }
    }
}
impl<'de> Deserialize<'de> for Items {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        #[derive(Default, Deserialize)]
        #[serde(default)]
        struct Stored {
            connection: Option<bool>, telemetry: Option<bool>, message: Option<bool>,
            status_rgb: Option<bool>, events: Option<bool>, progress: Option<bool>,
            network: Option<bool>, preferences: Option<bool>,
            order: serde_json::Value, trailing: Option<Vec<String>>,
        }
        let stored = Stored::deserialize(deserializer)?;
        let mut value = Self::default();
        value.connection = stored.connection.unwrap_or(value.connection);
        value.telemetry = stored.telemetry.unwrap_or(value.telemetry);
        value.message = stored.message.unwrap_or(value.message);
        value.status_rgb = stored.status_rgb.unwrap_or(value.status_rgb);
        value.events = stored.events.unwrap_or(value.events);
        value.progress = stored.progress.unwrap_or(value.progress);
        // Network used to be part of the single telemetry widget. A hidden
        // legacy widget must stay hidden, without tying future child toggles.
        value.network = stored.network.unwrap_or(value.telemetry);
        value.preferences = stored.preferences.unwrap_or(value.preferences);
        value.order = stored.order.as_array().map(|values| values.iter()
            .filter_map(|item| item.as_str().map(str::to_owned)).collect()).unwrap_or_default();
        value.order = value.ordered().iter().map(|widget| widget.key().into()).collect();
        if let Some(trailing) = stored.trailing {
            value.trailing = Widget::ALL.iter().filter(|widget| trailing.iter().any(|key| key == widget.key()))
                .map(|widget| widget.key().into()).collect();
        }
        Ok(value)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Widget { Connection, StatusRgb, Telemetry, Network, Progress, Events, Message, Preferences }
impl Widget {
    const ALL: [Self; 8] = [Self::Connection, Self::StatusRgb, Self::Telemetry, Self::Network,
        Self::Progress, Self::Events, Self::Message, Self::Preferences];
    fn key(self) -> &'static str { match self {
        Self::Connection => "connection", Self::StatusRgb => "status_rgb", Self::Telemetry => "telemetry",
        Self::Network => "network", Self::Progress => "progress", Self::Events => "events",
        Self::Message => "message", Self::Preferences => "preferences",
    } }
    fn label(self) -> &'static str { match self {
        Self::Connection => "Connection", Self::StatusRgb => "Physical status RGB", Self::Telemetry => "Hardware monitor",
        Self::Network => "Network & cloud", Self::Progress => "Update progress", Self::Events => "Events",
        Self::Message => "Status message", Self::Preferences => "Preferences",
    } }
    fn icon(self) -> &'static str { match self {
        Self::Connection => icons::PLUG, Self::StatusRgb => icons::LIGHTBULB, Self::Telemetry => icons::CIRCUITRY,
        Self::Network => icons::GLOBE, Self::Progress => icons::ARROW_DOWN, Self::Events => icons::BELL,
        Self::Message => icons::INFO, Self::Preferences => icons::GEAR,
    } }
    fn action(self) -> Action { match self {
        Self::Connection => Action::Connect, Self::StatusRgb | Self::Telemetry => Action::Hardware,
        Self::Network => Action::Network, Self::Progress => Action::Updates,
        Self::Events | Self::Message => Action::Events, Self::Preferences => Action::Preferences,
    } }
    fn width(self) -> f32 { match self {
        Self::Connection => 154.0, Self::StatusRgb | Self::Preferences => 28.0,
        Self::Telemetry => 144.0, Self::Network => 160.0, Self::Progress => 186.0,
        Self::Events => 94.0, Self::Message => 100.0,
    } }
}
impl Items {
    fn ordered(&self) -> Vec<Widget> {
        let mut ordered = Vec::new();
        for widget in self.order.iter().filter_map(|key| Widget::ALL.into_iter().find(|widget| widget.key() == key))
            .chain(Widget::ALL) {
            if !ordered.contains(&widget) { ordered.push(widget); }
        }
        ordered
    }
    fn visible(&self, widget: Widget) -> bool { match widget {
        Widget::Connection => self.connection, Widget::StatusRgb => self.status_rgb,
        Widget::Telemetry => self.telemetry, Widget::Network => self.network,
        Widget::Progress => self.progress, Widget::Events => self.events,
        Widget::Message => self.message, Widget::Preferences => self.preferences,
    } }
    fn set_visible(&mut self, widget: Widget, visible: bool) { match widget {
        Widget::Connection => self.connection = visible, Widget::StatusRgb => self.status_rgb = visible,
        Widget::Telemetry => self.telemetry = visible, Widget::Network => self.network = visible,
        Widget::Progress => self.progress = visible, Widget::Events => self.events = visible,
        Widget::Message => self.message = visible, Widget::Preferences => self.preferences = visible,
    } }
    fn is_trailing(&self, widget: Widget) -> bool { self.trailing.iter().any(|key| key == widget.key()) }
    fn set_trailing(&mut self, widget: Widget, trailing: bool) {
        self.trailing.retain(|key| key != widget.key());
        if trailing { self.trailing.push(widget.key().into()); }
        // Membership has no order of its own: retain the same canonical form
        // as Deserialize so moving an item cannot dirty preferences on restart.
        self.trailing = Widget::ALL.iter().filter(|item| self.trailing.iter().any(|key| key == item.key()))
            .map(|item| item.key().into()).collect();
    }
    fn move_by(&mut self, widget: Widget, offset: isize) -> bool {
        let mut ordered = self.ordered();
        let positions: Vec<_> = ordered.iter().enumerate().filter(|(_, item)| self.is_trailing(**item) == self.is_trailing(widget))
            .map(|(index, _)| index).collect();
        let at = positions.iter().position(|index| ordered[*index] == widget).unwrap();
        let target = (at as isize + offset).clamp(0, positions.len() as isize - 1) as usize;
        if target == at { return false; }
        ordered.swap(positions[at], positions[target]);
        self.order = ordered.iter().map(|item| item.key().into()).collect();
        true
    }
}
#[derive(Clone, Debug, Default, Deserialize, Serialize)]
#[serde(default)]
pub struct PhysicalStatusRgb {
    pub available: bool,
    pub have_output: bool,
    pub enabled: Option<bool>,
    pub rgb: Option<[u8; 3]>,
    pub brightness: Option<u8>,
    pub state: String,
    pub revision: u32,
    pub pin: Option<i8>,
    pub pin_confirmed: Option<bool>,
    pub persisted: Option<bool>,
    pub effect: Option<u8>,
    pub progress_percent: Option<u8>,
}
impl PhysicalStatusRgb {
    pub fn output_color(&self) -> Option<egui::Color32> {
        let [red, green, blue] = self.rgb?;
        // Actual Pealayer status_led_display_color: authoritative emitted RGB,
        // never apply brightness a second time in the mirror.
        (self.available && self.have_output).then(|| egui::Color32::from_rgb(red, green, blue))
    }
}
pub fn optional_status_rgb<'de, D: serde::Deserializer<'de>>(
    d: D,
) -> Result<Option<PhysicalStatusRgb>, D::Error> {
    // A malformed additive LED observation is Unknown, not a reason to retire
    // an otherwise valid board connection or invent a fallback RGB value.
    let value = serde_json::Value::deserialize(d)?;
    Ok(serde_json::from_value(value).ok())
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    Save,
    Refresh,
    Connect,
    Hardware,
    Network,
    Preferences,
    Events,
    Updates,
}
#[derive(Clone, Copy)]
pub struct Progress<'a> {
    pub label: &'a str,
    pub detail: &'a str,
    /// Measured progress only. Unknown, invalid or unreported values are None,
    /// never elapsed-time guesses or completion inferred from a caption.
    pub percent: Option<f32>,
}
impl Progress<'_> {
    fn fraction(self) -> Option<f32> {
        self.percent.filter(|value| value.is_finite() && (0.0..=100.0).contains(value)).map(|value| value / 100.0)
    }
}
pub struct StatusBar<'a> {
    pub status_rgb: Option<&'a PhysicalStatusRgb>,
    pub peer_known: bool,
    pub avr_available: bool,
    pub display_available: bool,
    pub network_available: bool,
    pub telemetry_available: bool,
    pub message: &'a str,
    pub hostname: &'a str,
    pub station: &'a str,
    pub avr_online: bool,
    pub tm1637_ready: bool,
    pub websocket_live: bool,
    pub serial_live: bool,
    pub connected: bool,
    pub connection_enabled: bool,
    pub serial_selected: bool,
    pub event_count: usize,
    pub error_count: usize,
    pub progress: Option<Progress<'a>>,
}

const BAR_HEIGHT: f32 = 30.0;
const ITEM_HEIGHT: f32 = 24.0;
const ICON_WIDTH: f32 = 28.0;
const GAP: f32 = 4.0;

impl StatusBar<'_> {
    fn present(&self, widget: Widget) -> bool { match widget {
        Widget::StatusRgb => self.peer_known && self.status_rgb.is_some_and(|led| led.available),
        Widget::Telemetry => self.peer_known && (self.telemetry_available || self.avr_available || self.display_available),
        Widget::Network => self.peer_known && self.network_available,
        Widget::Progress => self.progress.is_some(),
        Widget::Message => !self.message.trim().is_empty(),
        _ => true,
    } }
    fn connection_label(&self) -> &'static str {
        if self.connected { "Connected" } else if self.connection_enabled { "Connecting…" } else { "Disconnected" }
    }
    fn transport(&self) -> &'static str {
        if self.serial_selected { "USB" } else if self.websocket_live && self.connected { "WebSocket" } else { "HTTP" }
    }
    fn hardware_warning(&self) -> bool {
        self.connected && ((self.avr_available && !self.avr_online) || (self.display_available && !self.tm1637_ready))
    }
    fn rgb_color(&self) -> Option<egui::Color32> {
        self.connected.then(|| self.status_rgb.and_then(PhysicalStatusRgb::output_color)).flatten()
    }
    fn caption(&self, widget: Widget) -> String { match widget {
        Widget::Connection => i18n::text(self.connection_label()),
        Widget::Telemetry => i18n::text(if self.hardware_warning() { "Check hardware" } else { "Hardware" }),
        Widget::Network => if self.connected && !self.station.is_empty() { self.station.to_owned() } else { i18n::text("Network") },
        Widget::Progress => self.progress.map(|progress| progress.label.to_owned()).unwrap_or_default(),
        Widget::Events => format!("{} {}", i18n::text("Events"), count_label(if self.error_count > 0 { self.error_count } else { self.event_count })),
        Widget::Message => self.message.to_owned(),
        _ => i18n::text(widget.label()),
    } }
    fn detail(&self, widget: Widget) -> String {
        let known = |value: bool| i18n::text(if !self.connected { "Unknown" } else if value { "Connected" } else { "Disconnected" });
        match widget {
            Widget::Connection => format!("{} · {}\n{}\n{}", i18n::text(self.connection_label()), self.transport(),
                if self.hostname.is_empty() { i18n::text("No device selected") } else { self.hostname.to_owned() }, i18n::text("Manage connection")),
            Widget::Telemetry => {
                let mut lines = vec![i18n::text("Hardware monitor")];
                if self.avr_available { lines.push(format!("AVR: {}", known(self.avr_online))); }
                if self.display_available { lines.push(format!("TM1637: {}", i18n::text(if !self.connected { "Unknown" } else if self.tm1637_ready { "Ready" } else { "No acknowledgement" }))); }
                lines.join("\n")
            }
            Widget::Network => format!("{}\n{}\n{}: {}", i18n::text("Network & cloud"), self.hostname,
                i18n::text("Network"), if self.connected && !self.station.is_empty() { self.station.to_owned() } else { i18n::text("Unknown") }),
            Widget::Progress => self.progress.map(|progress| format!("{}\n{}\n{}", progress.label, progress.detail,
                progress.fraction().map(|value| format!("{:.0}%", value * 100.0)).unwrap_or_else(|| i18n::text("Progress not reported")))).unwrap_or_default(),
            Widget::Events => format!("{}: {}\n{}: {}", i18n::text("Events"), self.event_count, i18n::text("Active errors"), self.error_count),
            Widget::StatusRgb => {
                let Some(led) = self.status_rgb else { return i18n::text("Unknown"); };
                let rgb = self.rgb_color().map(|color| format!("#{:02X}{:02X}{:02X}", color.r(), color.g(), color.b())).unwrap_or_else(|| i18n::text("Unknown"));
                let effect = match led.effect { Some(0) => "Steady", Some(1) => "Breathe", Some(2) => "Blink",
                    Some(3) => "Color fade", Some(4) => "Fade out", Some(5) => "Pulse", Some(6) => "Double blink", _ => "Unknown" };
                format!("{}: {rgb}\n{}\n{}: {} · GPIO {}\n{}: {} · {}: {}\n{}: {}", i18n::text("Physical status RGB"),
                    if self.connected { led.state.clone() } else { i18n::text("Disconnected") }, i18n::text("Effect"), i18n::text(effect),
                    led.pin.map(|value| value.to_string()).unwrap_or_else(|| i18n::text("Unknown")), i18n::text("Brightness"),
                    led.brightness.map(|value| value.to_string()).unwrap_or_else(|| i18n::text("Unknown")), i18n::text("Revision"), led.revision,
                    i18n::text("Update progress"), led.progress_percent.filter(|value| *value <= 100).map(|value| format!("{value}%")).unwrap_or_else(|| i18n::text("Unknown")))
            }
            Widget::Message => self.message.to_owned(),
            Widget::Preferences => i18n::text("Preferences"),
        }
    }
}
fn count_label(value: usize) -> String { if value > 999 { "999+".into() } else { value.to_string() } }

#[derive(Clone, Debug)]
struct Slot { widget: Widget, width: f32 }
#[derive(Clone, Debug)]
struct Plan { slots: Vec<Slot>, overflow: Vec<Widget> }
fn required_width(slots: &[Slot]) -> f32 {
    slots.iter().map(|slot| slot.width).sum::<f32>() + GAP * slots.len().saturating_sub(1) as f32
}
fn layout_plan(available: f32, status: &StatusBar<'_>, items: &Items) -> Plan {
    let mut plan = Plan { slots: items.ordered().into_iter().filter(|widget| items.visible(*widget) && status.present(*widget))
        .map(|widget| Slot { widget, width: widget.width() }).collect(), overflow: Vec::new() };
    // Preserve connection, active operation and error access longest. Labels
    // collapse into the same accessible icon controls before anything moves to
    // overflow; arbitrary hostname/count/message lengths never affect geometry.
    let priorities = [Widget::Message, Widget::Network, Widget::Telemetry, Widget::StatusRgb,
        Widget::Preferences, Widget::Events, Widget::Connection, Widget::Progress];
    for widget in priorities {
        if required_width(&plan.slots) <= available { break; }
        if let Some(slot) = plan.slots.iter_mut().find(|slot| slot.widget == widget) {
            slot.width = ICON_WIDTH;
        }
    }
    for widget in priorities {
        let reserve = if plan.overflow.is_empty() { 0.0 } else { ICON_WIDTH + GAP };
        if required_width(&plan.slots) + reserve <= available { break; }
        if let Some(at) = plan.slots.iter().position(|slot| slot.widget == widget) {
            plan.overflow.push(plan.slots.remove(at).widget);
        }
    }
    let reserve = if plan.overflow.is_empty() { 0.0 } else { ICON_WIDTH + GAP };
    let spare = (available - required_width(&plan.slots) - reserve).max(0.0);
    if let Some(message) = plan.slots.iter_mut().find(|slot| slot.widget == Widget::Message && slot.width > ICON_WIDTH) {
        message.width += spare;
    }
    plan
}

fn clipped_text(ui: &egui::Ui, rect: egui::Rect, text: &str, color: egui::Color32, rtl: bool) {
    if rect.width() < 1.0 { return; }
    let mut job = egui::text::LayoutJob::simple(text.to_owned(), egui::FontId::proportional(12.0), color, rect.width());
    job.wrap.max_rows = 1;
    job.wrap.break_anywhere = true;
    let galley = ui.painter().layout_job(job);
    let x = if rtl { rect.right() - galley.size().x } else { rect.left() };
    ui.painter().with_clip_rect(rect.intersect(ui.clip_rect())).galley(
        egui::pos2(x, rect.center().y - galley.size().y * 0.5), galley, color);
}
fn button_shell(ui: &mut egui::Ui, id: egui::Id, rect: egui::Rect, label: &str) -> egui::Response {
    let response = ui.interact(rect, id, egui::Sense::click());
    response.widget_info(|| egui::WidgetInfo::labeled(egui::WidgetType::Button, ui.is_enabled(), label));
    let hovered = response.hovered() || response.has_focus();
    let fill = if response.is_pointer_button_down_on() { ui.visuals().widgets.active.weak_bg_fill }
        else if hovered { ui.visuals().widgets.hovered.weak_bg_fill } else { egui::Color32::TRANSPARENT };
    let stroke = if response.has_focus() { ui.visuals().selection.bg_fill }
        else if hovered { ui.visuals().widgets.hovered.bg_stroke.color } else { egui::Color32::TRANSPARENT };
    // Inside-only, constant-width focus/hover border: never changes layout.
    ui.painter().rect(rect, 5.0, fill, egui::Stroke::new(1.0, stroke), egui::StrokeKind::Inside);
    response.on_hover_cursor(egui::CursorIcon::PointingHand)
}
fn paint_widget(ui: &mut egui::Ui, rect: egui::Rect, widget: Widget, status: &StatusBar<'_>, rtl: bool) -> egui::Response {
    let label = status.caption(widget);
    let detail = status.detail(widget);
    let response = button_shell(ui, ui.id().with(("status-widget", widget.key())), rect, &format!("{}: {detail}", i18n::text(widget.label())));
    let muted = ui.visuals().weak_text_color();
    // Pakar's selection stroke is the contrasting text ON a solid accent
    // surface. This transparent footer needs the accent itself, not that text.
    let accent = ui.visuals().selection.bg_fill;
    let color = match widget {
        Widget::Connection if status.connected => if ui.visuals().dark_mode { egui::Color32::from_rgb(83, 195, 132) } else { egui::Color32::from_rgb(28, 115, 72) },
        Widget::Events if status.error_count > 0 => ui.visuals().error_fg_color,
        Widget::Telemetry if status.hardware_warning() => ui.visuals().warn_fg_color,
        Widget::Progress => accent,
        _ => muted,
    };
    let icon_center = if rect.width() <= ICON_WIDTH { rect.center() } else {
        egui::pos2(if rtl { rect.right() - 14.0 } else { rect.left() + 14.0 }, rect.center().y)
    };
    if widget == Widget::StatusRgb {
        if let Some(rgb) = status.rgb_color() {
            ui.painter().circle_filled(icon_center, 6.0, rgb);
            ui.painter().circle_stroke(icon_center, 6.0, egui::Stroke::new(1.0, ui.visuals().widgets.noninteractive.bg_stroke.color));
        } else {
            ui.painter().circle_stroke(icon_center, 6.0, egui::Stroke::new(1.0, muted));
            ui.painter().line_segment([icon_center + egui::vec2(-3.0, 3.0), icon_center + egui::vec2(3.0, -3.0)], egui::Stroke::new(1.0, muted));
        }
    } else {
        let icon = match widget {
            Widget::Connection if !status.serial_selected => icons::BROADCAST,
            Widget::Events if status.error_count > 0 => icons::WARNING,
            Widget::Telemetry if status.hardware_warning() => icons::WARNING,
            _ => widget.icon(),
        };
        // Phosphor private-use glyphs retain their registered proportional font.
        ui.painter().text(icon_center, egui::Align2::CENTER_CENTER, icon, egui::FontId::proportional(15.0), color);
    }
    if rect.width() > ICON_WIDTH {
        let mut text_rect = rect.shrink2(egui::vec2(7.0, 0.0));
        if rtl { text_rect.max.x -= 20.0; } else { text_rect.min.x += 20.0; }
        if widget == Widget::Progress {
            let progress = status.progress.unwrap();
            let fraction = progress.fraction();
            let percent = fraction.map(|value| format!("{:.0}%", value * 100.0)).unwrap_or_else(|| "—".into());
            let mut number_rect = text_rect;
            if rtl { number_rect.max.x = number_rect.min.x + 34.0; text_rect.min.x += 38.0; }
            else { number_rect.min.x = number_rect.max.x - 34.0; text_rect.max.x -= 38.0; }
            clipped_text(ui, number_rect, &percent, color, !rtl);
            let rail = egui::Rect::from_min_size(egui::pos2(rect.left() + 7.0, rect.bottom() - 3.0), egui::vec2(rect.width() - 14.0, 2.0));
            ui.painter().rect_filled(rail, 1.0, ui.visuals().widgets.inactive.bg_stroke.color);
            if let Some(fraction) = fraction {
                let start = if rtl { rail.right() - rail.width() * fraction } else { rail.left() };
                ui.painter().rect_filled(egui::Rect::from_min_size(egui::pos2(start, rail.top()), egui::vec2(rail.width() * fraction, rail.height())), 1.0, color);
            }
        }
        clipped_text(ui, text_rect, &label, color, rtl);
    } else if widget == Widget::Events && status.error_count > 0 {
        ui.painter().circle_filled(rect.right_top() + egui::vec2(-4.0, 4.0), 2.5, color);
    }
    response.on_hover_text(detail)
}

fn item_menu(ui: &mut egui::Ui, widget: Widget, status: &StatusBar<'_>, items: &mut Items, actions: &mut Vec<Action>) {
    compact_menu(ui);
    ui.strong(i18n::text(widget.label()));
    if ui.button(format!("{}  {}", widget.icon(), i18n::text(widget.label()))).clicked() {
        actions.push(widget.action()); ui.close();
    }
    if widget == Widget::StatusRgb && let Some(color) = status.rgb_color()
        && ui.button(format!("{}  {}", icons::COPY, i18n::text("Copy color"))).clicked() {
        ui.ctx().copy_text(format!("#{:02X}{:02X}{:02X}", color.r(), color.g(), color.b())); ui.close();
    }
    ui.separator();
    let mut changed = false;
    let at_end = items.is_trailing(widget);
    // Capture the current group before a placement click changes membership.
    // ui.close() schedules a popup close; it does not return from this function.
    let ordered: Vec<_> = items.ordered().into_iter().filter(|item| items.is_trailing(*item) == at_end).collect();
    let index = ordered.iter().position(|item| *item == widget).unwrap();
    if ui.button(format!("{}  {}", icons::ARROWS_OUT, i18n::text(if at_end { "Place at start" } else { "Place at end" }))).clicked() {
        items.set_trailing(widget, !at_end); changed = true; ui.close();
    }
    if ui.add_enabled(index > 0, egui::Button::new(format!("{}  {}", icons::ARROW_UP, i18n::text("Move earlier")))).clicked() {
        changed |= items.move_by(widget, -1); ui.close();
    }
    if ui.add_enabled(index + 1 < ordered.len(), egui::Button::new(format!("{}  {}", icons::ARROW_DOWN, i18n::text("Move later")))).clicked() {
        changed |= items.move_by(widget, 1); ui.close();
    }
    if ui.button(format!("{}  {}", icons::EYE_SLASH, i18n::text("Hide"))).clicked() {
        items.set_visible(widget, false); changed = true; ui.close();
    }
    if changed { actions.push(Action::Save); }
}
fn bar_menu(ui: &mut egui::Ui, status: &StatusBar<'_>, items: &mut Items, actions: &mut Vec<Action>) {
    compact_menu(ui);
    ui.strong(i18n::text("Status bar"));
    for widget in items.ordered() {
        let mut visible = items.visible(widget);
        if ui.checkbox(&mut visible, i18n::text(widget.label())).changed() {
            items.set_visible(widget, visible); actions.push(Action::Save);
        }
    }
    if ui.button(format!("{}  {}", icons::ARROW_COUNTER_CLOCKWISE, i18n::text("Restore status bar"))).clicked() {
        if *items != Items::default() { *items = Items::default(); actions.push(Action::Save); }
        ui.close();
    }
    ui.separator();
    for widget in [Widget::Connection, Widget::Telemetry, Widget::Network, Widget::Progress, Widget::Events, Widget::Preferences] {
        if !status.present(widget) { continue; }
        if ui.button(format!("{}  {}", widget.icon(), i18n::text(widget.label()))).clicked() { actions.push(widget.action()); ui.close(); }
    }
    if ui.add_enabled(status.connected, egui::Button::new(format!("{}  {}", icons::ARROW_CLOCKWISE, i18n::text("Refresh")))).clicked() {
        actions.push(Action::Refresh); ui.close();
    }
}

#[derive(Clone, Debug)]
struct Geometry { frame: egui::Rect, widgets: Vec<(Widget, egui::Rect)>, overflow: Option<egui::Rect> }
impl Default for Geometry {
    fn default() -> Self { Self { frame: egui::Rect::NOTHING, widgets: Vec::new(), overflow: None } }
}
fn draw_row(ui: &mut egui::Ui, status: &StatusBar<'_>, items: &mut Items, actions: &mut Vec<Action>, rtl: bool) -> Geometry {
    let (frame, _) = ui.allocate_exact_size(egui::vec2(ui.available_width().max(1.0), ITEM_HEIGHT), egui::Sense::hover());
    let plan = layout_plan(frame.width(), status, items);
    let overflow_width = if plan.overflow.is_empty() { 0.0 } else { (ICON_WIDTH + GAP).min(frame.width()) };
    let leading: Vec<_> = plan.slots.iter().filter(|slot| !items.is_trailing(slot.widget)).collect();
    let trailing: Vec<_> = plan.slots.iter().filter(|slot| items.is_trailing(slot.widget)).collect();
    let trailing_width = trailing.iter().map(|slot| slot.width).sum::<f32>() + GAP * trailing.len().saturating_sub(1) as f32;
    let to_rect = |x: f32, width: f32| egui::Rect::from_min_size(egui::pos2(
        if rtl { frame.right() - x - width } else { frame.left() + x }, frame.top()), egui::vec2(width, ITEM_HEIGHT));
    let mut geometry = Geometry { frame, ..Default::default() };
    let mut cursor: f32 = 0.0;
    for (group, start) in [(leading, 0.0), (trailing, (frame.width() - overflow_width - trailing_width).max(0.0))] {
        cursor = cursor.max(start);
        for slot in group {
            let rect = to_rect(cursor, slot.width);
            cursor += slot.width + GAP;
            let response = paint_widget(ui, rect, slot.widget, status, rtl);
            if response.clicked() { actions.push(slot.widget.action()); }
            response.context_menu(|ui| item_menu(ui, slot.widget, status, items, actions));
            geometry.widgets.push((slot.widget, rect));
        }
    }
    if !plan.overflow.is_empty() {
        let width = ICON_WIDTH.min(frame.width());
        let rect = to_rect((frame.width() - width).max(0.0), width);
        let id = ui.id().with("status-overflow");
        let response = button_shell(ui, id, rect, &i18n::text("More status actions"));
        ui.painter().text(rect.center(), egui::Align2::CENTER_CENTER, icons::DOTS_THREE, egui::FontId::proportional(17.0), ui.visuals().weak_text_color());
        // Use the already-imported Pealayer menu focus owner for keyboard
        // navigation; the popup opens above a bottom status bar.
        let popup_id = id.with("status-overflow-menu");
        if response.has_focus() && ui.input_mut(|input| input.consume_key(egui::Modifiers::NONE, egui::Key::ArrowUp)
            || input.consume_key(egui::Modifiers::NONE, egui::Key::ArrowDown)) {
            egui::Popup::open_id(ui.ctx(), popup_id);
        }
        let popup = egui::Popup::menu(&response).id(popup_id)
            .align(if rtl { egui::RectAlign::TOP_START } else { egui::RectAlign::TOP_END }).show(|ui| {
            dropdown::menu_ui(ui, id, |ui| {
                use super::dropdown::DropdownUiExt;
                compact_menu(ui);
                for widget in &plan.overflow {
                    if ui.dropdown_choice(false, format!("{}  {}", widget.icon(), i18n::text(widget.label()))).clicked() {
                        actions.push(widget.action()); ui.close();
                    }
                }
                ui.separator();
                ui.menu_button(i18n::text("Status bar"), |ui| bar_menu(ui, status, items, actions));
            });
        });
        if popup.is_none() { dropdown::clear_menu(ui.ctx(), id); }
        geometry.overflow = Some(rect);
    }
    geometry
}

pub fn draw(ui: &mut egui::Ui, status: StatusBar<'_>, items: &mut Items) -> Vec<Action> {
    let mut actions = Vec::new();
    let panel = egui::Panel::bottom("pakar_status_bar")
        .resizable(false).exact_size(BAR_HEIGHT)
        .frame(egui::Frame::NONE.fill(ui.visuals().panel_fill).inner_margin(egui::Margin::symmetric(6, 3)))
        .show_separator_line(true)
        .show(ui, |ui| draw_row(ui, &status, items, &mut actions, i18n::rtl()));
    let geometry = panel.inner;
    // Item menus own their clicks; an empty-area menu must not compete with
    // them. The whole row remains a restore target when every item is hidden.
    let over_item = ui.input(|input| input.pointer.hover_pos()).is_some_and(|position|
        geometry.widgets.iter().any(|(_, rect)| rect.contains(position)) || geometry.overflow.is_some_and(|rect| rect.contains(position)));
    if !over_item {
        panel.response.interact(egui::Sense::click()).context_menu(|ui| bar_menu(ui, &status, items, &mut actions));
    }
    #[cfg(test)]
    ui.ctx().data_mut(|data| data.insert_temp(egui::Id::new("status-test-geometry"), geometry));
    if actions.contains(&Action::Save) {
        actions.retain(|action| *action != Action::Save);
        actions.push(Action::Save);
    }
    actions
}
#[cfg(test)]
mod tests {
    use super::*;
    fn fixture<'a>() -> StatusBar<'a> {
        StatusBar { status_rgb: None, peer_known: true, avr_available: true, display_available: true,
            network_available: true, telemetry_available: true, message: "Connected", hostname: "ledboard.local",
            station: "Studio Wi-Fi", avr_online: true, tm1637_ready: true, websocket_live: true, serial_live: false,
            connected: true, connection_enabled: true, serial_selected: false, event_count: 12, error_count: 0,
            progress: Some(Progress { label: "Firmware update", detail: "Receiving verified image bytes", percent: Some(37.0) }) }
    }
    fn frame(context: &egui::Context, width: f32, status: &StatusBar<'_>, items: &mut Items,
        rtl: bool, events: Vec<egui::Event>) -> (Geometry, Vec<Action>) {
        let mut geometry = Geometry::default();
        let mut actions = Vec::new();
        let mut output = context.run_ui(egui::RawInput {
            screen_rect: Some(egui::Rect::from_min_size(egui::Pos2::ZERO, egui::vec2(width, 200.0))),
            events, ..Default::default()
        }, |ui| geometry = draw_row(ui, status, items, &mut actions, rtl));
        output.textures_delta.clear();
        (geometry, actions)
    }
    fn key(key: egui::Key) -> egui::Event {
        egui::Event::Key { key, physical_key: None, pressed: true, repeat: false, modifiers: egui::Modifiers::NONE }
    }
    #[test]
    fn status_legacy_visibility_migrates_all_combinations_without_reviving_telemetry() {
        for mask in 0..16 {
            let values: Vec<_> = (0..4).map(|bit| mask & (1 << bit) != 0).collect();
            let mut items: Items = serde_json::from_value(serde_json::json!({
                "connection":values[0], "telemetry":values[1], "message":values[2], "status_rgb":values[3]
            })).unwrap();
            assert_eq!([items.connection, items.telemetry, items.message, items.status_rgb], values.as_slice());
            assert_eq!(items.network, values[1]);
            items.set_visible(Widget::Network, true);
            assert_eq!(items.telemetry, values[1], "New child toggles never revive its former sibling");
            let restored: Items = serde_json::from_value(serde_json::to_value(&items).unwrap()).unwrap();
            assert_eq!(restored, items);
        }
    }
    #[test]
    fn status_order_normalizes_unknown_duplicate_and_malformed_entries() {
        for order in [serde_json::json!(["events", "events", "unknown", 5, "connection"]), serde_json::json!({"wrong":"type"}), serde_json::Value::Null] {
            let items: Items = serde_json::from_value(serde_json::json!({"order":order,"trailing":["events","events","unknown"]})).unwrap();
            assert_eq!(items.ordered().len(), Widget::ALL.len());
            for widget in Widget::ALL { assert_eq!(items.ordered().iter().filter(|item| **item == widget).count(), 1); }
            assert_eq!(items.trailing, ["events"]);
        }
        let mut items = Items::default();
        let leading_before: Vec<_> = items.ordered().into_iter().filter(|widget| !items.is_trailing(*widget)).collect();
        assert!(!items.move_by(Widget::Progress, -1), "First trailing item cannot silently swap with a leading item");
        assert!(items.move_by(Widget::Events, -1));
        let trailing: Vec<_> = items.ordered().into_iter().filter(|widget| items.is_trailing(*widget)).collect();
        assert_eq!(trailing[0], Widget::Events);
        assert_eq!(items.ordered().into_iter().filter(|widget| !items.is_trailing(*widget)).collect::<Vec<_>>(), leading_before);
        items.set_trailing(Widget::Connection, true);
        assert!(items.is_trailing(Widget::Connection));
    }
    #[test]
    fn status_placement_menu_click_moves_group_and_saves_once_without_panicking() {
        let context = egui::Context::default(); let status = fixture(); let mut items = Items::default();
        for label in ["Place at end", "Place at start"] {
            let mut render = |events| {
                let mut actions = Vec::new();
                let mut output = context.run_ui(egui::RawInput { events, ..Default::default() }, |ui| {
                    item_menu(ui, Widget::Connection, &status, &mut items, &mut actions);
                });
                output.textures_delta.clear();
                let position = output.shapes.iter().find_map(|shape| match &shape.shape {
                    egui::Shape::Text(text) if text.galley.job.text.contains(&i18n::text(label)) => Some(text.pos + text.galley.size() * 0.5),
                    _ => None,
                });
                (position, actions)
            };
            let position = render(vec![]).0.expect("placement action is visible");
            let event = |pressed| egui::Event::PointerButton { pos: position, button: egui::PointerButton::Primary, pressed, modifiers: egui::Modifiers::NONE };
            render(vec![egui::Event::PointerMoved(position), event(true)]);
            let (_, actions) = render(vec![event(false)]);
            assert_eq!(actions, [Action::Save]);
            assert_eq!(items.is_trailing(Widget::Connection), label == "Place at end");
            assert_eq!(serde_json::from_value::<Items>(serde_json::to_value(&items).unwrap()).unwrap(), items);
        }
    }
    #[test]
    fn status_connection_and_rgb_require_current_route_not_cached_peer_or_websocket() {
        let led = PhysicalStatusRgb { available: true, have_output: true, rgb: Some([24,105,61]), brightness: Some(128), ..Default::default() };
        let mut status = fixture();
        status.status_rgb = Some(&led);
        status.websocket_live = false;
        assert_eq!(status.connection_label(), "Connected");
        assert_eq!(status.transport(), "HTTP");
        assert_eq!(status.rgb_color(), Some(egui::Color32::from_rgb(24,105,61)));
        status.connected = false;
        assert_eq!(status.connection_label(), "Connecting…");
        assert_eq!(status.rgb_color(), None, "Cached emitted RGB is not current output after disconnect");
        status.connection_enabled = false;
        assert_eq!(status.connection_label(), "Disconnected");
        status.serial_selected = true;
        assert_eq!(status.transport(), "USB");
        status.peer_known = false;
        assert!(!status.present(Widget::Telemetry));
        assert!(!status.present(Widget::Network));
        assert!(!status.present(Widget::StatusRgb));
    }
    #[test]
    fn status_measured_progress_distinguishes_unknown_zero_and_completion() {
        let progress = |percent| Progress { label: "Update", detail: "", percent };
        for value in [None, Some(f32::NAN), Some(-1.0), Some(100.1), Some(f32::INFINITY)] {
            assert_eq!(progress(value).fraction(), None);
        }
        assert_eq!(progress(Some(0.0)).fraction(), Some(0.0));
        assert_eq!(progress(Some(100.0)).fraction(), Some(1.0));
        let context = egui::Context::default();
        let mut expected = None;
        for value in [None, Some(0.0), Some(1.0), Some(99.0), Some(100.0)] {
            let mut status = fixture(); status.progress = Some(progress(value));
            let (geometry, actions) = frame(&context, 1200.0, &status, &mut Items::default(), false, vec![]);
            let rect = geometry.widgets.iter().find(|(widget, _)| *widget == Widget::Progress).unwrap().1;
            if let Some(expected) = expected { assert_eq!(rect, expected); } else { expected = Some(rect); }
            assert!(actions.is_empty(), "Progress events cannot persist or send commands from paint");
        }
    }
    #[test]
    fn status_progress_and_focus_use_visible_accent_with_actual_dark_app_palette() {
        let context = egui::Context::default();
        let previous = crate::config::live();
        let config = crate::config::AppConfig { theme: crate::config::AppTheme::Dark, ..Default::default() };
        crate::config::apply(&context, &config);
        crate::config::set_live(&previous);
        let rect = egui::Rect::from_min_size(egui::pos2(40.0,40.0), egui::vec2(Widget::Progress.width(), ITEM_HEIGHT));
        let mut id = None;
        let mut first = context.run_ui(egui::RawInput::default(), |ui| {
            id = Some(ui.id().with(("status-widget", Widget::Progress.key())));
            paint_widget(ui, rect, Widget::Progress, &fixture(), false);
        });
        first.textures_delta.clear();
        context.memory_mut(|memory| memory.request_focus(id.unwrap()));
        let mut accent = egui::Color32::TRANSPARENT;
        let mut output = context.run_ui(egui::RawInput::default(), |ui| {
            assert!(ui.visuals().dark_mode);
            accent = ui.visuals().selection.bg_fill;
            assert_ne!(accent, ui.visuals().selection.stroke.color, "The real theme uses contrasting text on the selected fill");
            paint_widget(ui, rect, Widget::Progress, &fixture(), false);
        });
        output.textures_delta.clear();
        let text_colors: Vec<_> = output.shapes.iter().filter_map(|shape| match &shape.shape {
            egui::Shape::Text(text) => Some(text.fallback_color), _ => None,
        }).collect();
        assert!(text_colors.len() >= 3, "Icon, operation label and measured percentage are drawn");
        assert!(text_colors.iter().all(|color| *color == accent));
        assert!(output.shapes.iter().any(|shape| matches!(&shape.shape, egui::Shape::Rect(surface)
            if surface.rect == rect && surface.stroke.color == accent && surface.stroke.width == 1.0)));
        assert!(output.shapes.iter().any(|shape| matches!(&shape.shape, egui::Shape::Rect(surface)
            if surface.rect.height() == 2.0 && surface.fill == accent)), "Measured progress rail uses the same visible accent");
    }
    #[test]
    fn status_real_egui_slots_stay_single_row_in_narrow_rtl_and_scaled_views() {
        let led = PhysicalStatusRgb { available: true, have_output: false, ..Default::default() };
        for width in [72.0, 180.0, 280.0, 420.0, 900.0, 1920.0] {
            for rtl in [false, true] {
                for scale in [1.0, 1.25, 1.5, 2.0] {
                    let context = egui::Context::default(); context.set_pixels_per_point(scale);
                    let mut status = fixture(); status.status_rgb = Some(&led);
                    status.hostname = "very-long-hostname-that-must-never-push-an-event-chip.local";
                    status.station = "A very long wireless network with long localized text";
                    status.message = "A long informational message must truncate within its own stable row, never wrap or move any buttons";
                    let (geometry, actions) = frame(&context, width, &status, &mut Items::default(), rtl, vec![]);
                    assert!((geometry.frame.height() - ITEM_HEIGHT).abs() < 0.01);
                    let mut rects: Vec<_> = geometry.widgets.iter().map(|(_, rect)| *rect).collect();
                    if let Some(rect) = geometry.overflow { rects.push(rect); }
                    for (index, rect) in rects.iter().enumerate() {
                        assert!(geometry.frame.expand(0.01).contains_rect(*rect), "width={width} rtl={rtl} scale={scale}: {rect:?} outside {:?}", geometry.frame);
                        assert!((rect.height() - ITEM_HEIGHT).abs() < 0.01);
                        for other in rects.iter().skip(index + 1) { assert!(!rect.intersects(*other)); }
                    }
                    assert!(actions.is_empty());
                }
            }
        }
    }
    #[test]
    fn status_telemetry_changes_preserve_slot_bounds_and_never_request_save() {
        let context = egui::Context::default();
        let mut items = Items::default();
        let mut status = fixture();
        let before = frame(&context, 1100.0, &status, &mut items, false, vec![]).0;
        status.message = "A much longer state message with no layout authority";
        status.event_count = 15738; status.error_count = 37; status.avr_online = false;
        status.connected = false; status.connection_enabled = false;
        status.station = "different-network";
        let (after, actions) = frame(&context, 1100.0, &status, &mut items, false, vec![]);
        assert_eq!(before.widgets, after.widgets);
        assert!(actions.is_empty());
        assert_eq!(items, Items::default());
    }
    #[test]
    fn status_buttons_dispatch_click_and_keyboard_without_passive_actions() {
        let context = egui::Context::default();
        let status = fixture(); let mut items = Items::default();
        let (geometry, _) = frame(&context, 1100.0, &status, &mut items, false, vec![]);
        for widget in [Widget::Connection, Widget::Telemetry, Widget::Network, Widget::Progress, Widget::Events, Widget::Preferences] {
            let rect = geometry.widgets.iter().find(|(item, _)| *item == widget).unwrap().1;
            let event = |pressed| egui::Event::PointerButton { pos: rect.center(), button: egui::PointerButton::Primary, pressed, modifiers: egui::Modifiers::NONE };
            frame(&context, 1100.0, &status, &mut items, false, vec![egui::Event::PointerMoved(rect.center()), event(true)]);
            let (_, actions) = frame(&context, 1100.0, &status, &mut items, false, vec![event(false)]);
            assert_eq!(actions, vec![widget.action()]);
        }
        let mut button_id = None;
        let mut output = context.run_ui(egui::RawInput::default(), |ui| {
            button_id = Some(ui.id().with(("status-widget", Widget::Connection.key())));
            draw_row(ui, &status, &mut items, &mut Vec::new(), false);
        });
        output.textures_delta.clear();
        context.memory_mut(|memory| memory.request_focus(button_id.unwrap()));
        let (_, actions) = frame(&context, 1100.0, &status, &mut items, false, vec![key(egui::Key::Enter)]);
        assert_eq!(actions, vec![Action::Connect]);
    }
    #[test]
    fn status_overflow_retains_capability_gated_actions_and_arrow_keyboard_access() {
        let context = egui::Context::default(); let status = fixture(); let mut items = Items::default();
        let plan = layout_plan(180.0, &status, &items);
        assert!(plan.overflow.contains(&Widget::Message));
        assert!(plan.slots.iter().any(|slot| slot.widget == Widget::Progress));
        let expected: Vec<_> = items.ordered().into_iter().filter(|widget| status.present(*widget)).collect();
        for widget in expected { assert_eq!(plan.slots.iter().filter(|slot| slot.widget == widget).count() + plan.overflow.iter().filter(|item| **item == widget).count(), 1); }
        let mut trigger = None;
        let mut output = context.run_ui(egui::RawInput { screen_rect: Some(egui::Rect::from_min_size(egui::Pos2::ZERO, egui::vec2(180.0,200.0))), ..Default::default() }, |ui| {
            trigger = Some(ui.id().with("status-overflow"));
            draw_row(ui, &status, &mut items, &mut Vec::new(), false);
        });
        output.textures_delta.clear();
        context.memory_mut(|memory| memory.request_focus(trigger.unwrap()));
        frame(&context, 180.0, &status, &mut items, false, vec![key(egui::Key::ArrowUp)]);
        assert!(egui::Popup::is_id_open(&context, trigger.unwrap().with("status-overflow-menu")));
    }
    #[test]
    fn rgb_requires_observed_output_and_does_not_double_dim() {
        let mut led = PhysicalStatusRgb {
            available: true,
            have_output: true,
            rgb: Some([0, 255, 0]),
            brightness: Some(32),
            ..Default::default()
        };
        assert_eq!(led.output_color(), Some(egui::Color32::GREEN));
        led.have_output = false;
        assert_eq!(led.output_color(), None);
        led.have_output = true;
        led.available = false;
        assert_eq!(led.output_color(), None);
        assert_eq!(PhysicalStatusRgb::default().output_color(), None);
    }
    #[test]
    fn malformed_optional_rgb_does_not_disconnect_the_rest_of_status() {
        for value in [
            serde_json::json!({"rgb":[0,256,1]}),
            serde_json::json!({"pin":"48"}),
        ] {
            let s: crate::DeviceStatus = serde_json::from_value(
                serde_json::json!({"hostname":"ledboard","status_rgb":value}),
            )
            .unwrap();
            assert_eq!(s.hostname, "ledboard");
            assert!(s.status_rgb.is_none());
        }
    }
    #[test]
    fn status_preferences_roundtrip_and_missing_fields_default() {
        let mut items = Items::default();
        items.telemetry = false;
        assert_eq!(
            serde_json::from_str::<Items>(&serde_json::to_string(&items).unwrap()).unwrap(),
            items
        );
        assert_eq!(
            serde_json::from_str::<Items>("{}").unwrap(),
            Items::default()
        );
    }
}
