//! Fixed-geometry table interaction; selection never mutates hardware.
//! Uses Pealayer's painted interaction targets and existing imported dialog/icons.
use super::{dialog, icons};
use super::dropdown::{ComboBox, DropdownUiExt};
use crate::PakarControllerApp;
use eframe::egui::{self, Color32, Key, Modifiers, Pos2, Rect, Stroke};
use std::{
    collections::{BTreeSet, VecDeque},
    time::{Duration, Instant},
};

fn local_pointer(ui: &egui::Ui, position: Pos2) -> Pos2 {
    ui.ctx().layer_transform_from_global(ui.layer_id())
        .map_or(position, |transform| transform * position)
}

#[derive(Default, Clone, Debug)]
pub struct Selection {
    pub ids: BTreeSet<u8>,
    anchor: Option<u8>,
    revision: u64,
}
impl Selection {
    /// Find is an input adapter, not a second set of highlighted cells.
    pub fn replace_query(&mut self, query: &str) -> Result<(), String> {
        let ids = super::cell_search::parse(query)?;
        self.group(ids, false);
        self.anchor = self.ids.first().copied();
        Ok(())
    }
    pub fn cells(&self) -> Vec<u8> {
        self.ids.iter().copied().collect()
    }
    pub fn navigate(&self, current: u8, backwards: bool) -> Option<u8> {
        super::cell_search::navigate(&self.cells(), current, backwards)
    }
    pub fn click(&mut self, id: u8, toggle: bool, extend: bool) {
        if !(1..=80).contains(&id) {
            return;
        }
        self.revision = self.revision.wrapping_add(1);
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
        self.revision = self.revision.wrapping_add(1);
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
        let previous = self.ids.clone();
        self.ids = base.clone();
        self.ids.extend(
            cells
                .iter()
                .filter(|(_, cell)| rect.contains(cell.center()))
                .map(|(id, _)| *id),
        );
        if self.ids != previous {
            self.revision = self.revision.wrapping_add(1);
        }
    }
    pub fn clear(&mut self) {
        self.ids.clear();
        self.anchor = None;
        self.revision = self.revision.wrapping_add(1);
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum State {
    White,
    Red,
    Won,
    Off,
}
impl State {
    fn value(self) -> u8 {
        match self {
            Self::White => 0,
            Self::Red => 1,
            Self::Won => 3,
            Self::Off => 2,
        }
    }
    fn name(self) -> &'static str {
        match self {
            Self::White => "white",
            Self::Red => "red",
            Self::Won => "won",
            Self::Off => "off",
        }
    }
    fn label(self) -> &'static str {
        match self {Self::White=>"Idle",Self::Red=>"Lost",Self::Won=>"Won",Self::Off=>"Off"}
    }
    fn icon(self) -> &'static str {
        match self {
            Self::White => icons::SUN,
            Self::Red => icons::LIGHTBULB,
            Self::Won => icons::CHECK,
            Self::Off => icons::EYE_SLASH,
        }
    }
}
pub struct TableState {
    /// None follows the dock viewport; manual zoom is client-local, never a mapping edit.
    pub viewport: Option<Rect>,
    assignment_import: Option<(crate::cell_assignments::Document, crate::peer_http::Pin)>,
    assignment_export: Option<(std::path::PathBuf, String, crate::peer_http::Pin, Instant)>,
    pub pixel_sample: Option<super::cell_pixels::Sample>,
    pub editing_order: bool,
    order_drag: Option<(u8, bool)>,
    pub selection: Selection,
    reflected_revision: u64,
    pub manager_open: bool,
    manager_state: State,
    marquee: Option<(Pos2, BTreeSet<u8>)>,
    queue: VecDeque<(Vec<u8>, State, String)>,
    peer: Option<(crate::Transport, crate::Endpoint, String)>,
    next_send: Instant,
    queued: usize,
    dispatched: usize,
    awaiting: Vec<(u8, State)>,
    verification_started: Option<Instant>,
    acknowledged: bool,
    expected_batch: Option<crate::cell_batch::Correlation>,
    result: String,
}
impl Default for TableState {
    fn default() -> Self {
        Self {
            viewport: None,
            assignment_import: None,
            assignment_export: None,
            pixel_sample: None,
            editing_order: false,
            order_drag: None,
            selection: Default::default(),
            reflected_revision: 0,
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
            acknowledged: false,
            expected_batch: None,
            result: String::new(),
        }
    }
}
impl TableState {
    pub(super) fn feedback(&self) -> &str {
        &self.result
    }
    pub(crate) fn confirm_batch_ack(&mut self, receipt: &crate::cell_batch::Correlation) {
        if self.verification_started.is_some()
            && self.expected_batch.as_ref() == Some(receipt)
            && self.awaiting.len() == receipt.request.ids.len()
            && self.awaiting.iter().all(|(id, state)| {
                receipt.request.ids.contains(id) && crate::cell_assignments::same_state(state.name(), &receipt.request.state)
            })
        {
            self.acknowledged = true;
        }
    }
    pub(crate) fn fail_pending_edits(&mut self, message: String) {
        self.stop_pending_edits();
        self.result = message;
    }
    /// Revoke unsent work and old readback ownership on peer/capability loss.
    /// Sent writes are never retried or represented as rolled back.
    pub(crate) fn has_pending_edits(&self) -> bool {
        !self.queue.is_empty() || self.verification_started.is_some()
    }
    pub(crate) fn has_assignment_draft(&self) -> bool {
        self.assignment_import.is_some()
    }
    pub(crate) fn stop_pending_edits(&mut self) {
        let pending = !self.queue.is_empty() || self.verification_started.is_some();
        self.queue.clear();
        self.awaiting.clear();
        self.verification_started = None;
        self.acknowledged = false;
        self.expected_batch = None;
        self.peer = None;
        if pending {
            self.result = "Stopped: peer or capabilities changed. Previously sent edits may have applied; inspect device state.".into();
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
    let (mut row, mut col) = slot_position(id.clamp(1, 80) - 1);
    match key {
        Key::ArrowLeft => col = col.saturating_sub(1),
        Key::ArrowRight => col = (col + 1).min(19),
        Key::ArrowUp => row = row.saturating_sub(1),
        Key::ArrowDown => row = (row + 1).min(3),
        Key::Home if ctrl => {
            row = 0;
            col = 0;
        }
        Key::End if ctrl => {
            row = 3;
            col = 19;
        }
        Key::Home => col = 0,
        Key::End => col = 19,
        _ => {}
    }
    position_slot(row, col) + 1
}
fn slot_position(slot: u8) -> (u8, u8) {
    (slot / 20, 2 * (slot % 10) + (slot / 10) % 2)
}
fn position_slot(row: u8, col: u8) -> u8 {
    row * 20 + col / 2 + (col % 2) * 10
}
fn visual_line(order: &[u8], slot: usize, row: bool) -> Vec<u8> {
    let (selected_row, selected_column) = slot_position(slot as u8);
    order.iter().enumerate().filter_map(|(index, id)| {
        let (r, c) = slot_position(index as u8);
        (if row { r == selected_row } else { c == selected_column }).then_some(*id)
    }).collect()
}
fn action_unavailable(connected: bool, supported: bool, selected: usize, pending: bool,
    state_unsupported: bool, locked: bool) -> Option<&'static str> {
    if !connected { Some("Connect to the board before changing its cells.") }
    else if !supported || state_unsupported { Some("The connected board has not advertised this action.") }
    else if selected == 0 { Some("Select one or more cells first.") }
    else if pending { Some("Waiting for the current batch to be confirmed; no duplicate command will be sent.") }
    else if locked { Some("Lost cells are locked. Unlock them before returning the selection to idle.") }
    else { None }
}
pub fn normalized_order(saved: &[u8]) -> Vec<u8> {
    let mut seen = BTreeSet::new();
    saved
        .iter()
        .copied()
        .chain(1..=80)
        .filter(|id| (1..=80).contains(id) && seen.insert(*id))
        .collect()
}
/// Adapt Pealayer reordered_channel_rank: remove source, locate the remaining
/// target, insert before/after. This is presentation only, never a device write.
pub fn reorder_cell(
    order: &mut Vec<u8>,
    editing: bool,
    source: u8,
    target: u8,
    before: bool,
) -> bool {
    if !editing || source == target || order.len() != 80 || normalized_order(order) != *order {
        return false;
    }
    let (Some(from), Some(_)) = (
        order.iter().position(|id| *id == source),
        order.iter().position(|id| *id == target),
    ) else {
        return false;
    };
    let old = order.clone();
    order.remove(from);
    let to = order.iter().position(|id| *id == target).unwrap() + usize::from(!before);
    order.insert(to, source);
    *order != old
}
fn reorder_visual_cell(
    order: &mut Vec<u8>,
    editing: bool,
    source: u8,
    target: u8,
    before: bool,
) -> bool {
    if order.len() != 80 {
        return false;
    }
    let mut visual: Vec<_> = (0..80)
        .map(|i| order[position_slot(i / 20, i % 20) as usize])
        .collect();
    if !reorder_cell(&mut visual, editing, source, target, before) {
        return false;
    }
    for (i, id) in visual.into_iter().enumerate() {
        order[position_slot(i as u8 / 20, i as u8 % 20) as usize] = id;
    }
    true
}
fn navigate_order(order: &[u8], id: u8, key: Key, ctrl: bool) -> u8 {
    let Some(slot) = order.iter().position(|cell| *cell == id) else {
        return id;
    };
    order[navigate(slot as u8 + 1, key, ctrl) as usize - 1]
}
pub fn cell_rect(origin: Pos2, pitch: f32, height: f32, id: u8) -> Rect {
    let (row, col) = slot_position(id.clamp(1, 80) - 1);
    Rect::from_min_size(
        origin
            + egui::vec2(
                col as f32 * pitch,
                row as f32 * height + if col % 2 == 1 { height * 0.5 } else { 0.0 },
            ),
        egui::vec2(pitch - 7.0, height - 7.0),
    )
}
pub(crate) fn grid_metrics(width: f32, visible_height: f32, rounded: bool) -> (f32, f32) {
    let aspect = if rounded { 0.72 } else { 1.0 };
    // Adapt Pealayer navigation_control's minimum hit size + scroll_column
    // overflow contract: a short dock viewport must scroll, never miniaturize
    // eighty two-digit interaction targets to illegible postage stamps.
    let pitch = (width / 20.0)
        .min(visible_height / (4.5 * aspect))
        .clamp(64.0, 94.0);
    (pitch, pitch * aspect)
}
/// Fit the painted bounds, including the half-row stagger and focus outline.
/// Keeping one source of geometry prevents the footer/fit calculation from
/// drifting away from the eighty interaction targets.
pub(super) fn grid_canvas(pitch: f32, height: f32) -> (Rect, Pos2) {
    let origin = egui::pos2(8.0, 8.0);
    let cells = (1..=80).fold(Rect::NOTHING, |bounds, id| {
        bounds.union(cell_rect(origin, pitch, height, id))
    });
    (Rect::from_min_max(Pos2::ZERO, cells.max + egui::vec2(8.0, 8.0)), origin)
}

/// Reserve only the three navigation buttons and their gaps, not an arbitrary
/// fixed-width search field inside an otherwise empty desktop row.
pub(super) fn find_row_width(available: f32, control_height: f32, gap: f32) -> f32 {
    (available - 3.0 * (control_height + gap)).max(1.0)
}

/// Pealayer's fixed editor slot and center-aligned glyph contract, adapted to
/// Find. The icon occupies the editor's leading margin instead of an unrelated
/// label baseline; hover/focus never changes its allocation.
pub(super) fn find_field(ui: &mut egui::Ui, query: &mut String, width: f32) -> egui::Response {
    let height = super::control_density::height(ui).max(30.0);
    let response = ui.add_sized(
        [width.max(1.0), height],
        dialog::singleline_text_edit(ui, query)
            .id(egui::Id::new("table-cell-search"))
            .vertical_align(egui::Align::Center)
            .margin(egui::Margin { left: height as i8, right: 8, top: 3, bottom: 3 })
            .hint_text(super::i18n::text("Find cell: 12, 3–8"))
            .char_limit(256),
    );
    ui.painter().with_clip_rect(response.rect.intersect(ui.clip_rect())).text(
        egui::pos2(response.rect.left() + height * 0.5, response.rect.center().y),
        egui::Align2::CENTER_CENTER,
        icons::MAGNIFYING_GLASS,
        egui::FontId::proportional(16.0),
        ui.visuals().weak_text_color(),
    );
    response
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(super) enum HelperTone { Normal, Caution, Error }

/// Allocate the same one-line lane whether empty, informative or actionable.
/// Full text remains available on hover when the dock is narrow.
pub(super) fn table_helper(ui: &mut egui::Ui, text: &str, tone: HelperTone) -> egui::Response {
    let color = match tone {
        HelperTone::Normal => ui.visuals().weak_text_color(),
        HelperTone::Caution => ui.visuals().warn_fg_color,
        HelperTone::Error => ui.visuals().error_fg_color,
    };
    let caption = if text.is_empty() || tone == HelperTone::Normal {
        text.to_owned()
    } else {
        format!("{}  {text}", icons::WARNING)
    };
    let (rect, response) = ui.allocate_exact_size(
        egui::vec2(ui.available_width().max(1.0), ui.text_style_height(&egui::TextStyle::Small).max(18.0)),
        egui::Sense::hover(),
    );
    if !caption.is_empty() {
        // Ui::put centers a shrinking Label inside the allocated area even
        // with halign(Min). Paint the measured, ellipsized galley at the lane's
        // leading edge instead; allocation and text alignment stay separate.
        let mut job = egui::text::LayoutJob::simple(caption,
            egui::TextStyle::Small.resolve(ui.style()), color, rect.width());
        job.wrap.max_rows = 1;
        job.wrap.break_anywhere = true;
        let galley = ui.painter().layout_job(job);
        let position = egui::pos2(rect.left(), rect.center().y - galley.size().y * 0.5);
        ui.painter().with_clip_rect(rect.intersect(ui.clip_rect())).galley(position, galley, color);
    }
    response.on_hover_text(text)
}
fn semantic_colors(
    base: Color32,
    state: Option<&str>,
    visuals: &egui::Visuals,
) -> (Color32, Color32, &'static str) {
    match state {
        Some("white" | "idle") => (
            blend(base, visuals.strong_text_color(), 0.08),
            visuals.strong_text_color(),
            "WHITE",
        ),
        Some("red" | "lost") => (
            blend(base, visuals.error_fg_color, 0.22),
            visuals.error_fg_color,
            "RED",
        ),
        Some("off") => (
            blend(base, Color32::BLACK, 0.35),
            visuals.weak_text_color(),
            "OFF",
        ),
        Some("won") => (
            blend(base, Color32::from_rgb(63,185,111), 0.18),
            Color32::from_rgb(63,185,111),
            "WON",
        ),
        _ => (base, visuals.weak_text_color(), "UNKNOWN"),
    }
}
/// Board-assigned semantic RGB is presentation, never an inferred pixel frame.
/// Unknown states remain neutral; absent/malformed palette entries use only the
/// well-defined default for a known state.
fn state_fill(state: Option<&str>, presentation: Option<&serde_json::Value>) -> Option<Color32> {
    let (name, fallback) = match state?.trim() {
        "white" | "idle" => ("idle", 0xffffff),
        "red" | "lost" => ("lost", 0xff0000),
        "won" => ("won", 0x00ff00),
        "off" => ("off", 0x000000),
        _ => return None,
    };
    let rgb = presentation
        .and_then(|value| value["states"][name]["rgb"].as_u64())
        .filter(|rgb| *rgb <= 0xffffff)
        .unwrap_or(fallback);
    Some(Color32::from_rgb((rgb >> 16) as u8, (rgb >> 8) as u8, rgb as u8))
}
fn semantic_caption(state: Option<&str>, color: Option<Color32>) -> &'static str {
    let semantic = match state.map(str::trim) {
        Some("white" | "idle") => "IDLE",
        Some("red" | "lost") => "LOST",
        Some("won") => "WON",
        Some("off") => "OFF",
        _ => return "UNKNOWN",
    };
    match color.map(|c| [c.r(), c.g(), c.b()]) {
        Some([255, 255, 255]) => "WHITE",
        Some([255, 0, 0]) => "RED",
        Some([0, 255, 0]) => "GREEN",
        Some([0, 0, 255]) => "BLUE",
        Some([255, 255, 0]) => "YELLOW",
        Some([0, 255, 255]) => "CYAN",
        Some([255, 0, 255]) => "MAGENTA",
        Some([0, 0, 0]) if semantic == "OFF" => "OFF",
        Some([0, 0, 0]) => "BLACK",
        _ => semantic,
    }
}
fn luminance(color: Color32) -> f32 {
    let linear = |value: u8| {
        let value = f32::from(value) / 255.0;
        if value <= 0.04045 { value / 12.92 } else { ((value + 0.055) / 1.055).powf(2.4) }
    };
    linear(color.r()) * 0.2126 + linear(color.g()) * 0.7152 + linear(color.b()) * 0.0722
}
fn contrasting_ink(fill: Color32) -> Color32 {
    if luminance(fill) > 0.179 { Color32::BLACK } else { Color32::WHITE }
}
/// Adapt the donor's color-only hover contract without replacing a semantic
/// edge with an unrelated generic stroke. Hover increases contrast against its
/// painted fill; selection/focus retain their own stable identity.
pub(super) fn cell_outline_color(
    visuals: &egui::Visuals,
    resting: Color32,
    fill: Color32,
    selected: bool,
    focused: bool,
    hovered: bool,
) -> Color32 {
    if selected {
        visuals.warn_fg_color
    } else if focused {
        visuals.selection.stroke.color
    } else if hovered {
        blend(resting, contrasting_ink(fill), 0.28)
    } else {
        resting
    }
}
fn draw_semantic_cell(
    ui: &egui::Ui,
    response: &egui::Response,
    shape: super::cell_pixels::Shape,
    state: Option<&str>,
    label: &str,
    selected: bool,
    column_id: u8,
    animate: bool,
    solid_fill: Option<Color32>,
    caption: &'static str,
) {
    let v = ui.visuals();
    let rect = response.rect;
    let center = rect.center();
    let radius = (rect.width().min(rect.height()) * 0.5 - 3.0).max(1.0);
    let (mut fill, state_color, _) = semantic_colors(
        cell_base_fill(crate::config::live().color_palette, v.dark_mode, column_id),
        state,
        v,
    );
    if let Some(solid) = solid_fill { fill = solid; }
    let resting = if solid_fill.is_some() {
        blend(fill, contrasting_ink(fill), 0.35)
    } else {
        blend(fill, state_color, 0.65)
    };
    let line = cell_outline_color(v, resting, fill, selected, response.has_focus(),
        response.hovered() || response.is_pointer_button_down_on());
    let line = animated_color(ui, response.id.with("semantic-line"), line, animate);
    let fill = animated_color(ui, response.id.with("semantic-fill"), fill, animate);
    let ink = solid_fill.map_or(v.text_color(), |_| contrasting_ink(fill));
    let caption_color = solid_fill.map_or(state_color, |_| ink);
    let width = if selected || response.has_focus() {
        2.0
    } else {
        1.0
    };
    let stroke = Stroke::new(width, line);
    let painter = ui.painter().with_clip_rect(rect);
    if shape == super::cell_pixels::Shape::Circle {
        painter.circle_filled(center, radius, fill);
        painter.circle_stroke(center, radius, stroke);
    } else {
        painter.add(egui::Shape::convex_polygon(
            (0..6)
                .map(|i| super::cell_pixels::point(center, radius, i as f32 - 0.5))
                .collect(),
            fill,
            stroke,
        ));
    }
    let mut job = egui::text::LayoutJob::simple(
        label.into(),
        egui::FontId::proportional(14.0),
        ink,
        radius * 1.65,
    );
    job.wrap.max_rows = 1;
    let text = painter.layout_job(job);
    painter.galley(
        center - egui::vec2(text.size().x * 0.5, 12.0),
        text,
        ink,
    );
    painter.text(
        center + egui::vec2(0.0, 10.0),
        egui::Align2::CENTER_CENTER,
        caption,
        egui::FontId::proportional(8.0),
        caption_color,
    );
    // This is semantic state, not six fabricated sampled pixel RGB values.
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
pub(super) fn animated_color(
    ui: &egui::Ui,
    id: egui::Id,
    target: Color32,
    animate: bool,
) -> Color32 {
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

fn cell_base_fill(palette: super::palette::Palette, dark: bool, logical_id: u8) -> Color32 {
    // Row-major IDs: the same logical column keeps its shade on every row.
    let column = (logical_id.clamp(1, 80) - 1) % 10;
    super::palette::color(
        palette,
        dark,
        if column % 2 == 0 {
            "surface-0"
        } else {
            "surface-2"
        },
    )
}

// Paint strokes INSIDE a fixed interaction rectangle, never through widget padding.
fn cell_surface(
    ui: &mut egui::Ui,
    rect: Rect,
    id: egui::Id,
    logical_id: u8,
    _state: Option<&str>,
    selected: bool,
    animate: bool,
) -> egui::Response {
    cell_surface_with_fill(ui, rect, id, logical_id, selected, animate, None)
}
fn cell_surface_with_fill(
    ui: &mut egui::Ui,
    rect: Rect,
    id: egui::Id,
    logical_id: u8,
    selected: bool,
    animate: bool,
    solid_fill: Option<Color32>,
) -> egui::Response {
    let response = ui.interact(rect, id, egui::Sense::click_and_drag());
    let focus = response.has_focus();
    let v = ui.visuals();
    let base_fill = cell_base_fill(crate::config::live().color_palette, v.dark_mode, logical_id);
    let emphasized = selected;
    let mut fill = solid_fill.unwrap_or(base_fill);
    if response.hovered() && !emphasized && solid_fill.is_none() {
        fill = blend(
            fill,
            super::palette::color(
                crate::config::live().color_palette,
                v.dark_mode,
                "surface-3",
            ),
            0.65,
        );
    }
    if response.is_pointer_button_down_on() && !emphasized && solid_fill.is_none() {
        fill = super::palette::color(
            crate::config::live().color_palette,
            v.dark_mode,
            "surface-3",
        );
    }
    // One tint-free outline, independent of LED state, hover and focus.
    let resting = solid_fill.map_or(v.widgets.noninteractive.bg_stroke.color,
        |solid| blend(solid, contrasting_ink(solid), 0.35));
    let line = cell_outline_color(v, resting, fill, emphasized, focus,
        response.hovered() || response.is_pointer_button_down_on());
    let width = if selected || focus { 2.0 } else { 1.0 };
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
            .animate_bool_with_time(id.with("focus"), focus && !emphasized, 0.14)
    } else if focus && !emphasized {
        1.0
    } else {
        0.0
    };
    // Do not briefly retain a fading accent ring after selection begins.
    let focus_alpha = if emphasized { 0.0 } else { focus_alpha };
    ui.painter().rect_stroke(
        rect.shrink(4.0),
        4.0,
        Stroke::new(1.0, v.selection.stroke.color.linear_multiply(focus_alpha)),
        egui::StrokeKind::Inside,
    );
    response
}
/// Adapt Pealayer dialog::navigation_control's allocation/inside-stroke contract
/// to compact selectors. Use the table's single neutral selection surface, not
/// Button::selectable's state-dependent Frame margins. Text never follows stroke.
fn manager_selector(
    ui: &mut egui::Ui,
    label: &str,
    selected: bool,
    logical_id: u8,
    width: f32,
    animate: bool,
) -> egui::Response {
    let (id, rect) = ui.allocate_space(egui::vec2(width, dialog::ACTION_HEIGHT));
    let response = cell_surface(ui, rect, id, logical_id, None, selected, animate);
    response.widget_info(|| {
        egui::WidgetInfo::selected(egui::WidgetType::Button, ui.is_enabled(), selected, label)
    });
    if ui.is_rect_visible(rect) {
        ui.painter().with_clip_rect(rect).text(
            rect.center(),
            egui::Align2::CENTER_CENTER,
            label,
            egui::FontId::proportional(13.0),
            ui.visuals().text_color(),
        );
    }
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
    fn cell_order_controls(&mut self, ui: &mut egui::Ui) {
        let previous = (self.config.cell_shape, self.config.pixel_diagnostics);
        ComboBox::from_id_salt("cell-shape")
            .selected_text(self.config.cell_shape.label())
            .show_ui(ui, |ui| {
                for shape in [
                    super::cell_pixels::Shape::Rounded,
                    super::cell_pixels::Shape::Circle,
                    super::cell_pixels::Shape::Hexagon,
                ] {
                    ui.dropdown_value(&mut self.config.cell_shape, shape, shape.label());
                }
            });
        ui.add_enabled(self.config.cell_shape != super::cell_pixels::Shape::Rounded, egui::Checkbox::new(&mut self.config.pixel_diagnostics, "Pixel indices"))
            .on_hover_text("Pixel indices start at 0, counter-clockwise from the top. RGB values are before brightness adjustment.");
        if previous != (self.config.cell_shape, self.config.pixel_diagnostics) {
            crate::config::set_live(&self.config);
            self.save_workspace();
        }
        if self.config.cell_shape != super::cell_pixels::Shape::Rounded {
            let sample = self
                .current_pixel_frame()
                .and(self.table.pixel_sample.as_ref());
            let color = sample.map_or(ui.visuals().weak_text_color(), |sample| {
                if sample.fresh() {
                    ui.visuals().text_color()
                } else {
                    ui.visuals().warn_fg_color
                }
            });
            let detail = sample.map_or_else(
                || "Awaiting pixel frame".into(),
                |sample| {
                    format!(
                        "Frame {} · received {:.1}s ago",
                        sample.frame.revision,
                        sample.at.elapsed().as_secs_f32()
                    )
                },
            );
            ui.label(egui::RichText::new("RGB preview").small().color(color))
                .on_hover_text(detail);
        }
        if ui
            .checkbox(&mut self.table.editing_order, "Edit order")
            .changed()
        {
            self.table.order_drag = None;
            self.table.marquee = None;
        }
        if ui.add_enabled(self.table.editing_order && self.config.cell_order != (1..=80).collect::<Vec<_>>(),
            egui::Button::new(format!("{} Reset order", icons::ARROW_COUNTER_CLOCKWISE)))
            .on_hover_text("Restore presentation order; cell IDs and physical wiring stay unchanged.").clicked() {
            self.config.cell_order = (1..=80).collect();
            self.table.order_drag = None;
            self.save_workspace();
        }
    }
    fn finish_cell_order_drag(&mut self, ui: &mut egui::Ui, cells: &[(u8, Rect)], manager: bool) {
        if !self.table.editing_order || ui.input(|i| !i.focused || i.key_pressed(Key::Escape)) {
            self.table.order_drag = None;
            return;
        }
        let Some((source, surface)) = self.table.order_drag else {
            return;
        };
        if surface != manager {
            return;
        }
        ui.ctx().set_cursor_icon(egui::CursorIcon::Grabbing);
        if let Some(pointer) = ui.ctx().pointer_interact_pos().map(|p| local_pointer(ui, p)) {
            // Adapt Pealayer hardware_channel_drop_target's geometry/release
            // checks: horizontal table order uses the left/right half, not Y.
            if let Some((target, rect)) = cells.iter().find(|(id, rect)| {
                ui.clip_rect().contains(pointer) && *id != source && rect.contains(pointer)
            }) {
                let before = pointer.x < rect.center().x;
                let x = if before {
                    rect.left() + 2.0
                } else {
                    rect.right() - 2.0
                };
                ui.painter().line_segment(
                    [
                        egui::pos2(x, rect.top() + 4.0),
                        egui::pos2(x, rect.bottom() - 4.0),
                    ],
                    Stroke::new(2.0, ui.visuals().selection.stroke.color),
                );
                if ui.input(|i| i.pointer.button_released(egui::PointerButton::Primary))
                    && (if manager {
                        reorder_cell
                    } else {
                        reorder_visual_cell
                    })(
                        &mut self.config.cell_order,
                        self.table.editing_order,
                        source,
                        *target,
                        before,
                    )
                {
                    self.save_workspace();
                    ui.ctx().request_repaint();
                }
            }
            ui.painter().text(
                pointer + egui::vec2(12.0, 12.0),
                egui::Align2::LEFT_TOP,
                format!("Move cell {source}"),
                egui::FontId::proportional(13.0),
                ui.visuals().text_color(),
            );
        }
        if ui.input(|i| !i.pointer.primary_down()) {
            self.table.order_drag = None;
        }
    }
    pub fn sync_cell_selection_search(&mut self) -> bool {
        super::cell_search::synchronize_selection(
            &self.table.selection.ids,
            self.table.selection.revision,
            &mut self.table.reflected_revision,
            &mut self.cell_search,
            &mut self.selected_cell,
        )
    }
    pub fn apply_cell_search_selection(&mut self) {
        if self
            .table
            .selection
            .replace_query(&self.cell_search)
            .is_ok()
        {
            self.table.reflected_revision = self.table.selection.revision;
            if let Some(first) = self.table.selection.ids.first() {
                self.selected_cell = *first;
            }
        }
    }
    fn cell_queue_peer(&self) -> Option<(crate::Transport, crate::Endpoint, String)> {
        self.command_route()
            .map(|route| (route, self.endpoint(), self.serial_port.clone()))
    }
    pub fn process_cell_queue(&mut self, ctx: &egui::Context) {
        if !self.has_table() || !self.has_feature("cell-batch") {
            self.table.stop_pending_edits();
            return;
        }
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
                if self.command_route() == Some(crate::Transport::Serial)
                    && self.serial_operations_pending > 0
                {
                    ctx.request_repaint_after(Duration::from_millis(50));
                    return;
                }
                if let Some((ids, state, effect)) = self.table.queue.pop_front() {
                    let batch = match crate::cell_batch::Request::new(&ids, state.name(), &effect) {
                        Ok(batch) => batch,
                        Err(error) => {
                            self.table.fail_pending_edits(error);
                            return;
                        }
                    };
                    match self.dispatch_cell_batch(batch) {
                        Ok(correlation) => self.table.expected_batch = Some(correlation),
                        Err(error) => {
                            self.table.fail_pending_edits(error);
                            return;
                        }
                    }
                    self.table.dispatched += ids.len();
                    self.table.next_send = Instant::now() + Duration::from_millis(50);
                    self.table.result = format!(
                        "Sent one atomic batch ({} of {} cells); waiting for ACK and device readback",
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
                    self.serial_operations_pending == 0
                        && self.last_serial_status_at.is_some_and(|at| at > start)
                }
                _ => false,
            };
            if self.table.acknowledged
                && fresh
                && self.live_status().is_some_and(|s| {
                    self.table.awaiting.iter().all(|(id, state)| {
                        s.cells
                            .get(*id as usize - 1)
                            .is_some_and(|v| crate::cell_assignments::same_state(v, state.name()))
                    })
                })
            {
                self.table.result =
                    format!("Verified {} cell states on the device", self.table.queued);
                self.table.verification_started = None;
                self.table.awaiting.clear();
                self.table.expected_batch = None;
            } else if start.elapsed() > Duration::from_secs(24) {
                self.table.result="ACK/readback did not confirm the atomic batch. Inspect before retrying; nothing replayed.".into();
                self.table.verification_started = None;
                self.table.awaiting.clear();
                self.table.expected_batch = None;
            } else {
                ctx.request_repaint_after(Duration::from_millis(100));
            }
        }
    }
    fn queue_selected_cells(&mut self, state: State) {
        if self.cell_action_unavailable(state).is_some() {
            return;
        }
        let ids: Vec<_> = self.table.selection.ids.iter().copied().collect();
        if ids.is_empty() {
            return;
        }
        self.table.queued = ids.len();
        self.table.peer = self.cell_queue_peer();
        self.table.dispatched = 0;
        self.table.acknowledged = false;
        self.table.awaiting = ids.iter().map(|id| (*id, state)).collect();
        self.table.queue = VecDeque::from([(ids, state, self.cell_effect.clone())]);
        self.table.next_send = Instant::now();
        self.table.result =
            "One atomic selection batch queued; transitions run on the board".into();
    }
    fn cell_action_unavailable(&self, state: State) -> Option<&'static str> {
        action_unavailable(
            self.command_route().is_some(), self.has_feature("cell-batch"),
            self.table.selection.ids.len(), self.table.has_pending_edits(),
            state == State::Won && !self.has_feature("state-presentation"),
            state == State::White && self.live_status().is_some_and(|status| {
                status.red_lock == Some(true) && self.table.selection.ids.iter().any(|id| {
                    status.cells.get(*id as usize - 1)
                        .is_some_and(|state| crate::cell_assignments::same_state(state, "lost"))
                })
            }),
        )
    }
    pub fn cell_selection_toolbar(&mut self, ui: &mut egui::Ui) {
        if self
            .presentation_context()
            .or_else(|| self.peer_context())
            .is_some_and(|c| !c.table_selection())
        {
            return;
        }
        self.cell_assignment_toolbar(ui);
        ui.horizontal_wrapped(|ui| {
            ui.spacing_mut().item_spacing = egui::vec2(8.0, 6.0);
            ui.spacing_mut().interact_size.y = super::control_density::height(ui).max(30.0);
            ui.add_sized([94.0, ui.spacing().interact_size.y],
                egui::Label::new(format!("{} selected", self.table.selection.ids.len())).halign(egui::Align::Min));
            if ui.button(format!("{} All", icons::CHECK)).clicked() {
                self.table.selection.group(1..=80, false);
            }
            if ui.add_enabled(!self.table.selection.ids.is_empty(), egui::Button::new(format!("{} Clear", icons::X))).clicked() {
                self.table.selection.clear();
            }
            if ui.button(format!("{} Invert", icons::SELECTION_ALL)).clicked() {
                self.invert_cell_selection();
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
        ui.horizontal_wrapped(|ui| self.cell_order_controls(ui));
    }
    fn cell_actions_menu(&mut self, ui: &mut egui::Ui) {
        compact_menu(ui);
        for state in [State::White, State::Red, State::Won, State::Off] {
            if state == State::Won && !self.has_feature("state-presentation") {continue;}
            let unavailable = self.cell_action_unavailable(state);
            if ui
                .add_enabled(
                    unavailable.is_none(),
                    egui::Button::new(format!(
                        "{}  {} · {} cells",
                        state.icon(),
                        state.label(),
                        self.table.selection.ids.len()
                    )),
                )
                .on_disabled_hover_text(unavailable.unwrap_or_default()).clicked()
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
        let cells=self.live_status().map(|s|s.cells.clone()).unwrap_or_default();
        ui.menu_button("Select by state",|ui|{
            for state in [State::White,State::Red,State::Won,State::Off]{
                if state==State::Won&&!self.has_feature("state-presentation"){continue;}
                if ui.add_enabled(cells.len()==80,egui::Button::new(state.label())).clicked(){
                    self.table.selection.group(cells.iter().enumerate().filter_map(|(i,v)|crate::cell_assignments::same_state(v,state.name()).then_some(i as u8+1)),false);
                    ui.close();
                }
            }
        });
    }
    fn invert_cell_selection(&mut self) {
        let inverse: Vec<_> = (1..=80).filter(|id| !self.table.selection.ids.contains(id)).collect();
        self.table.selection.group(inverse, false);
    }

    /// One shared contextual surface, following Pealayer's transport menu:
    /// identical actions and availability from a cell, background or keyboard.
    fn cell_context_menu(&mut self, ui: &mut egui::Ui, target: Option<(u8, usize)>) {
        compact_menu(ui);
        ui.strong(match target {
            Some((id, _)) => format!("Cell {id} · {} selected", self.table.selection.ids.len()),
            None => format!("Table · {} selected", self.table.selection.ids.len()),
        });
        ui.separator();
        self.cell_actions_menu(ui);
        ui.separator();
        ui.menu_button(format!("{}  Selection", icons::SELECTION_ALL), |ui| {
            if ui.button(egui::RichText::new(format!("{}  Select all", icons::CHECK)))
                .on_hover_text("Ctrl+A").clicked() {
                self.table.selection.group(1..=80, false); ui.close();
            }
            if ui.button(format!("{}  Invert selection", icons::SELECTION_ALL)).clicked() {
                self.invert_cell_selection(); ui.close();
            }
            if let Some((_, slot)) = target {
                let (row, column) = slot_position(slot as u8);
                for (caption, along_row) in [(format!("Select row {}", row + 1), true),
                    (format!("Select column {}", column + 1), false)] {
                    if ui.button(format!("{}  {caption}", icons::GRID_FOUR)).clicked() {
                        let ids = visual_line(&self.config.cell_order, slot, along_row);
                        self.table.selection.group(ids, self.config.additive_cell_selection); ui.close();
                    }
                }
            }
            if ui.add_enabled(!self.table.selection.ids.is_empty(),
                egui::Button::new(format!("{}  Clear selection", icons::X)).shortcut_text("Esc")).clicked() {
                self.table.selection.clear(); ui.close();
            }
            if ui.add_enabled(!self.table.selection.ids.is_empty(), egui::Button::new(format!("{}  Copy cell IDs", icons::COPY))).clicked() {
                ui.ctx().copy_text(super::cell_search::format_selection(&self.table.selection.ids)); ui.close();
            }
        });
        if ui.add(egui::Button::new(format!("{}  Manage cells…", icons::SLIDERS_HORIZONTAL)).shortcut_text("Enter")).clicked() {
            self.table.manager_open = true; ui.close();
        }
        let mapping_cell = target.map(|(id, _)| id)
            .or_else(|| (self.table.selection.ids.len() == 1).then(|| *self.table.selection.ids.first().unwrap()));
        let mapping_ready = self.command_route().is_some()
            && self.live_status().and_then(crate::peer_http::Pin::from_status)
                .is_some_and(|pin| self.mapping_peer.as_ref() == Some(&pin))
            && mapping_cell.is_some_and(|id| self.mappings.iter().any(|mapping| mapping.logical_id == id));
        if ui.add_enabled(mapping_ready, egui::Button::new(format!("{}  Physical mapping…", icons::PENCIL_SIMPLE)))
            .on_disabled_hover_text("Select one cell with a confirmed mapping from the connected board.").clicked() {
            self.select_mapping(mapping_cell.unwrap()); self.show_mapping = true; ui.close();
        }
        for (panel, icon, label) in [
            (super::layout::Panel::PixelDiagnostics, icons::TARGET, "Pixel diagnostics"),
            (super::layout::Panel::PixelCorrections, icons::SLIDERS_HORIZONTAL, "Pixel corrections"),
            (super::layout::Panel::EffectControls, icons::SPARKLE, "Output & effects"),
        ] {
            if panel.available(self.presentation_context()) && ui.button(format!("{icon}  {label}")).clicked() {
                self.reveal_panels.push(panel); ui.close();
            }
        }
        ui.separator();
        ui.menu_button(format!("{}  View", icons::EYE), |ui| {
            if ui.button(format!("{}  Fit table", icons::ARROWS_IN)).clicked() {
                self.table.viewport = None; ui.close();
            }
            let mut changed = ui.checkbox(&mut self.config.show_cell_indicators, "LED indicators").changed();
            changed |= ui.checkbox(&mut self.config.color_cell_backgrounds, "Color cell backgrounds").changed();
            changed |= ui.checkbox(&mut self.config.additive_cell_selection, "Toggle selection without Ctrl").changed();
            if changed { crate::config::set_live(&self.config); self.save_workspace(); }
            if ui.checkbox(&mut self.table.editing_order, "Edit presentation order").changed() {
                self.table.order_drag = None; self.table.marquee = None;
            }
        });
    }
    pub(super) fn current_pixel_frame(&self) -> Option<&super::cell_pixels::Frame> {
        // The last observed pixels remain present while this exact live peer,
        // mapping and semantic state own them. Their age is shown separately;
        // this presentation path never acknowledges a command.
        let sample = self.table.pixel_sample.as_ref()?;
        let peer = self
            .live_status()
            .and_then(crate::peer_http::Pin::from_status)?;
        if self.command_route().is_none()
            || self.live_status().is_some_and(|s| crate::pixel_groups::advertised(s)
                && crate::pixel_groups::for_status(s).is_none_or(|layout|
                    sample.frame.group_layout_revision != Some(layout.revision)))
            || self.mapping_peer.as_ref() != Some(&peer)
            || !crate::streaming::valid_mapping(&self.mappings)
            || self.mappings.iter().any(|mapping| {
                sample
                    .frame
                    .physical
                    .get(usize::from(mapping.logical_id - 1))
                    != Some(&mapping.physical_cell)
            })
        {
            return None;
        }
        Some(&sample.frame)
    }
    pub fn draw_cell_grid(
        &mut self,
        ui: &mut egui::Ui,
        area: Rect,
        origin: Pos2,
        pitch: f32,
        height: f32,
    ) {
        let snapshot = self.live_status().cloned();
        let cells: Vec<_> = self
            .config
            .cell_order
            .iter()
            .enumerate()
            .map(|(slot, id)| (*id, cell_rect(origin, pitch, height, slot as u8 + 1)))
            .collect();
        let background = ui.interact(
            area,
            ui.id().with("table-marquee-background"),
            egui::Sense::click_and_drag(),
        );
        let modifiers = ui.input(|i| i.modifiers);
        let mut begin_drag = background.drag_started();
        let mut focus_target = None;
        for (slot, (id, rect)) in cells.iter().enumerate() {
            let visual_column = slot_position(slot as u8).1 + 1;
            let widget_id = ui.id().with(("logical-cell", id));
            let selected = self.table.selection.ids.contains(id);
            let state = snapshot
                .as_ref()
                .and_then(|s| s.cells.get(*id as usize - 1))
                .map(String::as_str);
            let state_rgb = state_fill(state, snapshot.as_ref().map(|s| &s.state_presentation));
            let solid_fill = self.config.color_cell_backgrounds.then_some(state_rgb).flatten();
            let response = if self.config.cell_shape == super::cell_pixels::Shape::Rounded {
                cell_surface_with_fill(
                    ui,
                    *rect,
                    widget_id,
                    visual_column,
                    selected,
                    self.config.animate_cells,
                    solid_fill,
                )
            } else {
                ui.interact(*rect, widget_id, egui::Sense::click_and_drag())
            };
            response.widget_info(|| {
                egui::WidgetInfo::labeled(egui::WidgetType::Button, true, format!("Cell {id}"))
            });
            let focus = response.has_focus()
                && !self.table.manager_open
                && self.preferences.is_none()
                && !self.device_settings.open
                && !self.show_connection
                && !self.show_mapping;
            let v = ui.visuals();
            let label_ink = solid_fill.map_or(v.text_color(), contrasting_ink);
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
                label_ink,
            );
            if self.config.cell_shape != super::cell_pixels::Shape::Rounded {
                let frame = self.current_pixel_frame();
                let physical = (self.command_route().is_some()
                    && crate::streaming::valid_mapping(&self.mappings))
                .then(|| mapping.as_ref().map(|m| m.physical_cell))
                .flatten();
                let label = mapping
                    .as_ref()
                    .map(|m| m.name.as_str())
                    .filter(|s| !s.is_empty())
                    .map(str::to_owned)
                    .unwrap_or_else(|| id.to_string());
                if frame.is_none() {
                    draw_semantic_cell(
                        ui,
                        &response,
                        self.config.cell_shape,
                        state,
                        &label,
                        selected,
                        visual_column,
                        self.config.animate_cells,
                        solid_fill,
                        semantic_caption(state, state_rgb),
                    );
                } else {
                    super::cell_pixels::draw(
                        ui,
                        &response,
                        self.config.cell_shape,
                        selected,
                        self.config.pixel_diagnostics,
                        frame,
                        physical,
                        *id,
                        &label,
                        cell_base_fill(self.config.color_palette, v.dark_mode, visual_column),
                        self.config.animate_cells,
                    );
                }
            } else {
                ui.painter().with_clip_rect(rect.shrink(9.0)).galley(
                    rect.center() - text.size() * 0.5,
                    text,
                    label_ink,
                );
            }
            if self.config.show_cell_indicators
                && self.config.cell_shape == super::cell_pixels::Shape::Rounded
            {
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
                    if self.config.cell_shape == super::cell_pixels::Shape::Rounded {
                        solid_fill.map_or(v.weak_text_color(), contrasting_ink)
                    } else { v.weak_text_color() },
                );
            }
            if !self.table.editing_order && pointer_or_accessibility_click(&response) {
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
            if !self.table.editing_order && response.double_clicked() {
                if !self.table.selection.ids.contains(id) {
                    self.table.selection.click(*id, false, false);
                }
                self.table.manager_open = true;
            }
            if self.table.editing_order {
                if response.drag_started_by(egui::PointerButton::Primary) {
                    self.table.order_drag = Some((*id, false));
                }
                if response.hovered() {
                    ui.ctx().set_cursor_icon(egui::CursorIcon::Grab);
                }
            } else {
                begin_drag |= response.drag_started();
            }
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
                        let next =
                            navigate_order(&self.config.cell_order, *id, key, modifiers.command);
                        self.selected_cell = next;
                        if !self.table.editing_order && (!modifiers.command || modifiers.shift) {
                            self.table.selection.click(next, false, modifiers.shift);
                        }
                        focus_target = Some(ui.id().with(("logical-cell", next)));
                    }
                }
                if ui.input_mut(|i| i.consume_key(Modifiers::COMMAND, Key::A)) {
                    self.table.selection.group(1..=80, false);
                }
                if ui.input_mut(|i| i.consume_key(Modifiers::NONE, Key::Escape)) {
                    self.table.selection.clear();
                }
                if !self.table.editing_order && activation_key(ui, modifiers, Key::Space) {
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
            popup.show(|ui| self.cell_context_menu(ui, Some((*id, slot))));
            let gesture = if self.table.editing_order {
                "Drag left/right of a cell to reorder · Escape cancels"
            } else {
                "Click selects · Ctrl toggles · Shift extends · drag selects · Enter manages"
            };
            let pixel_details = self
                .current_pixel_frame()
                .and_then(|frame| {
                    frame.cell(*id).map(|(first, colors)| {
                        format!(
                            "\nFrame {} · pre-brightness RGB · brightness {}\n{}",
                            frame.revision,
                            frame.brightness,
                            colors
                                .iter()
                                .enumerate()
                                .map(|(s, c)| format!(
                                    "Pixel {}: RGB({}, {}, {}) #{:02X}{:02X}{:02X}",
                                    first + s as u16,
                                    c.r(),
                                    c.g(),
                                    c.b(),
                                    c.r(),
                                    c.g(),
                                    c.b()
                                ))
                                .collect::<Vec<_>>()
                                .join("\n")
                        )
                    })
                })
                .unwrap_or_else(|| {
                    "\nPixel RGB unavailable: awaiting a fresh published frame from this board."
                        .into()
                });
            response.on_hover_text(format!(
                "Cell {id} · {} · {}\n{gesture}{pixel_details}",
                mapping
                    .map(|m| format!("physical {}", m.physical_cell))
                    .unwrap_or_else(|| self.mapping_status()),
                state.unwrap_or("state unavailable")
            ));
        }
        if let Some(target) = focus_target {
            ui.memory_mut(|m| m.request_focus(target));
        }
        self.finish_cell_order_drag(ui, &cells, false);
        if !self.table.editing_order
            && begin_drag
            && let Some(start) = ui.input(|i| i.pointer.press_origin()).map(|p| local_pointer(ui, p))
        {
            let base = if modifiers.command || modifiers.shift {
                self.table.selection.ids.clone()
            } else {
                BTreeSet::new()
            };
            self.table.marquee = Some((start, base));
        }
        if let Some((start, base)) = self.table.marquee.clone() {
            if let Some(end) = ui.input(|i| i.pointer.interact_pos()).map(|p| local_pointer(ui, p)) {
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
            self.table.selection.clear();
        }
        background.context_menu(|ui| self.cell_context_menu(ui, None));
    }
    pub fn draw_cell_manager(&mut self, ctx: &egui::Context) {
        self.finish_assignment_export();
        self.draw_assignment_import(ctx);
        if !self.has_table()
            || self.peer_context().is_some_and(|c| !c.table_selection())
            || !self.table.manager_open
        {
            return;
        }
        let mut close = false;
        let modal=egui::Modal::new(egui::Id::new("cell-management")).show(ctx,|ui| {
            ui.set_width((ctx.content_rect().width()-64.0).clamp(280.0,650.0));
            ui.heading(format!("{}  Manage cells",icons::GRID_FOUR));
            ui.label("Select table rows or individual cells, then apply one state and transition to the selection.");
            ui.weak(self.mapping_status());
            ui.add_space(8.0);
            ui.horizontal_wrapped(|ui| {
                ui.strong(format!("{} selected",self.table.selection.ids.len()));
                if ui.button("Select all").clicked(){self.table.selection.group(1..=80,false);}
                if ui.button("Clear").clicked(){self.table.selection.clear();}
                if ui.checkbox(&mut self.config.additive_cell_selection,"Toggle without Ctrl").changed(){self.save_workspace();}
            });
            let snapshot=self.live_status().cloned();
            ui.horizontal_wrapped(|ui| self.cell_order_controls(ui));
            let order=self.config.cell_order.clone();
            let mut manager_cells=Vec::new();
            dialog::scroll_column(ui,"cell-management-rows",Some((ctx.content_rect().height()-340.0).clamp(120.0,360.0)),|ui| {
                for row in 0..8u8 {
                    let row_ids=&order[row as usize*10..row as usize*10+10];
                    ui.horizontal_top(|ui| {
                        let all=row_ids.iter().all(|id|self.table.selection.ids.contains(id));
                        let row_response=manager_selector(ui,&format!("{} Row {}",icons::LIST,row+1),all,row*10+1,96.0,self.config.animate_cells);
                        if row_response.clicked(){let m=ui.input(|i|i.modifiers);self.table.selection.group(row_ids.iter().copied(),m.command||self.config.additive_cell_selection);}
                        ui.horizontal_wrapped(|ui|{for (column,id) in row_ids.iter().copied().enumerate() {let response=manager_selector(ui,&format!("{id:02}"),self.table.selection.ids.contains(&id),column as u8+1,32.0,self.config.animate_cells);manager_cells.push((id,response.rect));if self.table.editing_order && response.drag_started_by(egui::PointerButton::Primary){self.table.order_drag=Some((id,true));}if self.table.editing_order && response.hovered(){ui.ctx().set_cursor_icon(egui::CursorIcon::Grab);}if !self.table.editing_order && response.clicked(){let m=ui.input(|i|i.modifiers);self.table.selection.click(id,m.command||self.config.additive_cell_selection,m.shift);self.selected_cell=id;}response.on_hover_text(self.mappings.iter().find(|m|m.logical_id==id).map(|m|format!("{} · physical {} · {}",m.name,m.physical_cell,snapshot.as_ref().and_then(|s|s.cells.get(id as usize-1)).map(String::as_str).unwrap_or("unavailable"))).unwrap_or_else(||format!("Cell {id} · mapping unavailable")));}});
                    });
                }
                self.finish_cell_order_drag(ui,&manager_cells,true);
            });
            dialog::section(ui,icons::LIGHTBULB,"Selected cell action",|ui| {
                ui.horizontal_wrapped(|ui|{for state in [State::White,State::Red,State::Won,State::Off]{if state==State::Won&&!self.has_feature("state-presentation"){continue;}if manager_selector(ui,&format!("{} {}",state.icon(),state.name()),self.table.manager_state==state,1,104.0,self.config.animate_cells).clicked(){self.table.manager_state=state;}}});
                ui.horizontal(|ui|{ui.label("Transition");egui::ComboBox::from_id_salt("bulk-effect").selected_text(&self.cell_effect).show_ui(ui,|ui|{for effect in ["immediate","fade","pulse"]{ui.selectable_value(&mut self.cell_effect,effect.into(),effect);}});});
            ui.label(egui::RichText::new("One atomic board-owned batch; all selected cell states require ACK and readback.").small().weak());
            if !self.has_feature("cell-batch"){ui.weak("This board has not acknowledged atomic cell-batch support; selection edits are unavailable.");}
            });
            if !self.table.result.is_empty(){ui.label(&self.table.result);}
            ui.separator();
            ui.horizontal_wrapped(|ui| {
                let unavailable = self.cell_action_unavailable(self.table.manager_state);
                if ui.add_enabled(unavailable.is_none(),egui::Button::new(format!("{} Apply to {} cells",icons::CHECK,self.table.selection.ids.len())))
                    .on_disabled_hover_text(unavailable.unwrap_or_default()).clicked(){self.queue_selected_cells(self.table.manager_state);}
                if ui.add_enabled(!self.table.queue.is_empty(),egui::Button::new(format!("{} Cancel unsent",icons::X))).clicked(){self.table.queue.clear();self.table.awaiting.clear();self.table.verification_started=None;self.table.result="Cancelled unsent edits. Previously sent edits may have applied.".into();}
                if ui.add_enabled(self.table.selection.ids.len()==1&&!self.mappings.is_empty(),egui::Button::new(format!("{} Mapping…",icons::PENCIL_SIMPLE))).clicked(){let id=*self.table.selection.ids.first().unwrap();self.select_mapping(id);self.show_mapping=true;close=true;}
                if dialog::action_button(ui,icons::X,"Close").clicked(){close=true;}
            });
        });
        if close || modal.should_close() {
            self.table.manager_open = false;
            self.table.order_drag = None;
        }
    }
}

impl PakarControllerApp {
    fn cell_assignment_toolbar(&mut self, ui: &mut egui::Ui) {
        let Some(context) = self
            .presentation_context()
            .or_else(|| self.peer_context())
            .cloned()
        else {
            return;
        };
        if !context.table_selection() || !context.has("cell-assignments") {
            return;
        }
        let status = self.live_status().cloned();
        let live = status
            .as_ref()
            .is_some_and(crate::cell_assignments::supported)
            && self.command_route().is_some();
        ui.add_enabled_ui(live,|ui|{
        ui.horizontal_wrapped(|ui|{
            if context.has("cell-lock") {
                let observed=status.as_ref().and_then(|s|s.red_lock);
                let mut enabled=observed.unwrap_or(false);
                    if ui.add_enabled(observed.is_some(),egui::Checkbox::new(&mut enabled,super::i18n::text("Lock lost cells")).indeterminate(observed.is_none()))
                        .on_hover_text(super::i18n::text("Board-enforced; Off remains available. The observed board value is shown only after ACK/readback.")).changed(){
                        self.post("/api/cells/lock",vec![("enabled".into(),enabled.to_string())]);
                    }
            }
            if ui.button(format!("{} {}",icons::FOLDER_OPEN,super::i18n::text("Import colors…"))).clicked(){
                // Pealayer's native file chooser and detached draft ownership:
                // parse into a reviewable draft; never publish during selection.
                if let Some(path)=rfd::FileDialog::new().set_title(super::i18n::text("Import cell colors")).add_filter(super::i18n::text("Cell assignments"),&["xlsx","csv","json"]).pick_file(){
                    match crate::cell_assignments::load(&path){
                        Ok(doc)=>if let Some(pin)=status.as_ref().and_then(crate::peer_http::Pin::from_status){self.table.assignment_import=Some((doc,pin));},
                        Err(error)=>self.table.result=error,
                    }
                }
            }
            ui.menu_button(format!("{} {}",icons::FLOPPY_DISK,super::i18n::text("Export colors")),|ui|{
                for (label,extension) in [("Excel workbook (.xlsx)","xlsx"),("CSV (.csv)","csv"),("JSON (.json)","json")] {
                    if ui.button(super::i18n::text(label)).clicked(){
                        if let Some(path)=rfd::FileDialog::new().set_title(super::i18n::text("Export cell colors")).set_file_name(format!("cell-colors.{extension}")).add_filter(super::i18n::text(label),&[extension]).save_file(){
                            if let Some(pin)=status.as_ref().and_then(crate::peer_http::Pin::from_status){
                                self.table.assignment_export=Some((path,extension.into(),pin,Instant::now()));
                                self.table.result=super::i18n::text("Waiting for a fresh board observation before export…");
                            }
                        }
                        ui.close();
                    }
                }
            });
        });
        });
    }
    fn finish_assignment_export(&mut self) {
        let Some((path, extension, pin, at)) = self.table.assignment_export.clone() else {
            return;
        };
        if at.elapsed() > Duration::from_secs(10) {
            self.table.assignment_export = None;
            self.table.result =
                super::i18n::text("Export cancelled: fresh board state was not available");
            return;
        }
        let Some(status) = self.live_status() else {
            return;
        };
        let result = if crate::peer_http::Pin::from_status(status).as_ref() != Some(&pin) {
            Err("Selected board changed; export cancelled".into())
        } else {
            crate::cell_assignments::Document::from_status(status)
                .and_then(|doc| crate::cell_assignments::encode(&extension, &doc))
                .and_then(|bytes| std::fs::write(path, bytes).map_err(|e| e.to_string()))
        };
        self.table.assignment_export = None;
        self.table.result = match result {
            Ok(()) => super::i18n::text("Exported all 80 observed assignments"),
            Err(e) => format!("{}: {e}", super::i18n::text("Export failed")),
        };
    }
    fn draw_assignment_import(&mut self, ctx: &egui::Context) {
        let Some((doc, pin)) = self.table.assignment_import.clone() else {
            return;
        };
        let status = self.live_status();
        let current = status.is_some_and(|s| {
            crate::peer_http::Pin::from_status(s).as_ref() == Some(&pin)
                && crate::cell_assignments::supported(s)
        });
        let locked = status.is_some_and(|s| {
            s.red_lock == Some(true)
                && doc.assignments.iter().any(|a| {
                    crate::cell_assignments::same_state(&a.state, "idle")
                        && s.cells.get(a.cell as usize - 1).is_some_and(|v| crate::cell_assignments::same_state(v, "lost"))
                })
        });
        let mut apply = false;
        let mut close = false;
        let modal=egui::Modal::new(egui::Id::new("cell-assignment-import")).show(ctx,|ui|{
            let geometry=dialog::bounded_geometry(ctx.content_rect(),24.0,egui::vec2(560.0,480.0),egui::vec2(1.0,1.0),egui::vec2(560.0,600.0));
            ui.set_width((geometry.max_size.x-32.0).max(1.0));
            ui.heading(format!("{} {}",icons::GRID_FOUR,super::i18n::text("Import cell colors")));
            ui.weak(format!("{} · {}",doc.assignments.len(),super::i18n::text("assignments; omitted cells stay unchanged")));
            dialog::section(ui,icons::LIGHTBULB,"Review changes",|ui|{
                dialog::scroll_column(ui,"assignment-preview",Some((geometry.max_size.y-190.0).clamp(1.0,260.0)),|ui|{
                    egui::Grid::new("assignment-preview-rows").striped(true).spacing([24.0,6.0]).show(ui,|ui|{
                        ui.strong(super::i18n::text("Cell"));ui.strong(super::i18n::text("Current"));ui.strong(super::i18n::text("Imported"));ui.end_row();
                        for row in &doc.assignments {ui.label(format!("{:02}",row.cell));ui.weak(super::i18n::text(status.and_then(|s|s.cells.get(row.cell as usize-1)).map(String::as_str).unwrap_or("Unknown")));ui.label(super::i18n::text(&row.state));ui.end_row();}
                    });
                });
            });
            if !current {ui.weak(super::i18n::text("The selected board changed or is offline. Reopen this file after reconnecting."));}
            if locked {ui.colored_label(ui.visuals().warn_fg_color,super::i18n::text("Lost lock blocks this import. No assignments will be applied."));}
            ui.separator();
            dialog::action_row(ui,super::i18n::rtl(),|ui|{
                apply=ui.add_enabled(current&&!locked,egui::Button::new(format!("{} {}",icons::CHECK,super::i18n::text("Apply atomically")))).clicked();
                close=dialog::action_button(ui,icons::X,"Cancel").clicked();
            });
        });
        if apply {
            self.post("/api/cells/assignments", doc.form());
            self.table.assignment_import = None;
        } else if close || modal.should_close() {
            self.table.assignment_import = None;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn table_canvas_contains_every_staggered_cell_and_focus_padding() {
        for rounded in [false, true] {
            for (width, height) in [(280.0, 160.0), (1280.0, 400.0), (1900.0, 600.0)] {
                let (pitch, row_height) = grid_metrics(width, height, rounded);
                let (canvas, origin) = grid_canvas(pitch, row_height);
                for id in 1..=80 {
                    assert!(canvas.contains_rect(cell_rect(origin, pitch, row_height, id).expand(2.0)),
                        "cell {id} focus outline escapes {canvas:?}");
                }
                let first = cell_rect(origin, pitch, row_height, 1);
                let staggered = cell_rect(origin, pitch, row_height, 11);
                assert!((staggered.top() - first.top() - row_height * 0.5).abs() < 0.001);
                assert_eq!(canvas.min, Pos2::ZERO);
            }
        }
    }

    #[test]
    fn visual_context_selection_matches_rows_columns_and_reordered_ids() {
        let mut order: Vec<u8> = (1..=80).collect();
        assert_eq!(visual_line(&order, 0, true), (1..=20).collect::<Vec<_>>());
        assert_eq!(visual_line(&order, 0, false), [1, 21, 41, 61]);
        assert_eq!(visual_line(&order, 10, false), [11, 31, 51, 71]);
        order.swap(0, 79);
        assert_eq!(visual_line(&order, 0, false), [80, 21, 41, 61]);
        assert!(visual_line(&order, 0, true).contains(&80));
        assert!(!visual_line(&order, 0, true).contains(&1));
    }

    #[test]
    fn table_actions_are_gated_by_route_capabilities_selection_pending_and_lock() {
        assert!(action_unavailable(true, true, 2, false, false, false).is_none());
        for args in [(false, true, 2, false, false, false),
            (true, false, 2, false, false, false), (true, true, 0, false, false, false),
            (true, true, 2, true, false, false), (true, true, 2, false, true, false),
            (true, true, 2, false, false, true)] {
            assert!(action_unavailable(args.0, args.1, args.2, args.3, args.4, args.5).is_some());
        }
        assert!(action_unavailable(true, true, 2, false, false, true).unwrap().contains("locked"));
    }

    #[test]
    fn find_icon_is_centered_inside_stable_editor_slot() {
        for dark in [true, false] {
            let context = egui::Context::default();
            context.set_visuals(if dark { egui::Visuals::dark() } else { egui::Visuals::light() });
            let mut text = String::new();
            for width in [90.0, 270.0, 420.0] {
                let mut field = Rect::NOTHING;
                let output = context.run_ui(egui::RawInput::default(), |ui| {
                    field = find_field(ui, &mut text, width).rect;
                });
                let icon = output.shapes.iter().find_map(|shape| match &shape.shape {
                    egui::Shape::Text(text) if text.galley.job.text == icons::MAGNIFYING_GLASS => Some(text),
                    _ => None,
                }).expect("Find icon is painted inside its field");
                let center = icon.pos + icon.galley.size() * 0.5;
                assert!((center.y - field.center().y).abs() <= 0.5);
                assert!(field.contains(center));
                assert!(field.height() >= 30.0);
            }
        }
    }

    #[test]
    fn find_row_fills_responsive_panel_without_a_fixed_width_cap() {
        for width in [280.0, 900.0, 1920.0] {
            let context = egui::Context::default();
            let mut query = String::new();
            let _ = context.run_ui(egui::RawInput {
                screen_rect: Some(Rect::from_min_size(Pos2::ZERO, egui::vec2(width, 240.0))),
                ..Default::default()
            }, |ui| {
                ui.horizontal(|ui| {
                    ui.spacing_mut().item_spacing.x = 6.0;
                    let height = super::super::control_density::height(ui).max(30.0);
                    let row = ui.available_rect_before_wrap();
                    let available = ui.available_width();
                    let expected = find_row_width(available, height, 6.0);
                    let field = find_field(ui, &mut query, expected).rect;
                    let mut end = field.right();
                    for glyph in [icons::CARET_LEFT, icons::CARET_RIGHT, icons::X] {
                        end = ui.add(egui::Button::new(glyph).min_size(egui::vec2(height, height))).rect.right();
                    }
                    assert!((field.width() - expected).abs() <= 1.0);
                    assert!((end - row.right()).abs() <= 1.0, "Find row leaves space at width {width}: {end} != {}", row.right());
                    if width >= 900.0 { assert!(field.width() > 700.0); }
                });
            });
        }
    }

    #[test]
    fn table_helpers_reserve_height_and_warn_only_for_actual_cautions() {
        for width in [180.0, 700.0] {
            let context = egui::Context::default();
            let mut previous = None;
            for (text, tone) in [("", HelperTone::Normal), ("Selection 2 of 4 · logical cell 3", HelperTone::Normal),
                ("Use cell IDs 1–80, separated by commas, or ranges such as 20–28", HelperTone::Error),
                ("Mapping failed; retry is available", HelperTone::Caution)] {
                let mut rect = Rect::NOTHING;
                let output = context.run_ui(egui::RawInput::default(), |ui| {
                    ui.set_width(width);
                    rect = table_helper(ui, text, tone).rect;
                });
                if let Some(previous) = previous { assert_eq!(rect, previous); }
                previous = Some(rect);
                let captions: Vec<_> = output.shapes.iter().filter_map(|shape| match &shape.shape {
                    egui::Shape::Text(text) => {
                        assert!((text.pos.x - rect.left()).abs() <= 0.5, "Helper must start at the lane's leading edge");
                        assert!((text.pos.y + text.galley.size().y * 0.5 - rect.center().y).abs() <= 0.5);
                        Some(text.galley.job.text.as_str())
                    }, _ => None,
                }).collect();
                assert_eq!(captions.iter().any(|text| text.contains(icons::WARNING)), tone != HelperTone::Normal);
            }
        }
    }
    #[test]
    fn atomic_readback_confirmation_cannot_be_inferred_from_pending_selection() {
        let status = crate::DeviceStatus {
            board_id: "esp32s3-fixture".into(),
            boot_id: "a".repeat(32),
            http_peer_fencing: true,
            ..Default::default()
        };
        let peer = crate::peer_http::Pin::from_status(&status).unwrap();
        let request = crate::cell_batch::Request::new(&[1, 80], "red", "fade").unwrap();
        let own = crate::cell_batch::Correlation::new(
            request.clone(),
            peer.clone(),
            crate::Transport::WiFi,
        )
        .unwrap();
        let foreign = crate::cell_batch::Correlation::new(
            crate::cell_batch::Request::new(&[2, 79], "white", "fade").unwrap(),
            peer.clone(),
            crate::Transport::WiFi,
        )
        .unwrap();
        let mut table = TableState::default();
        table.awaiting = vec![(1, State::Red), (80, State::Red)];
        table.expected_batch = Some(own.clone());
        table.confirm_batch_ack(&own);
        assert!(!table.acknowledged);
        table.verification_started = Some(Instant::now());
        assert_eq!(foreign.request.ids.len(), own.request.ids.len());
        table.confirm_batch_ack(&foreign);
        assert!(
            !table.acknowledged,
            "An IPC batch of equal size must not ACK the manager's batch"
        );
        let identical_foreign =
            crate::cell_batch::Correlation::new(request, peer, crate::Transport::WiFi).unwrap();
        table.confirm_batch_ack(&identical_foreign);
        assert!(
            !table.acknowledged,
            "Even identical values require the exact operation ID"
        );
        let mut altered = own.clone();
        altered.route = crate::Transport::Serial;
        table.confirm_batch_ack(&altered);
        assert!(!table.acknowledged);
        altered = own.clone();
        altered.request.state = "white".into();
        table.confirm_batch_ack(&altered);
        assert!(!table.acknowledged);
        table.confirm_batch_ack(&own);
        assert!(table.acknowledged);
        table.stop_pending_edits();
        assert!(!table.acknowledged);
        table.confirm_batch_ack(&own);
        assert!(!table.acknowledged);
    }
    #[test]
    fn canonical_cell_geometry_is_independent_of_viewport_transform() {
        for width in [280.0, 640.0, 1350.0] {
            for visible in [120.0, 230.0, 700.0] {
                let (pitch, height) = grid_metrics(width, visible, false);
                assert!(pitch >= 64.0 && pitch <= 94.0);
                assert_eq!(height, pitch);
                let slot = cell_rect(Pos2::ZERO, pitch, height, 80);
                assert!(slot.width() >= 57.0 && slot.height() >= 57.0);
                assert!(slot.width().min(slot.height()) - 6.0 >= 51.0);
            }
        }
    }
    #[test]
    fn semantic_fallback_distinguishes_states_without_fabricating_rgb() {
        let v = egui::Visuals::dark();
        let base = Color32::from_rgb(17, 22, 30);
        let white = semantic_colors(base, Some("white"), &v);
        let red = semantic_colors(base, Some("red"), &v);
        let off = semantic_colors(base, Some("off"), &v);
        assert_eq!((white.2, red.2, off.2), ("WHITE", "RED", "OFF"));
        assert_ne!(white.0, red.0);
        assert_ne!(red.0, off.0);
        assert_ne!(white.0, Color32::WHITE);
        assert_eq!(semantic_colors(base, None, &v).2, "UNKNOWN");
    }
    #[test]
    fn semantic_background_uses_board_palette_and_never_colors_unknown_state() {
        let presentation = serde_json::json!({"states":{
            "idle":{"rgb":0x123456}, "lost":{"rgb":0x00ffaa},
            "won":{"rgb":0xff33cc}, "off":{"rgb":0x001122}
        }});
        for (state, rgb) in [
            ("white", 0x123456), ("idle", 0x123456), ("red", 0x00ffaa),
            ("lost", 0x00ffaa), ("won", 0xff33cc), ("off", 0x001122),
        ] {
            assert_eq!(state_fill(Some(state), Some(&presentation)),
                Some(Color32::from_rgb((rgb >> 16) as u8, (rgb >> 8) as u8, rgb as u8)));
        }
        assert_eq!(state_fill(None, Some(&presentation)), None);
        assert_eq!(state_fill(Some("unknown"), Some(&presentation)), None);
        for unknown in ["#ffffff", "#ff0000", "#000000", "12ab34"] {
            assert_eq!(state_fill(Some(unknown), Some(&presentation)), None);
            assert_eq!(semantic_caption(Some(unknown), Some(Color32::RED)), "UNKNOWN");
        }
        assert_eq!(state_fill(Some("white"), None), Some(Color32::WHITE));
        assert_eq!(state_fill(Some("red"), None), Some(Color32::RED));
        assert_eq!(state_fill(Some("won"), None), Some(Color32::GREEN));
        assert_eq!(state_fill(Some("off"), None), Some(Color32::BLACK));
        for invalid in [serde_json::json!(0x1000000_u64), serde_json::json!(-1), serde_json::json!("red")] {
            assert_eq!(state_fill(Some("red"), Some(&serde_json::json!({"states":{"lost":{"rgb":invalid}}}))),
                Some(Color32::RED));
        }
    }
    #[test]
    fn semantic_caption_names_palette_color_without_mislabeling_custom_states() {
        assert_eq!(semantic_caption(Some("lost"), Some(Color32::BLUE)), "BLUE");
        assert_eq!(semantic_caption(Some("white"), Some(Color32::RED)), "RED");
        assert_eq!(semantic_caption(Some("won"), Some(Color32::GREEN)), "GREEN");
        assert_eq!(semantic_caption(Some("off"), Some(Color32::BLACK)), "OFF");
        let custom = Some(Color32::from_rgb(0x12, 0x34, 0x56));
        assert_eq!(semantic_caption(Some("lost"), custom), "LOST");
        assert_eq!(semantic_caption(Some("idle"), custom), "IDLE");
        assert_eq!(semantic_caption(Some("won"), custom), "WON");
        assert_eq!(semantic_caption(None, custom), "UNKNOWN");
    }
    #[test]
    fn hover_strengthens_existing_cell_edge_without_replacing_selection_identity() {
        let contrast = |a: Color32, b: Color32| {
            let (a, b) = (luminance(a), luminance(b));
            (a.max(b) + 0.05) / (a.min(b) + 0.05)
        };
        for palette in [super::super::palette::Palette::Studio, super::super::palette::Palette::Native] {
            for dark in [false, true] {
                let mut v = if dark { egui::Visuals::dark() } else { egui::Visuals::light() };
                super::super::palette::apply(&mut v, palette);
                let base = cell_base_fill(palette, dark, 1);
                for state in [None, Some("white"), Some("red"), Some("won"), Some("off")] {
                    let (fill, color, _) = semantic_colors(base, state, &v);
                    let resting = blend(fill, color, 0.65);
                    let hover = cell_outline_color(&v, resting, fill, false, false, true);
                    assert!(contrast(hover, fill) >= contrast(resting, fill), "{palette:?}, dark={dark}, {state:?}");
                    if dark { assert!(luminance(hover) >= luminance(resting)); }
                    assert_eq!(cell_outline_color(&v, resting, fill, true, false, true), v.warn_fg_color);
                    assert_eq!(cell_outline_color(&v, resting, fill, true, true, true), v.warn_fg_color);
                    assert_eq!(cell_outline_color(&v, resting, fill, false, true, true), v.selection.stroke.color);
                }
            }
        }
        for r in [0, 64, 128, 192, 255] { for g in [0, 64, 128, 192, 255] { for b in [0, 64, 128, 192, 255] {
            let fill = Color32::from_rgb(r, g, b);
            assert!(contrast(contrasting_ink(fill), fill) >= 4.5);
        } } }
    }
    #[test]
    fn opted_in_semantic_shapes_use_exact_fill_without_moving_the_hit_slot() {
        let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(64.0, 64.0));
        for shape in [super::super::cell_pixels::Shape::Circle, super::super::cell_pixels::Shape::Hexagon] {
            for fill in [Color32::WHITE, Color32::RED, Color32::GREEN, Color32::BLACK] {
                let ctx = egui::Context::default();
                let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                    let response = ui.interact(rect, egui::Id::new("semantic-solid"), egui::Sense::click());
                    draw_semantic_cell(ui, &response, shape, Some("lost"), "15", true, 1, false, Some(fill), "LOST");
                    assert_eq!(response.rect, rect);
                });
                output.textures_delta.clear();
                assert!(output.shapes.iter().any(|shape| match &shape.shape {
                    egui::Shape::Circle(circle) => circle.fill == fill,
                    egui::Shape::Path(path) => path.fill == fill,
                    _ => false,
                }));
                let texts: Vec<_> = output.shapes.iter().filter_map(|shape| match &shape.shape {
                    egui::Shape::Text(text) => Some(text.fallback_color), _ => None,
                }).collect();
                assert_eq!(texts, vec![contrasting_ink(fill); 2]);
            }
        }
        let ctx = egui::Context::default();
        let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
            let response = cell_surface_with_fill(ui, rect, egui::Id::new("rounded-solid"), 1, true, false, Some(Color32::GREEN));
            assert_eq!(response.rect, rect);
        });
        output.textures_delta.clear();
        assert!(output.shapes.iter().any(|shape| matches!(&shape.shape,
            egui::Shape::Rect(paint) if paint.rect == rect && paint.fill == Color32::GREEN && paint.stroke_kind == egui::StrokeKind::Inside)));
    }
    #[test]
    fn context_revocation_drops_unsent_and_readback_work_without_claiming_rollback() {
        for verifying in [false, true] {
            let mut table = TableState::default();
            table.selection.click(3, false, false);
            table.dispatched = 1;
            table.queued = 2;
            table.awaiting = vec![(3, State::Red)];
            table.peer = Some((
                crate::Transport::WiFi,
                crate::Endpoint {
                    base: "http://fixture.invalid".into(),
                    user: String::new(),
                    password: String::new(),
                },
                String::new(),
            ));
            if verifying {
                table.verification_started = Some(Instant::now());
            } else {
                table.queue.push_back((vec![4], State::Red, "fade".into()));
            }
            table.stop_pending_edits();
            assert!(table.queue.is_empty());
            assert!(table.awaiting.is_empty());
            assert!(table.verification_started.is_none());
            assert!(table.peer.is_none());
            assert_eq!(table.dispatched, 1, "sent writes are not undone");
            assert_eq!(
                table.selection.cells(),
                [3],
                "revocation is not a selection mutation"
            );
            assert!(
                table
                    .result
                    .contains("Previously sent edits may have applied")
            );
            let result = table.result.clone();
            table.stop_pending_edits();
            assert_eq!(table.result, result, "idle frames retain the warning");
        }
        let shell = include_str!("shell.rs");
        assert!(!shell.contains("self.table.queue"));
        assert!(!shell.contains("self.table.awaiting"));
        assert!(!shell.contains("self.table.verification_started"));
    }
    #[test]
    fn reorder_is_mode_guarded_identity_preserving_and_matches_web_contract() {
        let contract: serde_json::Value =
            serde_json::from_str(include_str!("../../../assets/cells/order-contract.json"))
                .unwrap();
        let mut order: Vec<u8> = (1..=80).collect();
        for step in contract["steps"].as_array().unwrap() {
            let changed = reorder_cell(
                &mut order,
                step["editing"].as_bool().unwrap(),
                step["source"].as_u64().unwrap() as u8,
                step["target"].as_u64().unwrap() as u8,
                step["before"].as_bool().unwrap(),
            );
            let expected: Vec<u8> = step["expected"]
                .as_array()
                .unwrap()
                .iter()
                .flat_map(|part| {
                    if let Some(id) = part.as_u64() {
                        vec![id as u8]
                    } else {
                        (part[0].as_u64().unwrap() as u8..=part[1].as_u64().unwrap() as u8)
                            .collect()
                    }
                })
                .collect();
            assert_eq!(changed, step["changed"].as_bool().unwrap());
            assert_eq!(order, expected);
            assert_eq!(
                order.iter().copied().collect::<BTreeSet<_>>(),
                (1..=80).collect()
            );
        }
        assert_eq!(
            navigate_order(&order, order[0], Key::ArrowRight, false),
            order[10]
        );
        assert_eq!(navigate_order(&order, order[37], Key::Home, true), order[0]);
        assert_eq!(navigate_order(&order, order[0], Key::End, true), order[79]);
        let repaired = normalized_order(&[80, 80, 0, 81, 1]);
        assert_eq!(repaired.len(), 80);
        assert_eq!(&repaired[..3], &[80, 1, 2]);
        assert_eq!(normalized_order(&[]), (1..=80).collect::<Vec<_>>());
    }
    #[test]
    fn manager_hit_slots_text_and_wrap_never_follow_hover_or_stroke_width() {
        for width in [240.0, 650.0] {
            for animate in [false, true] {
                let ctx = egui::Context::default();
                let mut render = |events, selected| {
                    let mut slots = Vec::new();
                    let mut first_id = egui::Id::NULL;
                    let mut hovered = false;
                    let mut output = ctx.run_ui(
                        egui::RawInput {
                            screen_rect: Some(Rect::from_min_size(
                                Pos2::ZERO,
                                egui::vec2(width, 400.0),
                            )),
                            events,
                            ..Default::default()
                        },
                        |ui| {
                            ui.set_width(width);
                            ui.horizontal_wrapped(|ui| {
                                for id in 1..=12 {
                                    let response = manager_selector(
                                        ui,
                                        &format!("{id:02}"),
                                        selected,
                                        id,
                                        32.0,
                                        animate,
                                    );
                                    slots.push(response.rect);
                                    if id == 1 {
                                        first_id = response.id;
                                        hovered = response.hovered();
                                    }
                                }
                            });
                        },
                    );
                    output.textures_delta.clear();
                    let text_positions: Vec<_> = output
                        .shapes
                        .iter()
                        .filter_map(|s| match &s.shape {
                            egui::Shape::Text(t) => Some(t.pos),
                            egui::Shape::Rect(r) => {
                                assert_eq!(r.stroke_kind, egui::StrokeKind::Inside);
                                assert!(slots.iter().any(|slot| slot.contains_rect(r.rect)));
                                None
                            }
                            _ => None,
                        })
                        .collect();
                    assert_eq!(text_positions.len(), 12);
                    for slot in &slots {
                        assert_eq!(slot.size(), egui::vec2(32.0, dialog::ACTION_HEIGHT));
                    }
                    (slots, text_positions, first_id, hovered)
                };
                let baseline = render(vec![], false);
                let point = baseline.0[0].center();
                for (selected, events) in [
                    (false, vec![egui::Event::PointerMoved(point)]),
                    (
                        false,
                        vec![egui::Event::PointerButton {
                            pos: point,
                            button: egui::PointerButton::Primary,
                            pressed: true,
                            modifiers: Modifiers::NONE,
                        }],
                    ),
                    (
                        true,
                        vec![egui::Event::PointerButton {
                            pos: point,
                            button: egui::PointerButton::Primary,
                            pressed: false,
                            modifiers: Modifiers::NONE,
                        }],
                    ),
                    (true, vec![]),
                ] {
                    ctx.memory_mut(|m| m.request_focus(baseline.2));
                    let actual = render(events, selected);
                    assert_eq!(actual.0, baseline.0, "Slot/wrap changed with interaction");
                    assert_eq!(actual.1, baseline.1, "Text moved with interaction");
                }
                assert!(render(vec![egui::Event::PointerMoved(point)], false).3);
            }
        }
    }
    #[test]
    fn alternating_surfaces_are_painted_on_cells_not_background_bands() {
        for palette in [
            super::super::palette::Palette::Studio,
            super::super::palette::Palette::Native,
        ] {
            for dark in [true, false] {
                assert_ne!(
                    cell_base_fill(palette, dark, 1),
                    cell_base_fill(palette, dark, 2)
                );
                for id in 1..=70 {
                    assert_eq!(
                        cell_base_fill(palette, dark, id),
                        cell_base_fill(palette, dark, id + 10)
                    );
                }
            }
        }
        let ctx = egui::Context::default();
        let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
            for logical_id in [1, 2, 11, 12] {
                let rect = cell_rect(egui::pos2(40.0, 40.0), 60.0, 43.2, logical_id);
                cell_surface(
                    ui,
                    rect,
                    egui::Id::new(logical_id),
                    slot_position(logical_id - 1).1 + 1,
                    Some("white"),
                    false,
                    false,
                );
            }
        });
        output.textures_delta.clear();
        let fills: Vec<_> = output
            .shapes
            .iter()
            .filter_map(|s| match &s.shape {
                egui::Shape::Rect(r) if r.rect.width() == 53.0 => Some(r.fill),
                _ => None,
            })
            .collect();
        assert_eq!(fills.len(), 4);
        assert_eq!(fills[0], fills[1]);
        assert_ne!(fills[0], fills[2]);
        assert_eq!(fills[2], fills[3]);
        let shell = include_str!("shell.rs");
        assert!(
            !shell.contains("let band ="),
            "No table background band painter"
        );
    }
    #[test]
    fn keyboard_activation_does_not_double_toggle_or_repeat() {
        let ctx = egui::Context::default();
        let id = egui::Id::new("activation-cell");
        let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(53.0, 36.2));
        let mut selection = Selection::default();
        selection.click(1, false, false);
        let mut first = ctx.run_ui(egui::RawInput::default(), |ui| {
            cell_surface(ui, rect, id, 1, Some("white"), true, false);
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
                let response = cell_surface(ui, rect, id, 1, Some("white"), true, false);
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
            for (index, (state, selected)) in [("white", false), ("red", true), ("off", true)]
                .into_iter()
                .enumerate()
            {
                if index == 2 {
                    ctx.memory_mut(|m| m.request_focus(id));
                }
                let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                    let response = cell_surface(ui, rect, id, 1, Some(state), selected, false);
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
    fn native_and_web_share_the_exact_selection_transition_contract() {
        let contract: serde_json::Value = serde_json::from_str(include_str!(
            "../../../assets/cells/selection-contract.json"
        ))
        .unwrap();
        let mut selection = Selection::default();
        let mut active = 1;
        for step in contract["steps"].as_array().unwrap() {
            let number = |name: &str| step[name].as_u64().unwrap() as u8;
            match step["op"].as_str().unwrap() {
                "find" => {
                    let invalid = selection
                        .replace_query(step["query"].as_str().unwrap())
                        .is_err();
                    assert_eq!(invalid, step["invalid"].as_bool().unwrap_or(false));
                    if !invalid && let Some(first) = selection.ids.first() {
                        active = *first;
                    }
                }
                "click" => {
                    active = number("id");
                    selection.click(
                        active,
                        step["toggle"].as_bool().unwrap_or(false),
                        step["extend"].as_bool().unwrap_or(false),
                    );
                }
                "row" => {
                    let start = number("row") * 10 + 1;
                    selection.group(start..start + 10, false);
                }
                "clear" => selection.clear(),
                "next" => {
                    active = selection.navigate(active, false).unwrap();
                }
                op => panic!("Unknown shared selection operation: {op}"),
            }
            let expected: Vec<u8> = serde_json::from_value(step["ids"].clone()).unwrap();
            assert_eq!(selection.cells(), expected, "{step}");
            assert_eq!(
                super::super::cell_search::format_selection(&selection.ids),
                step["canonical"].as_str().unwrap()
            );
            if let Some(expected_active) = step["active"].as_u64() {
                assert_eq!(active, expected_active as u8);
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
    fn highlighted_cell_keeps_one_style_when_selected_and_focused() {
        for dark in [true, false] {
            let ctx = egui::Context::default();
            ctx.set_visuals(if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            });
            let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(53.0, 36.2));
            let id = egui::Id::new("highlight-precedence");
            let mut expected = None;
            for state in [None, Some("white"), Some("red"), Some("off")] {
                for focus in [false, true] {
                    ctx.memory_mut(|m| {
                        if focus {
                            m.request_focus(id)
                        } else {
                            m.surrender_focus(id)
                        }
                    });
                    let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                        cell_surface(ui, rect, id, 1, state, true, false);
                    });
                    output.textures_delta.clear();
                    let surface = output
                        .shapes
                        .iter()
                        .find_map(|s| match &s.shape {
                            egui::Shape::Rect(r) if r.rect == rect => Some((r.fill, r.stroke)),
                            _ => None,
                        })
                        .unwrap();
                    if let Some(expected) = expected {
                        assert_eq!(surface, expected)
                    } else {
                        expected = Some(surface)
                    }
                    assert!(
                        output.shapes.iter().all(|s| match &s.shape {
                            egui::Shape::Rect(r) if r.rect == rect.shrink(4.0) =>
                                r.stroke.color.a() == 0,
                            _ => true,
                        }),
                        "No competing focus ring on selection or search highlights"
                    );
                }
            }
        }
    }
    #[test]
    fn led_state_and_emphasis_never_tint_the_neutral_cell_surface() {
        for dark in [false, true] {
            let ctx = egui::Context::default();
            ctx.set_visuals(if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            });
            let rect = Rect::from_min_size(egui::pos2(40.0, 40.0), egui::vec2(53.0, 36.2));
            for state in [None, Some("white"), Some("red"), Some("off")] {
                for selected in [false, true] {
                    let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                        cell_surface(
                            ui,
                            rect,
                            egui::Id::new("tint-free"),
                            1,
                            state,
                            selected,
                            false,
                        );
                    });
                    output.textures_delta.clear();
                    let fill = output
                        .shapes
                        .iter()
                        .find_map(|s| match &s.shape {
                            egui::Shape::Rect(r) if r.rect == rect => Some(r.fill),
                            _ => None,
                        })
                        .unwrap();
                    assert_eq!(
                        fill,
                        cell_base_fill(crate::config::live().color_palette, dark, 1)
                    );
                }
            }
        }
    }
    #[test]
    fn indicators_are_hidden_in_new_and_existing_preferences() {
        assert!(!crate::config::AppConfig::default().show_cell_indicators);
        let mut config: crate::config::AppConfig = serde_json::from_str("{}").unwrap();
        assert!(!config.show_cell_indicators);
        config.show_cell_indicators = true;
        let restored: crate::config::AppConfig =
            serde_json::from_str(&serde_json::to_string(&config).unwrap()).unwrap();
        assert!(restored.show_cell_indicators);
    }
    #[test]
    fn keyboard_edges_follow_interleaved_visual_columns_without_wrapping() {
        assert_eq!(navigate(1, Key::ArrowRight, false), 11);
        assert_eq!(navigate(11, Key::ArrowRight, false), 2);
        assert_eq!(navigate(10, Key::ArrowRight, false), 20);
        assert_eq!(navigate(20, Key::ArrowRight, false), 20);
        assert_eq!(navigate(11, Key::ArrowLeft, false), 1);
        assert_eq!(navigate(1, Key::ArrowLeft, false), 1);
        assert_eq!(navigate(11, Key::ArrowUp, false), 11);
        assert_eq!(navigate(1, Key::ArrowDown, false), 21);
        assert_eq!(navigate(11, Key::ArrowDown, false), 31);
        assert_eq!(navigate(78, Key::ArrowDown, false), 78);
        assert_eq!(navigate(38, Key::Home, false), 21);
        assert_eq!(navigate(38, Key::End, false), 40);
        assert_eq!(navigate(38, Key::End, true), 80);
    }
    #[test]
    fn logical_slots_interleave_without_changing_identity() {
        let first_row: Vec<_> = (0..20).map(|col| position_slot(0, col) + 1).collect();
        assert_eq!(
            first_row,
            [
                1, 11, 2, 12, 3, 13, 4, 14, 5, 15, 6, 16, 7, 17, 8, 18, 9, 19, 10, 20
            ]
        );
        for slot in 0..80 {
            let (row, col) = slot_position(slot);
            assert_eq!(position_slot(row, col), slot);
            let rect = cell_rect(Pos2::ZERO, 64.0, 64.0, slot + 1);
            assert_eq!(rect.min.x, col as f32 * 64.0);
            assert_eq!(
                rect.min.y,
                (row as f32 + if col % 2 == 1 { 0.5 } else { 0.0 }) * 64.0
            );
        }
        let (pitch, height) = grid_metrics(1380.0, 700.0, false);
        assert_eq!(pitch, 69.0);
        assert_eq!(height, pitch);
        assert!(cell_rect(Pos2::ZERO, pitch, height, 80).max.y <= height * 4.5);
        let (pitch, height) = grid_metrics(1380.0, 700.0, true);
        assert_eq!(height, pitch * 0.72);
    }
    #[test]
    fn visual_drag_reorders_visual_neighbors_only_in_edit_mode() {
        let original: Vec<u8> = (1..=80).collect();
        let mut order = original.clone();
        assert!(!reorder_visual_cell(&mut order, false, 1, 11, false));
        assert_eq!(order, original);
        assert!(reorder_visual_cell(&mut order, true, 1, 11, false));
        let visual: Vec<_> = (0..80)
            .map(|i| order[position_slot(i / 20, i % 20) as usize])
            .collect();
        assert_eq!(&visual[..4], &[11, 1, 2, 12]);
        assert_eq!(
            order.iter().copied().collect::<BTreeSet<_>>(),
            (1..=80).collect()
        );
        assert_eq!(navigate_order(&order, 11, Key::ArrowRight, false), 1);
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
        assert_eq!(s.ids, BTreeSet::from([1, 11, 80]));
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
