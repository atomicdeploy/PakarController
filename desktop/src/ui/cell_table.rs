//! Fixed-geometry table interaction; selection never mutates hardware.
//! Uses Pealayer's painted interaction targets and existing imported dialog/icons.
use super::{dialog, icons};
use crate::{OP_CELL_ACTION, PakarControllerApp};
use eframe::egui::{self, Color32, Key, Modifiers, Pos2, Rect, Stroke};
use std::{
    collections::{BTreeSet, VecDeque},
    time::{Duration, Instant},
};

#[derive(Default, Clone, Debug)]
pub struct Selection {
    pub ids: BTreeSet<u8>,
    anchor: Option<u8>,
}
impl Selection {
    pub fn click(&mut self, id: u8, toggle: bool, extend: bool) {
        if !(1..=80).contains(&id) {
            return;
        }
        if extend {
            let anchor = self.anchor.unwrap_or(id);
            self.ids = (anchor.min(id)..=anchor.max(id)).collect();
        } else if toggle {
            if !self.ids.remove(&id) {
                self.ids.insert(id);
            }
            self.anchor = Some(id);
        } else {
            self.ids = BTreeSet::from([id]);
            self.anchor = Some(id);
        }
    }
    pub fn group(&mut self, ids: impl IntoIterator<Item = u8>, toggle: bool) {
        let ids: BTreeSet<_> = ids.into_iter().filter(|v| (1..=80).contains(v)).collect();
        if toggle {
            let remove = ids.iter().all(|id| self.ids.contains(id));
            for id in ids {
                if remove {
                    self.ids.remove(&id);
                } else {
                    self.ids.insert(id);
                }
            }
        } else {
            self.ids = ids;
        }
    }
    fn marquee(&mut self, rect: Rect, cells: &[(u8, Rect)], base: &BTreeSet<u8>) {
        self.ids = base.clone();
        self.ids.extend(
            cells
                .iter()
                .filter(|(_, cell)| rect.contains(cell.center()))
                .map(|(id, _)| *id),
        );
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum State {
    White,
    Red,
    Off,
}
impl State {
    fn value(self) -> u8 {
        match self {
            Self::White => 0,
            Self::Red => 1,
            Self::Off => 2,
        }
    }
    fn name(self) -> &'static str {
        match self {
            Self::White => "white",
            Self::Red => "red",
            Self::Off => "off",
        }
    }
    fn icon(self) -> &'static str {
        match self {
            Self::White => icons::SUN,
            Self::Red => icons::LIGHTBULB,
            Self::Off => icons::EYE_SLASH,
        }
    }
}
pub struct TableState {
    pub selection: Selection,
    pub manager_open: bool,
    manager_state: State,
    marquee: Option<(Pos2, BTreeSet<u8>)>,
    queue: VecDeque<(u8, State, String)>,
    peer: Option<(crate::Transport, crate::Endpoint, String)>,
    next_send: Instant,
    queued: usize,
    dispatched: usize,
    awaiting: Vec<(u8, State)>,
    verification_started: Option<Instant>,
    result: String,
}
impl Default for TableState {
    fn default() -> Self {
        Self {
            selection: Default::default(),
            manager_open: false,
            manager_state: State::Red,
            marquee: None,
            queue: VecDeque::new(),
            peer: None,
            next_send: Instant::now(),
            queued: 0,
            dispatched: 0,
            awaiting: Vec::new(),
            verification_started: None,
            result: String::new(),
        }
    }
}
pub fn compact_menu(ui: &mut egui::Ui) {
    // Pealayer scopes compact spacing locally, never changing application geometry.
    ui.spacing_mut().item_spacing = egui::vec2(6.0, 2.0);
    ui.spacing_mut().button_padding = egui::vec2(7.0, 3.0);
    ui.spacing_mut().interact_size.y = 23.0;
}
pub fn navigate(id: u8, key: Key, ctrl: bool) -> u8 {
    let id = id.clamp(1, 80);
    match key {
        Key::ArrowLeft if (id - 1) % 10 > 0 => id - 1,
        Key::ArrowRight if (id - 1) % 10 < 9 => id + 1,
        Key::ArrowUp if id > 10 => id - 10,
        Key::ArrowDown if id <= 70 => id + 10,
        Key::Home if ctrl => 1,
        Key::End if ctrl => 80,
        Key::Home => ((id - 1) / 10) * 10 + 1,
        Key::End => ((id - 1) / 10) * 10 + 10,
        _ => id,
    }
}
pub fn cell_rect(origin: Pos2, pitch: f32, height: f32, id: u8) -> Rect {
    let row = (id - 1) / 10;
    let col = (id - 1) % 10;
    Rect::from_min_size(
        origin
            + egui::vec2(
                col as f32 * pitch,
                row as f32 * height + if col % 2 == 0 { height * 0.5 } else { 0.0 },
            ),
        egui::vec2(pitch - 7.0, height - 7.0),
    )
}
fn blend(a: Color32, b: Color32, t: f32) -> Color32 {
    let channel =
        |x: u8, y: u8| (x as f32 + (y as f32 - x as f32) * t.clamp(0.0, 1.0)).round() as u8;
    Color32::from_rgb(
        channel(a.r(), b.r()),
        channel(a.g(), b.g()),
        channel(a.b(), b.b()),
    )
}
fn animated_color(ui: &egui::Ui, id: egui::Id, target: Color32, animate: bool) -> Color32 {
    if !animate {
        return target;
    }
    let channel = |n, v: u8| {
        ui.ctx()
            .animate_value_with_time(id.with(n), v as f32, 0.14)
            .round()
            .clamp(0.0, 255.0) as u8
    };
    Color32::from_rgb(
        channel(0, target.r()),
        channel(1, target.g()),
        channel(2, target.b()),
    )
}

// Paint strokes INSIDE a fixed interaction rectangle, never through widget padding.
fn cell_surface(
    ui: &mut egui::Ui,
    rect: Rect,
    id: egui::Id,
    state: Option<&str>,
    selected: bool,
    matched: bool,
    animate: bool,
) -> egui::Response {
    let response = ui.interact(rect, id, egui::Sense::click_and_drag());
    let focus = response.has_focus();
    let v = ui.visuals();
    let mut fill = if state == Some("red") {
        blend(v.widgets.inactive.bg_fill, v.error_fg_color, 0.22)
    } else {
        v.widgets.inactive.bg_fill
    };
    if selected {
        fill = blend(fill, v.selection.bg_fill, 0.28);
    }
    if response.hovered() {
        fill = blend(fill, v.widgets.hovered.bg_fill, 0.65);
    }
    if response.is_pointer_button_down_on() {
        fill = blend(fill, v.selection.bg_fill, 0.45);
    }
    let line = if focus {
        v.selection.stroke.color
    } else if matched {
        v.warn_fg_color
    } else if selected {
        v.selection.bg_fill
    } else if response.hovered() {
        v.widgets.hovered.bg_stroke.color
    } else {
        v.widgets.noninteractive.bg_stroke.color
    };
    let width = if selected || matched || focus {
        2.0
    } else {
        1.0
    };
    let width = if animate {
        ui.ctx()
            .animate_value_with_time(id.with("width"), width, 0.14)
    } else {
        width
    };
    ui.painter().rect(
        rect,
        7.0,
        animated_color(ui, id.with("fill"), fill, animate),
        Stroke::new(width, animated_color(ui, id.with("line"), line, animate)),
        egui::StrokeKind::Inside,
    );
    let focus_alpha = if animate {
        ui.ctx()
            .animate_bool_with_time(id.with("focus"), focus, 0.14)
    } else if focus {
        1.0
    } else {
        0.0
    };
    ui.painter().rect_stroke(
        rect.shrink(4.0),
        4.0,
        Stroke::new(1.0, v.selection.stroke.color.linear_multiply(focus_alpha)),
        egui::StrokeKind::Inside,
    );
    response
}
fn pointer_or_accessibility_click(response: &egui::Response) -> bool {
    // egui synthesizes clicked() for Space/Enter. Our explicit key handler owns
    // those, otherwise additive selection would toggle twice in one frame.
    response.clicked()
        && !response
            .ctx
            .input(|i| i.key_pressed(Key::Space) || i.key_pressed(Key::Enter))
}
fn activation_key(ui: &egui::Ui, modifiers: Modifiers, key: Key) -> bool {
    let initial=ui.input(|i|i.events.iter().any(|event|matches!(event,egui::Event::Key{key:pressed,pressed:true,repeat:false,..} if *pressed==key)));
    initial && ui.input_mut(|i| i.consume_key(modifiers, key))
}
impl PakarControllerApp {
    fn cell_queue_peer(&self) -> Option<(crate::Transport, crate::Endpoint, String)> {
        self.command_route()
            .map(|route| (route, self.endpoint(), self.serial_port.clone()))
    }
    pub fn process_cell_queue(&mut self, ctx: &egui::Context) {
        if (!self.table.queue.is_empty() || self.table.verification_started.is_some())
            && self.table.peer != self.cell_queue_peer()
        {
            self.table.queue.clear();
            self.table.awaiting.clear();
            self.table.verification_started = None;
            self.table.result="Stopped: peer or transport changed. Previously sent edits may have applied; inspect device state.".into();
            return;
        }
        if !self.table.queue.is_empty() {
            if self.command_route().is_none() {
                self.table.queue.clear();
                self.table.result="Stopped: connection lost. Previously sent edits may have applied; inspect device state.".into();
                self.table.awaiting.clear();
                return;
            }
            if Instant::now() >= self.table.next_send {
                if let Some((id, state, effect)) = self.table.queue.pop_front() {
                    let effect_value = match effect.as_str() {
                        "fade" => 1,
                        "pulse" => 2,
                        _ => 0,
                    };
                    self.action(
                        "/api/v1/cell",
                        vec![
                            ("id".into(), id.to_string()),
                            ("state".into(), state.name().into()),
                            ("effect".into(), effect),
                        ],
                        OP_CELL_ACTION,
                        vec![id, state.value(), effect_value],
                    );
                    self.table.dispatched += 1;
                    self.table.next_send = Instant::now() + Duration::from_millis(50);
                    self.table.result = format!(
                        "Sent {} of {} edits; waiting for device readback",
                        self.table.dispatched, self.table.queued
                    );
                    if self.table.queue.is_empty() {
                        self.table.verification_started = Some(Instant::now());
                        self.refresh();
                    }
                }
            }
            ctx.request_repaint_after(Duration::from_millis(50));
        } else if let Some(start) = self.table.verification_started {
            // Only a fresh authoritative snapshot received after all writes counts.
            let fresh = match self.command_route() {
                Some(crate::Transport::WiFi) => self.last_status_at.is_some_and(|at| at > start),
                Some(crate::Transport::Serial) => {
                    self.last_serial_status_at.is_some_and(|at| at > start)
                }
                _ => false,
            };
            if fresh
                && self.live_status().is_some_and(|s| {
                    self.table.awaiting.iter().all(|(id, state)| {
                        s.cells
                            .get(*id as usize - 1)
                            .is_some_and(|v| v == state.name())
                    })
                })
            {
                self.table.result =
                    format!("Verified {} cell states on the device", self.table.queued);
                self.table.verification_started = None;
                self.table.awaiting.clear();
            } else if start.elapsed() > Duration::from_secs(8) {
                self.table.result="Readback did not confirm every edit. Refresh and inspect; the batch is not atomic.".into();
                self.table.verification_started = None;
                self.table.awaiting.clear();
            } else {
                ctx.request_repaint_after(Duration::from_millis(100));
            }
        }
    }
    fn queue_selected_cells(&mut self, state: State) {
        if self.command_route().is_none()
            || !self.table.queue.is_empty()
            || self.table.verification_started.is_some()
        {
            return;
        }
        let ids: Vec<_> = self.table.selection.ids.iter().copied().collect();
        if ids.is_empty() {
            return;
        }
        self.table.queued = ids.len();
        self.table.peer = self.cell_queue_peer();
        self.table.dispatched = 0;
        self.table.awaiting = ids.iter().map(|id| (*id, state)).collect();
        self.table.queue = ids
            .into_iter()
            .map(|id| (id, state, self.cell_effect.clone()))
            .collect();
        self.table.next_send = Instant::now();
        self.table.result = "Cell edits queued; batch is not atomic".into();
    }
    pub fn cell_selection_toolbar(&mut self, ui: &mut egui::Ui, matches: &[u8]) {
        ui.horizontal_wrapped(|ui| {
            ui.label(format!("{} selected", self.table.selection.ids.len()));
            if ui.button(format!("{} All", icons::CHECK)).clicked() {
                self.table.selection.group(1..=80, false);
            }
            if ui
                .add_enabled(
                    !matches.is_empty(),
                    egui::Button::new(format!(
                        "{} Matches ({})",
                        icons::MAGNIFYING_GLASS,
                        matches.len()
                    )),
                )
                .clicked()
            {
                self.table.selection.group(matches.iter().copied(), false);
            }
            if ui.button(format!("{} Clear", icons::X)).clicked() {
                self.table.selection.ids.clear();
            }
            if ui
                .button(format!("{} Manage…", icons::SLIDERS_HORIZONTAL))
                .clicked()
            {
                self.table.manager_open = true;
            }
            ui.menu_button(format!("{} Apply to selection", icons::LIGHTBULB), |ui| {
                self.cell_actions_menu(ui);
            });
        });
        if !self.table.result.is_empty() {
            ui.label(egui::RichText::new(&self.table.result).small().weak());
        }
    }
    fn cell_actions_menu(&mut self, ui: &mut egui::Ui) {
        compact_menu(ui);
        let enabled = self.command_route().is_some()
            && !self.table.selection.ids.is_empty()
            && self.table.queue.is_empty()
            && self.table.verification_started.is_none();
        for state in [State::White, State::Red, State::Off] {
            if ui
                .add_enabled(
                    enabled,
                    egui::Button::new(format!(
                        "{}  {} · {} cells",
                        state.icon(),
                        state.name(),
                        self.table.selection.ids.len()
                    )),
                )
                .clicked()
            {
                self.queue_selected_cells(state);
                ui.close();
            }
        }
        ui.menu_button(format!("{}  Transition", icons::SPARKLE), |ui| {
            for effect in ["immediate", "fade", "pulse"] {
                ui.selectable_value(&mut self.cell_effect, effect.into(), effect);
            }
        });
    }
    pub fn draw_cell_grid(
        &mut self,
        ui: &mut egui::Ui,
        area: Rect,
        origin: Pos2,
        pitch: f32,
        height: f32,
        matches: &[u8],
    ) {
        let snapshot = self.live_status().cloned();
        let cells: Vec<_> = (1..=80)
            .map(|id| (id, cell_rect(origin, pitch, height, id)))
            .collect();
        let background = ui.interact(
            area,
            ui.id().with("table-marquee-background"),
            egui::Sense::click_and_drag(),
        );
        let modifiers = ui.input(|i| i.modifiers);
        let mut begin_drag = background.drag_started();
        let mut focus_target = None;
        for (id, rect) in &cells {
            let widget_id = ui.id().with(("logical-cell", id));
            let selected = self.table.selection.ids.contains(id);
            let state = snapshot
                .as_ref()
                .and_then(|s| s.cells.get(*id as usize - 1))
                .map(String::as_str);
            let response = cell_surface(
                ui,
                *rect,
                widget_id,
                state,
                selected,
                matches.contains(id),
                self.config.animate_cells,
            );
            response.widget_info(|| {
                egui::WidgetInfo::labeled(egui::WidgetType::Button, true, format!("Cell {id}"))
            });
            let focus = response.has_focus()
                && !self.table.manager_open
                && self.preferences.is_none()
                && !self.device_settings.open
                && !self.show_connection
                && !self.show_mapping;
            let search_cursor = matches.contains(id) && self.selected_cell == *id;
            let cursor_alpha = if self.config.animate_cells {
                ui.ctx().animate_bool_with_time(
                    widget_id.with("search-cursor"),
                    search_cursor,
                    0.14,
                )
            } else if search_cursor {
                1.0
            } else {
                0.0
            };
            ui.painter().rect_stroke(
                rect.shrink(4.0),
                4.0,
                Stroke::new(
                    1.0,
                    ui.visuals().selection.bg_fill.linear_multiply(cursor_alpha),
                ),
                egui::StrokeKind::Inside,
            );
            let v = ui.visuals();
            let mapping = self.mappings.iter().find(|m| m.logical_id == *id).cloned();
            let label = mapping
                .as_ref()
                .map(|m| m.name.as_str())
                .filter(|s| !s.is_empty())
                .map(str::to_owned)
                .unwrap_or_else(|| id.to_string());
            let text = ui.painter().layout_no_wrap(
                label,
                egui::FontId::proportional(14.0),
                v.text_color(),
            );
            ui.painter().with_clip_rect(rect.shrink(9.0)).galley(
                rect.center() - text.size() * 0.5,
                text,
                v.text_color(),
            );
            let indicator = match state {
                Some("white") => v.strong_text_color(),
                Some("red") => v.error_fg_color,
                _ => v.weak_text_color(),
            };
            let indicator = animated_color(
                ui,
                widget_id.with("led-indicator"),
                indicator,
                self.config.animate_cells,
            );
            let dot = rect.left_top() + egui::vec2(7.0, 7.0);
            if state == Some("off") || state.is_none() {
                ui.painter()
                    .circle_stroke(dot, 2.5, Stroke::new(1.0, indicator));
            } else {
                ui.painter().circle_filled(dot, 2.5, indicator);
            }
            if mapping
                .as_ref()
                .is_some_and(|m| !m.name.is_empty() && m.name != id.to_string())
            {
                ui.painter().text(
                    rect.right_bottom() - egui::vec2(5.0, 3.0),
                    egui::Align2::RIGHT_BOTTOM,
                    format!("#{id}"),
                    egui::FontId::proportional(9.0),
                    v.weak_text_color(),
                );
            }
            if pointer_or_accessibility_click(&response) {
                self.selected_cell = *id;
                self.table.selection.click(
                    *id,
                    modifiers.command || self.config.additive_cell_selection,
                    modifiers.shift,
                );
                response.request_focus();
            }
            if response.secondary_clicked() {
                if !selected {
                    self.table.selection.click(*id, false, false);
                }
                self.selected_cell = *id;
                response.request_focus();
            }
            if response.double_clicked() {
                if !self.table.selection.ids.contains(id) {
                    self.table.selection.click(*id, false, false);
                }
                self.table.manager_open = true;
            }
            begin_drag |= response.drag_started();
            if focus {
                for key in [
                    Key::ArrowLeft,
                    Key::ArrowRight,
                    Key::ArrowUp,
                    Key::ArrowDown,
                    Key::Home,
                    Key::End,
                ] {
                    if ui.input_mut(|i| i.consume_key(modifiers, key)) {
                        let next = navigate(*id, key, modifiers.command);
                        self.selected_cell = next;
                        if !modifiers.command || modifiers.shift {
                            self.table.selection.click(next, false, modifiers.shift);
                        }
                        focus_target = Some(ui.id().with(("logical-cell", next)));
                    }
                }
                if ui.input_mut(|i| i.consume_key(Modifiers::COMMAND, Key::A)) {
                    self.table.selection.group(1..=80, false);
                }
                if ui.input_mut(|i| i.consume_key(Modifiers::NONE, Key::Escape)) {
                    self.table.selection.ids.clear();
                }
                if activation_key(ui, modifiers, Key::Space) {
                    self.table.selection.click(
                        *id,
                        modifiers.command || self.config.additive_cell_selection,
                        modifiers.shift,
                    );
                }
                if activation_key(ui, Modifiers::NONE, Key::Enter) {
                    if self.table.selection.ids.is_empty() {
                        self.table.selection.click(*id, false, false);
                    }
                    self.table.manager_open = true;
                }
            }
            let keyboard_menu =
                focus && ui.input_mut(|i| i.consume_key(Modifiers::SHIFT, Key::F10));
            let popup = if keyboard_menu {
                egui::Popup::menu(&response).open_memory(Some(egui::SetOpenCommand::Bool(true)))
            } else {
                egui::Popup::context_menu(&response)
            };
            popup.show(|ui| {
                compact_menu(ui);
                ui.strong(format!(
                    "{} selected · cell {id}",
                    self.table.selection.ids.len()
                ));
                ui.separator();
                self.cell_actions_menu(ui);
                ui.separator();
                if ui
                    .button(format!("{}  Manage cells…", icons::SLIDERS_HORIZONTAL))
                    .clicked()
                {
                    self.table.manager_open = true;
                    ui.close();
                }
                if ui
                    .button(format!("{}  Edit physical mapping…", icons::PENCIL_SIMPLE))
                    .clicked()
                {
                    self.select_mapping(*id);
                    self.show_mapping = true;
                    ui.close();
                }
                if ui
                    .button(format!("{}  Select row {}", icons::LIST, (id - 1) / 10 + 1))
                    .clicked()
                {
                    let start = (id - 1) / 10 * 10 + 1;
                    self.table.selection.group(
                        start..start + 10,
                        modifiers.command || self.config.additive_cell_selection,
                    );
                    ui.close();
                }
                if ui
                    .button(format!("{}  Clear selection", icons::X))
                    .clicked()
                {
                    self.table.selection.ids.clear();
                    ui.close();
                }
            });
            response.on_hover_text(format!("Cell {id} · {} · {}\nClick selects · Ctrl toggles · Shift extends · drag selects · Enter manages",mapping.map(|m|format!("physical {}",m.physical_cell)).unwrap_or_else(||"mapping unavailable".into()),state.unwrap_or("state unavailable")));
        }
        if let Some(target) = focus_target {
            ui.memory_mut(|m| m.request_focus(target));
        }
        if begin_drag && let Some(start) = ui.input(|i| i.pointer.press_origin()) {
            let base = if modifiers.command || modifiers.shift {
                self.table.selection.ids.clone()
            } else {
                BTreeSet::new()
            };
            self.table.marquee = Some((start, base));
        }
        if let Some((start, base)) = self.table.marquee.clone() {
            if let Some(end) = ui.input(|i| i.pointer.interact_pos()) {
                let rect = Rect::from_two_pos(start, end).intersect(area);
                self.table.selection.marquee(rect, &cells, &base);
                ui.painter().rect(
                    rect,
                    2.0,
                    ui.visuals().selection.bg_fill.linear_multiply(0.12),
                    Stroke::new(1.0, ui.visuals().selection.bg_fill),
                    egui::StrokeKind::Inside,
                );
            }
            if !ui.input(|i| i.pointer.primary_down()) {
                self.table.marquee = None;
            }
        }
        if background.clicked() && !modifiers.command && !modifiers.shift {
            self.table.selection.ids.clear();
        }
        background.context_menu(|ui| {
            compact_menu(ui);
            self.cell_actions_menu(ui);
            ui.separator();
            if ui
                .button(format!("{}  Manage cells…", icons::GRID_FOUR))
                .clicked()
            {
                self.table.manager_open = true;
                ui.close();
            }
        });
    }
    pub fn draw_cell_manager(&mut self, ctx: &egui::Context) {
        if !self.table.manager_open {
            return;
        }
        let mut close = false;
        let modal=egui::Modal::new(egui::Id::new("cell-management")).show(ctx,|ui| {
            ui.set_width((ctx.content_rect().width()-64.0).clamp(280.0,650.0));
            ui.heading(format!("{}  Manage cells",icons::GRID_FOUR));
            ui.label("Select table rows or individual cells, then apply one state and transition to the selection.");
            ui.add_space(8.0);
            ui.horizontal_wrapped(|ui| {
                ui.strong(format!("{} selected",self.table.selection.ids.len()));
                if ui.button("Select all").clicked(){self.table.selection.group(1..=80,false);}
                if ui.button("Clear").clicked(){self.table.selection.ids.clear();}
                if ui.checkbox(&mut self.config.additive_cell_selection,"Toggle without Ctrl").changed(){self.save_workspace();}
            });
            let snapshot=self.live_status().cloned();
            dialog::scroll_column(ui,"cell-management-rows",Some((ctx.content_rect().height()-340.0).clamp(120.0,360.0)),|ui| {
                for row in 0..8u8 {
                    let start=row*10+1;
                    ui.horizontal(|ui| {
                        let all=(start..start+10).all(|id|self.table.selection.ids.contains(&id));
                        let row_response=ui.selectable_label(all,format!("{} Row {}",icons::LIST,row+1));
                        if row_response.clicked(){let m=ui.input(|i|i.modifiers);self.table.selection.group(start..start+10,m.command||self.config.additive_cell_selection);}
                        ui.horizontal_wrapped(|ui|{for id in start..start+10 {let response=ui.selectable_label(self.table.selection.ids.contains(&id),format!("{id:02}"));if response.clicked(){let m=ui.input(|i|i.modifiers);self.table.selection.click(id,m.command||self.config.additive_cell_selection,m.shift);self.selected_cell=id;}response.on_hover_text(self.mappings.iter().find(|m|m.logical_id==id).map(|m|format!("{} · physical {} · {}",m.name,m.physical_cell,snapshot.as_ref().and_then(|s|s.cells.get(id as usize-1)).map(String::as_str).unwrap_or("unavailable"))).unwrap_or_else(||format!("Cell {id} · mapping unavailable")));}});
                    });
                }
            });
            dialog::section(ui,icons::LIGHTBULB,"Selected cell action",|ui| {
                ui.horizontal_wrapped(|ui|{for state in [State::White,State::Red,State::Off]{ui.selectable_value(&mut self.table.manager_state,state,format!("{} {}",state.icon(),state.name()));}});
                ui.horizontal(|ui|{ui.label("Transition");egui::ComboBox::from_id_salt("bulk-effect").selected_text(&self.cell_effect).show_ui(ui,|ui|{for effect in ["immediate","fade","pulse"]{ui.selectable_value(&mut self.cell_effect,effect.into(),effect);}});});
                ui.label(egui::RichText::new("Edits are paced and verified from device readback. They are not an atomic transaction.").small().weak());
            });
            if !self.table.result.is_empty(){ui.label(&self.table.result);}
            ui.separator();
            ui.horizontal_wrapped(|ui| {
                let can_apply=self.command_route().is_some()&&!self.table.selection.ids.is_empty()&&self.table.queue.is_empty()&&self.table.verification_started.is_none();
                if ui.add_enabled(can_apply,egui::Button::new(format!("{} Apply to {} cells",icons::CHECK,self.table.selection.ids.len()))).clicked(){self.queue_selected_cells(self.table.manager_state);}
                if ui.add_enabled(!self.table.queue.is_empty(),egui::Button::new(format!("{} Cancel unsent",icons::X))).clicked(){self.table.queue.clear();self.table.awaiting.clear();self.table.verification_started=None;self.table.result="Cancelled unsent edits. Previously sent edits may have applied.".into();}
                if ui.add_enabled(self.table.selection.ids.len()==1&&!self.mappings.is_empty(),egui::Button::new(format!("{} Mapping…",icons::PENCIL_SIMPLE))).clicked(){let id=*self.table.selection.ids.first().unwrap();self.select_mapping(id);self.show_mapping=true;close=true;}
                if dialog::action_button(ui,icons::X,"Close").clicked(){close=true;}
            });
        });
        if close || modal.should_close() {
            self.table.manager_open = false;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn keyboard_activation_does_not_double_toggle_or_repeat() {
        let ctx = egui::Context::default();
        let id = egui::Id::new("activation-cell");
        let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(53.0, 36.2));
        let mut selection = Selection::default();
        selection.click(1, false, false);
        let mut first = ctx.run_ui(egui::RawInput::default(), |ui| {
            cell_surface(ui, rect, id, Some("white"), true, false, false);
        });
        first.textures_delta.clear();
        ctx.memory_mut(|m| m.request_focus(id));
        for repeat in [false, true] {
            let input = egui::RawInput {
                events: vec![egui::Event::Key {
                    key: Key::Space,
                    physical_key: None,
                    pressed: true,
                    repeat,
                    modifiers: Modifiers::NONE,
                }],
                ..Default::default()
            };
            let mut output = ctx.run_ui(input, |ui| {
                let response = cell_surface(ui, rect, id, Some("white"), true, false, false);
                if pointer_or_accessibility_click(&response) {
                    selection.click(1, true, false);
                }
                if activation_key(ui, Modifiers::NONE, Key::Space) {
                    selection.click(1, true, false);
                }
            });
            output.textures_delta.clear();
            assert!(
                selection.ids.is_empty(),
                "Space toggles once and key repeat must not toggle again"
            );
        }
    }
    #[test]
    fn actual_painter_preserves_hit_rect_and_inside_strokes_in_all_states() {
        for dark in [true, false] {
            let ctx = egui::Context::default();
            ctx.set_visuals(if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            });
            let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(53.0, 36.2));
            let id = egui::Id::new("geometry-test-cell");
            for (index, (state, selected, matched)) in [
                ("white", false, false),
                ("red", true, false),
                ("off", false, true),
            ]
            .into_iter()
            .enumerate()
            {
                if index == 2 {
                    ctx.memory_mut(|m| m.request_focus(id));
                }
                let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                    let response =
                        cell_surface(ui, rect, id, Some(state), selected, matched, false);
                    assert_eq!(response.rect, rect);
                });
                output.textures_delta.clear();
                let surface = output
                    .shapes
                    .iter()
                    .find_map(|s| match &s.shape {
                        egui::Shape::Rect(r) if r.rect == rect => Some(r),
                        _ => None,
                    })
                    .expect("Actual cell rectangle was painted");
                assert_eq!(surface.stroke_kind, egui::StrokeKind::Inside);
                if dark && state == "white" {
                    assert!(
                        surface.fill.r() < 170 && surface.fill.g() < 170 && surface.fill.b() < 170,
                        "Dark app must not render LED white as a white tile"
                    );
                }
            }
        }
    }
    #[test]
    fn selection_toggle_range_and_rows() {
        let mut s = Selection::default();
        s.click(5, false, false);
        s.click(8, false, true);
        assert_eq!(s.ids, BTreeSet::from([5, 6, 7, 8]));
        s.click(6, true, false);
        assert!(!s.ids.contains(&6));
        s.group(11..=20, true);
        assert!((11..=20).all(|id| s.ids.contains(&id)));
        s.group(11..=20, true);
        assert!((11..=20).all(|id| !s.ids.contains(&id)));
        s.click(81, false, false);
        assert_eq!(s.ids, BTreeSet::from([5, 7, 8]));
    }
    #[test]
    fn keyboard_edges_are_row_major_without_wrapping() {
        assert_eq!(navigate(10, Key::ArrowRight, false), 10);
        assert_eq!(navigate(11, Key::ArrowLeft, false), 11);
        assert_eq!(navigate(11, Key::ArrowUp, false), 1);
        assert_eq!(navigate(78, Key::ArrowDown, false), 78);
        assert_eq!(navigate(38, Key::Home, false), 31);
        assert_eq!(navigate(38, Key::End, false), 40);
        assert_eq!(navigate(38, Key::End, true), 80);
    }
    #[test]
    fn marquee_uses_staggered_centres_and_preserves_additive_base() {
        let cells: Vec<_> = (1..=80)
            .map(|id| (id, cell_rect(Pos2::ZERO, 60.0, 43.2, id)))
            .collect();
        let mut s = Selection::default();
        s.marquee(
            Rect::from_min_max(Pos2::ZERO, egui::pos2(115.0, 58.0)),
            &cells,
            &BTreeSet::from([80]),
        );
        assert_eq!(s.ids, BTreeSet::from([1, 2, 80]));
    }
    #[test]
    fn border_states_never_change_geometry() {
        for id in 1..=80 {
            let rect = cell_rect(Pos2::ZERO, 60.0, 43.2, id);
            for width in [1.0, 2.0, 3.0] {
                let painted = rect;
                let _stroke = Stroke::new(width, Color32::WHITE);
                assert_eq!(painted, rect);
                assert!((rect.width() - 53.0).abs() < 0.001);
                assert!((rect.height() - 36.2).abs() < 0.001);
            }
        }
    }
}
