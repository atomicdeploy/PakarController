//! Board-owned games. Native controls use the imported Pealayer dialog section,
//! setting_row and action geometry; local edits never simulate game state.
use crate::{
    DeviceStatus,
    ui::{
        dialog,
        dropdown::{ComboBox, DropdownUiExt},
        icons,
    },
};
use eframe::egui;
use serde_json::{Value, json};

pub const CONFIGURE: &str = "/api/minigame/configure";
pub const CONTROL: &str = "/api/minigame/control";
pub const INPUT: &str = "/api/minigame/input";
pub fn path(path: &str) -> bool {
    matches!(path, CONFIGURE | CONTROL | INPUT)
}
pub fn snapshot(status: &DeviceStatus) -> Option<&Value> {
    status
        .capabilities
        .as_ref()
        .filter(|c| c.role == "first-wins")?;
    status.minigame.as_object().map(|_| &status.minigame)
}
pub fn ready(status: &DeviceStatus) -> bool {
    snapshot(status).is_some_and(|game| game["ready"] == true) && !status.programming_latch
}
pub fn modes(game: &Value) -> Vec<(&str, &str)> {
    let mut modes = Vec::new();
    if let Some(items) = game["available_modes"].as_array() {
        for item in items {
            if let (Some(id), Some(label)) = (item["id"].as_str(), item["label"].as_str()) {
                if !id.is_empty() && !modes.iter().any(|(known, _)| *known == id) {
                    modes.push((id, label));
                }
            }
        }
    }
    modes
}
fn mode_visual(id: &str) -> (&'static str, &'static str) {
    match id {
        "press-to-win" => (
            icons::HAND_TAP,
            "First press wins; reset for the next round",
        ),
        "illuminated" => (
            icons::TARGET,
            "Press the glowing target before its window closes",
        ),
        "color-match" => (
            icons::PALETTE,
            "Cycle button colors to match the indicator colors",
        ),
        "simon" => (
            icons::MUSIC_NOTE,
            "Watch, remember and repeat the light sequence",
        ),
        "memory-pairs" => (icons::COPY, "Reveal colors and find every matching pair"),
        "clear-all" => (
            icons::LIST_CHECKS,
            "Press every illuminated button to clear the board",
        ),
        "color-order" => (icons::LIST, "Press the boxes in palette color order"),
        "switch-pattern" => (
            icons::GRID_FOUR,
            "Toggle buttons to copy the indicator pattern",
        ),
        "reaction-chase" => (
            icons::LIGHTNING,
            "Catch the moving light inside its response window",
        ),
        "mine-field" => (
            icons::DIAMOND,
            "Toggle boxes to all cyan or all yellow; fastest time wins",
        ),
        "toggle" => (
            icons::TOGGLE_RIGHT,
            "Control latched or momentary outputs without a timer",
        ),
        _ => (icons::PLAY, "Board-provided game"),
    }
}
pub fn settings(text: &str, game: &Value) -> Result<Value, String> {
    let patch: Value = serde_json::from_str(text).map_err(|_| "Invalid game settings")?;
    let fields = patch
        .as_object()
        .filter(|p| !p.is_empty())
        .ok_or("Empty game settings")?;
    for (name, value) in fields {
        let valid = match name.as_str() {
            "mode" => value
                .as_str()
                .is_some_and(|id| modes(game).iter().any(|(known, _)| *known == id)),
            "hard"
            | "intro_enabled"
            | "color_arpeggio"
            | "segment_animation"
            | "timer_beeps"
            | "timer_alternate_tones" => value.is_boolean(),
            "boot_mode" => value
                .as_str()
                .is_some_and(|v| matches!(v, "first" | "last")),
            "toggle_kind" => value
                .as_str()
                .is_some_and(|v| matches!(v, "toggle" | "latch" | "press")),
            "outputs" => value.as_array().is_some_and(|rules| {
                rules.len() == 8
                    && rules.iter().all(|rule| {
                        let key = rule["key"].as_u64();
                        match rule["source"].as_str() {
                            Some("disabled" | "active" | "success" | "failure") => key == Some(0),
                            Some("held" | "latched" | "winner" | "lost") => {
                                key.is_some_and(|k| (1..=9).contains(&k))
                            }
                            _ => false,
                        }
                    })
            }),
            field => value.as_u64().is_some_and(|v| {
                bounds(field).is_some_and(|(min, max, _)| (min..=max).contains(&v))
            }),
        };
        if !valid {
            return Err(format!("Invalid game setting: {name}"));
        }
    }
    let get = |name: &str| patch.get(name).unwrap_or(&game["settings"][name]).as_u64();
    if matches!((get("urgent_ms"),get("warning_ms")),(Some(urgent),Some(warning)) if urgent>warning)
    {
        return Err("Urgent time must not exceed warning time".into());
    }
    Ok(patch)
}
fn bounds(field: &str) -> Option<(u64, u64, &'static str)> {
    Some(match field {
        "limit_ms" => (0, 86400000, "Time limit (0 = untimed)"),
        "warning_ms" => (0, 86400000, "Warning"),
        "urgent_ms" => (0, 86400000, "Urgent warning"),
        "penalty_ms" => (0, 86400000, "Mistake penalty"),
        "window_ms" => (100, 60000, "Response window"),
        "show_ms" => (100, 10000, "Show duration"),
        "gap_ms" => (50, 5000, "Sequence gap"),
        "feedback_ms" => (50, 5000, "Feedback duration"),
        "intro_ms" => (100, 10000, "Reveal fade per box"),
        "preview_ms" => (0, 10000, "Pairs preview hold"),
        "mismatch_ms" => (600, 10000, "Wrong pair feedback"),
        "rounds" => (1, 32, "Rounds"),
        "sequence_length" => (1, 32, "Sequence length"),
        "sequence_growth" => (0, 31, "Additional steps per round"),
        "palette_size" => (2, 9, "Palette size"),
        "fixed_target" => (0, 9, "Target box (0 = random)"),
        _ => return None,
    })
}
pub fn validate(
    status: &DeviceStatus,
    path: &str,
    form: &[(String, String)],
) -> Result<(), String> {
    if !ready(status) {
        return Err("Game hardware is not ready; nothing sent".into());
    }
    let get = |key: &str| form.iter().find(|(k, _)| k == key).map(|(_, v)| v.as_str());
    match path {
        CONFIGURE if form.len() == 1 && get("settings").is_some() => {
            settings(get("settings").unwrap(), &status.minigame)?;
        }
        CONTROL
            if form.len() == 1
                && get("action").is_some_and(|v| {
                    matches!(v, "start" | "pause" | "resume" | "reset" | "stop")
                }) => {}
        INPUT
            if form.len() == 2
                && get("key")
                    .is_some_and(|v| v.parse::<u8>().is_ok_and(|k| (1..=9).contains(&k)))
                && get("action").is_some_and(|v| matches!(v, "tap" | "down" | "up")) => {}
        _ => return Err("Invalid game command".into()),
    }
    Ok(())
}
pub fn verify(path: &str, form: &[(String, String)], status: &DeviceStatus) -> bool {
    let Some(game) = snapshot(status) else {
        return false;
    };
    if game["revision"].as_u64().is_none() || game["phase"].as_str().is_none() {
        return false;
    }
    let get = |key: &str| {
        form.iter()
            .find(|(k, _)| k == key)
            .map(|(_, v)| v.as_str())
            .unwrap_or("")
    };
    match path {
        CONFIGURE => {
            serde_json::from_str::<Value>(get("settings"))
                .ok()
                .and_then(|patch| patch.as_object().cloned())
                .is_some_and(|fields| fields.iter().all(|(k, v)| game["settings"][k] == *v))
                && game["phase"] == "ready"
        }
        CONTROL => match get("action") {
            "reset" => game["phase"] == "ready",
            "stop" => game["phase"] == "stopped",
            "pause" => game["phase"] == "paused",
            "start" | "resume" => {
                game["phase"] != "ready" && game["phase"] != "paused" && game["phase"] != "stopped"
            }
            _ => false,
        },
        // Inputs can complete a round immediately. The correlated accepted receipt
        // and its authoritative revision are proof; never invent a local winner.
        INPUT => true,
        _ => false,
    }
}

#[derive(Clone)]
struct Draft {
    base: Value,
    edited: Value,
}
pub struct Request {
    pub path: &'static str,
    pub form: Vec<(String, String)>,
}
pub struct UsbRequest {
    request: Request,
    id: String,
    peer: crate::peer_http::Pin,
    revision: u64,
    pub stamp: crate::connection::Stamp,
}
impl UsbRequest {
    pub fn prepare(
        status: &DeviceStatus,
        request: Request,
        stamp: crate::connection::Stamp,
    ) -> Result<(Self, Value), String> {
        validate(status, request.path, &request.form)?;
        let peer = crate::peer_http::Pin::from_status(status).ok_or("USB game peer unavailable")?;
        if !stamp.current() {
            return Err("USB game connection retired".into());
        }
        let revision = status.minigame["revision"]
            .as_u64()
            .ok_or("Game revision unavailable")?;
        let expires = status
            .command_clock_ms
            .and_then(|v| v.checked_add(8000))
            .ok_or("Game command clock unavailable")?;
        let id = uuid::Uuid::new_v4().simple().to_string();
        let command = match request.path {
            CONFIGURE => "minigame-configure",
            CONTROL => "minigame-control",
            _ => "minigame-input",
        };
        let mut packet = json!({"type":"command","request_id":id,"board_id":status.board_id,"boot_id":status.boot_id,"expires_at_ms":expires,"command":command});
        for (key, value) in &request.form {
            packet[key] = match key.as_str() {
                "settings" => settings(value, &status.minigame)?,
                "key" => json!(value.parse::<u8>().map_err(|_| "Invalid key")?),
                _ => json!(value),
            };
        }
        Ok((
            Self {
                request,
                id,
                peer,
                revision,
                stamp,
            },
            packet,
        ))
    }
    pub fn verify(&self, value: Value) -> Result<DeviceStatus, String> {
        if !self.stamp.current()
            || value["type"] != "command-result"
            || value["request_id"] != self.id
            || !self.peer.matches_identity(
                value["board_id"].as_str().unwrap_or(""),
                value["boot_id"].as_str().unwrap_or(""),
            )
        {
            return Err("USB game owner or receipt changed; nothing replayed".into());
        }
        if value["accepted"] != true {
            return Err(format!(
                "Game rejected: {}",
                value["reason"]
                    .as_str()
                    .or(value["error"].as_str())
                    .unwrap_or("unknown")
            ));
        }
        let status: DeviceStatus = serde_json::from_value(value["status"].clone())
            .map_err(|_| "Missing game status readback")?;
        if crate::peer_http::Pin::from_status(&status).as_ref() != Some(&self.peer)
            || status.minigame["revision"]
                .as_u64()
                .is_none_or(|revision| revision < self.revision)
            || !verify(self.request.path, &self.request.form, &status)
        {
            return Err("USB game readback differs; nothing replayed".into());
        }
        if status.compatibility.is_some() {
            crate::peer_identity::validate_board(&status)?;
        }
        Ok(status)
    }
}
pub fn input(key: u8, action: &str) -> Request {
    Request {
        path: INPUT,
        form: vec![
            ("key".into(), key.to_string()),
            ("action".into(), action.into()),
        ],
    }
}
pub fn input_action(ui: &egui::Ui, status: &DeviceStatus) -> String {
    if !hold_controls(status) {
        return "tap".into();
    }
    ui.data(|data| {
        data.get_temp::<String>(egui::Id::new((
            "minigame-input",
            &status.board_id,
            &status.boot_id,
            status.minigame["mode"].as_str().unwrap_or(""),
        )))
    })
    .unwrap_or_else(|| "tap".into())
}
pub fn controls(ui: &mut egui::Ui, status: &DeviceStatus, writable: bool) -> Vec<Request> {
    panel(ui, status, writable, false)
}
pub fn settings_controls(ui: &mut egui::Ui, status: &DeviceStatus, writable: bool) -> Vec<Request> {
    panel(ui, status, writable, true)
}
pub fn shows_elapsed(mode: &str) -> bool {
    !matches!(mode, "press-to-win" | "toggle")
}
fn winning_elapsed_label(game: &Value) -> Option<String> {
    if game["mode"] != "mine-field" || game["phase"] != "success" {
        return None;
    }
    game["elapsed_ms"]
        .as_u64()
        .map(|ms| format!("Winning time: {}.{:03} s", ms / 1000, ms % 1000))
}
fn setting_visible(mode: &str, field: &str, timed: bool) -> bool {
    match field {
        "limit_ms" | "warning_ms" | "urgent_ms" => timed,
        "penalty_ms" => timed && mode == "color-order",
        "window_ms" | "fixed_target" => matches!(mode, "illuminated" | "reaction-chase"),
        "show_ms" => matches!(mode, "simon" | "mine-field"),
        "gap_ms" | "sequence_length" | "sequence_growth" => mode == "simon",
        "intro_ms" => matches!(mode, "color-match" | "memory-pairs"),
        "preview_ms" | "mismatch_ms" => mode == "memory-pairs",
        "rounds" => matches!(mode, "illuminated" | "reaction-chase" | "simon"),
        "palette_size" => matches!(mode, "color-match" | "memory-pairs" | "color-order"),
        "feedback_ms" => !matches!(mode, "press-to-win" | "toggle"),
        _ => true,
    }
}
fn setting_caption(mode: &str, field: &str) -> Option<&'static str> {
    if mode == "mine-field" && field == "show_ms" {
        Some("Pulse period")
    } else {
        bounds(field).map(|(_, _, label)| label)
    }
}
// Explicit picker changes only; rendering or reopening the current mode must
// never overwrite an edited timing value. Match Config::defaults(Simon).
fn select_mode(draft: &mut Value, selected: &str) {
    if draft["mode"] == selected {
        return;
    }
    draft["limit_ms"] = json!(
        if matches!(selected, "press-to-win" | "toggle" | "clear-all") {
            0
        } else {
            60000
        }
    );
    draft["window_ms"] = json!(if selected == "illuminated" {
        10000
    } else {
        1200
    });
    if selected == "simon" {
        draft["rounds"] = json!(3);
        draft["sequence_length"] = json!(3);
        draft["show_ms"] = json!(300);
        draft["gap_ms"] = json!(120);
        // Optional additive setting: don't invent support on an older board.
        if draft.get("sequence_growth").is_some() {
            draft["sequence_growth"] = json!(2);
        }
    }
    if selected == "mine-field" && draft.get("show_ms").is_some() {
        draft["show_ms"] = json!(1000);
    }
    draft["mode"] = json!(selected);
}
pub fn input_controls(ui: &mut egui::Ui, status: &DeviceStatus) {
    if !hold_controls(status) {
        return;
    }
    let id = egui::Id::new((
        "minigame-input",
        &status.board_id,
        &status.boot_id,
        status.minigame["mode"].as_str().unwrap_or(""),
    ));
    let mut action = input_action(ui, status);
    ui.horizontal_wrapped(|ui| {
        ui.weak("On click");
        for (value, label) in [("tap", "Tap"), ("down", "Hold"), ("up", "Release")] {
            if crate::ui::effect_controls::choice(ui, icons::HAND_TAP, label, action == value, true)
                .clicked()
            {
                action = value.into();
            }
        }
    });
    ui.data_mut(|data| data.insert_temp(id, action));
}
fn hold_controls(status: &DeviceStatus) -> bool {
    status.minigame["mode"] == "color-match"
        || (status.minigame["mode"] == "toggle"
            && status.minigame["settings"]["toggle_kind"] == "press")
}
fn panel(
    ui: &mut egui::Ui,
    status: &DeviceStatus,
    writable: bool,
    settings_only: bool,
) -> Vec<Request> {
    let Some(game) = snapshot(status) else {
        return Vec::new();
    };
    let mut requests = Vec::new();
    let enabled = writable && ready(status);
    dialog::section(
        ui,
        if settings_only {
            icons::GEAR
        } else {
            icons::PLAY
        },
        if settings_only {
            "Rules & timing"
        } else {
            "Game"
        },
        |ui| {
            let phase = game["phase"].as_str().unwrap_or("Unknown");
            let mode = game["mode"].as_str().unwrap_or("");
            let catalog = modes(game);
            if !settings_only {
                ui.horizontal_wrapped(|ui| {
                    ui.strong(
                        catalog
                            .iter()
                            .find(|(id, _)| *id == mode)
                            .map(|(_, label)| *label)
                            .unwrap_or(mode),
                    );
                    let color = match phase {
                        "success" => ui.visuals().selection.bg_fill,
                        "failure" | "timeout" => ui.visuals().error_fg_color,
                        _ => ui.visuals().text_color(),
                    };
                    ui.colored_label(color, format!("{}  {}", icons::DOT_OUTLINE, phase));
                    if game["timed"] != true {
                        ui.weak("Untimed");
                    }
                    if let Some(winner) = game["winner"].as_u64().filter(|v| *v > 0) {
                        ui.strong(format!("Winner · Box {winner}"));
                    }
                    if let Some(target) = game["target"].as_u64().filter(|v| *v > 0) {
                        ui.label(format!("Target · Box {target}"));
                    }
                });
                let winning_time = winning_elapsed_label(game);
                if let Some(label) = &winning_time {
                    ui.strong(format!("{}  {label}", icons::CLOCK));
                }
                if game["timed"] == true && winning_time.is_none() {
                    if let (Some(remaining), Some(limit)) = (
                        game["remaining_ms"].as_u64(),
                        game["settings"]["limit_ms"].as_u64(),
                    ) {
                        let fraction = if limit > 0 {
                            (remaining as f32 / limit as f32).clamp(0.0, 1.0)
                        } else {
                            0.0
                        };
                        let mut bar = egui::ProgressBar::new(fraction).text(format!(
                            "{}:{:02}.{:01} remaining",
                            remaining / 60000,
                            remaining / 1000 % 60,
                            remaining / 100 % 10
                        ));
                        if game["warning"].as_u64().unwrap_or(0) > 0 {
                            bar = bar.fill(ui.visuals().warn_fg_color);
                        }
                        ui.add(bar);
                    }
                }
                ui.horizontal_wrapped(|ui| {
                    for (field, label) in [
                        ("round", "Round"),
                        ("progress", "Progress"),
                        ("mistakes", "Mistakes"),
                    ] {
                        if shows_elapsed(mode)
                            && let Some(n) = game[field].as_u64()
                        {
                            ui.weak(format!("{label} {n}"));
                        }
                    }
                    if winning_time.is_none()
                        && shows_elapsed(mode)
                        && let Some(ms) = game["elapsed_ms"].as_u64()
                    {
                        ui.weak(format!("Elapsed {}:{:02}", ms / 60000, ms / 1000 % 60));
                    }
                    for (field, label) in
                        [("last_reaction_ms", "Last"), ("best_reaction_ms", "Best")]
                    {
                        if let Some(ms) = game[field].as_u64().filter(|v| *v > 0) {
                            ui.weak(format!("{label} {ms} ms"));
                        }
                    }
                });
                ui.horizontal_wrapped(|ui| {
                    for (field, label) in [
                        ("participating_mask", "Playing"),
                        ("held_mask", "Held"),
                        ("latched_mask", "Latched"),
                    ] {
                        if let Some(mask) = game[field].as_u64() {
                            let keys: Vec<String> = (1..=9)
                                .filter(|key| mask & (1 << (key - 1)) != 0)
                                .map(|key| key.to_string())
                                .collect();
                            if !keys.is_empty() {
                                ui.weak(format!("{label}: {}", keys.join(", ")));
                            }
                        }
                    }
                });
                if !ready(status) {
                    ui.colored_label(
                        ui.visuals().warn_fg_color,
                        "Waiting for game hardware readiness",
                    );
                }
                ui.add_enabled_ui(enabled, |ui| {
                    let running = matches!(phase, "show" | "input" | "feedback");
                    let (primary_action, primary_label) = if running {
                        ("pause", "Pause")
                    } else if phase == "paused" {
                        ("resume", "Resume")
                    } else {
                        ("start", "Start")
                    };
                    // Reuse the measured scoped-action helper: horizontal_wrapped cannot
                    // wrap dialog scopes after their minimum width is discovered.
                    crate::ui::panel_details::action_row(
                        ui,
                        &[
                            (
                                if running { icons::PAUSE } else { icons::PLAY },
                                primary_label,
                            ),
                            (icons::ARROW_COUNTER_CLOCKWISE, "Reset to white"),
                            (icons::STOP, "Stop"),
                        ],
                        |ui| {
                            for (action, label, icon, allowed) in [
                                (
                                    primary_action,
                                    primary_label,
                                    if running { icons::PAUSE } else { icons::PLAY },
                                    true,
                                ),
                                (
                                    "reset",
                                    "Reset to white",
                                    icons::ARROW_COUNTER_CLOCKWISE,
                                    true,
                                ),
                                ("stop", "Stop", icons::STOP, phase != "stopped"),
                            ] {
                                if ui
                                    .add_enabled_ui(allowed, |ui| {
                                        if action == "reset" {
                                            dialog::primary_action_button(ui, icon, label)
                                        } else {
                                            dialog::action_button(ui, icon, label)
                                        }
                                    })
                                    .inner
                                    .clicked()
                                {
                                    requests.push(Request {
                                        path: CONTROL,
                                        form: vec![("action".into(), action.into())],
                                    });
                                }
                            }
                        },
                    );
                });
            }
            if settings_only {
                ui.add_enabled_ui(enabled, |ui| {
                    let id =
                        egui::Id::new(("minigame-settings", &status.board_id, &status.boot_id));
                    let current = &game["settings"];
                    if !current.is_object() {
                        ui.weak("Settings unavailable");
                        return;
                    }
                    let mut draft =
                        ui.data(|data| data.get_temp::<Draft>(id))
                            .unwrap_or_else(|| Draft {
                                base: current.clone(),
                                edited: current.clone(),
                            });
                    if draft.edited == *current || draft.edited == draft.base {
                        draft = Draft {
                            base: current.clone(),
                            edited: current.clone(),
                        };
                    }
                    let stale = draft.base != *current;
                    if stale {
                        ui.colored_label(
                            ui.visuals().warn_fg_color,
                            "Board settings changed. Revert draft to reload before applying.",
                        );
                    }
                    let mut selected = draft.edited["mode"].as_str().unwrap_or(mode).to_owned();
                    ComboBox::from_id_salt("minigame-mode")
                        .selected_text(
                            catalog
                                .iter()
                                .find(|(id, _)| *id == selected)
                                .map(|(_, label)| format!("{}  {label}", mode_visual(&selected).0))
                                .unwrap_or_else(|| selected.clone()),
                        )
                        .width(ui.available_width().min(430.0))
                        .height(460.0)
                        .show_ui(ui, |ui| {
                            for (id, label) in &catalog {
                                let (icon, fallback) = mode_visual(id);
                                let detail = game["available_modes"]
                                    .as_array()
                                    .and_then(|items| items.iter().find(|item| item["id"] == *id))
                                    .and_then(|item| item["description"].as_str())
                                    .unwrap_or(fallback);
                                let response = dialog::navigation_detail_button(
                                    ui,
                                    selected == *id,
                                    icon,
                                    label,
                                    detail,
                                    ui.available_width(),
                                );
                                let response = crate::ui::dropdown::register_choice(
                                    ui,
                                    response,
                                    selected == *id,
                                );
                                if response.clicked() {
                                    selected = (*id).to_owned();
                                    select_mode(&mut draft.edited, &selected);
                                    ui.close();
                                }
                            }
                        });
                    let mut timed = draft.edited["limit_ms"].as_u64().unwrap_or(0) > 0;
                    if ui.checkbox(&mut timed, "Time limit").changed() {
                        draft.edited["limit_ms"] = json!(if timed { 60000 } else { 0 });
                    }
                    for field in [
                        "limit_ms",
                        "warning_ms",
                        "urgent_ms",
                        "penalty_ms",
                        "window_ms",
                        "show_ms",
                        "gap_ms",
                        "feedback_ms",
                        "intro_ms",
                        "preview_ms",
                        "mismatch_ms",
                        "rounds",
                        "sequence_length",
                        "sequence_growth",
                        "palette_size",
                        "fixed_target",
                    ] {
                        if !setting_visible(&selected, field, timed) {
                            continue;
                        }
                        if let Some(mut number) = draft.edited[field].as_u64() {
                            let (min, max, _) = bounds(field).unwrap();
                            let label = setting_caption(&selected, field).unwrap();
                            dialog::setting_row(ui, icons::SLIDERS_HORIZONTAL, label, None, |ui| {
                                let suffix = if field.ends_with("_ms") { " ms" } else { "" };
                                if ui
                                    .add(
                                        egui::DragValue::new(&mut number)
                                            .range(min..=max)
                                            .suffix(suffix),
                                    )
                                    .changed()
                                {
                                    draft.edited[field] = json!(number);
                                }
                            });
                        }
                    }
                    if let Some(policy) = draft.edited["boot_mode"].as_str() {
                        let mut policy = policy.to_owned();
                        dialog::setting_row(ui, icons::POWER, "Game on power-up", None, |ui| {
                            ComboBox::from_id_salt("boot-game")
                                .selected_text(if policy == "last" {
                                    "Last configured game"
                                } else {
                                    "First available game"
                                })
                                .show_ui(ui, |ui| {
                                    ui.dropdown_value(
                                        &mut policy,
                                        "first".into(),
                                        "Press to win (first available game)",
                                    );
                                    ui.dropdown_value(
                                        &mut policy,
                                        "last".into(),
                                        "Last configured game · fresh round",
                                    );
                                });
                        });
                        draft.edited["boot_mode"] = json!(policy);
                    }
                    for (field, label) in [
                        ("intro_enabled", "Reveal colors and notes before play"),
                        ("color_arpeggio", "Arpeggio color notes"),
                        ("segment_animation", "Ready display · perimeter animation"),
                        ("timer_beeps", "Timer · second ticks and minute warning"),
                        ("timer_alternate_tones", "Timer · alternate odd/even tones"),
                    ] {
                        if field == "intro_enabled"
                            && !matches!(selected.as_str(), "color-match" | "memory-pairs")
                        {
                            continue;
                        }
                        if let Some(mut value) = draft.edited[field].as_bool() {
                            if ui.checkbox(&mut value, label).changed() {
                                draft.edited[field] = json!(value);
                            }
                        }
                    }
                    if matches!(selected.as_str(), "switch-pattern" | "color-match")
                        && let Some(mut hard) = draft.edited["hard"].as_bool()
                    {
                        if ui.checkbox(&mut hard, "Hard mode").changed() {
                            draft.edited["hard"] = json!(hard);
                        }
                    }
                    if selected == "toggle"
                        && let Some(kind) = draft.edited["toggle_kind"].as_str()
                    {
                        let mut kind = kind.to_owned();
                        ui.horizontal_wrapped(|ui| {
                            ui.label("Switch behavior");
                            for value in ["toggle", "latch", "press"] {
                                ui.selectable_value(&mut kind, value.into(), value);
                            }
                        });
                        draft.edited["toggle_kind"] = json!(kind);
                    }
                    ui.collapsing("Output mapping", |ui| {
                        if let Some(rules) = draft.edited["outputs"].as_array_mut() {
                            for (index, rule) in rules.iter_mut().enumerate() {
                                ui.push_id(index, |ui| {
                                    ui.horizontal_wrapped(|ui| {
                                        ui.label(format!("Output {}", index + 1));
                                        let mut source = rule["source"]
                                            .as_str()
                                            .unwrap_or("disabled")
                                            .to_owned();
                                        ComboBox::from_id_salt("source")
                                            .selected_text(&source)
                                            .show_ui(ui, |ui| {
                                                for source_id in [
                                                    "disabled", "active", "success", "failure",
                                                    "held", "latched", "winner", "lost",
                                                ] {
                                                    ui.dropdown_value(
                                                        &mut source,
                                                        source_id.into(),
                                                        source_id,
                                                    );
                                                }
                                            });
                                        let keyed = matches!(
                                            source.as_str(),
                                            "held" | "latched" | "winner" | "lost"
                                        );
                                        let mut key = rule["key"].as_u64().unwrap_or(0);
                                        if keyed {
                                            key = key.clamp(1, 9);
                                            ui.add(
                                                egui::DragValue::new(&mut key)
                                                    .range(1..=9)
                                                    .prefix("Box "),
                                            );
                                        } else {
                                            key = 0;
                                        }
                                        *rule = json!({"source":source,"key":key});
                                    });
                                });
                            }
                        }
                    });
                    let patch: serde_json::Map<String, Value> = draft
                        .edited
                        .as_object()
                        .unwrap()
                        .iter()
                        .filter(|(k, v)| draft.base[*k] != **v)
                        .map(|(k, v)| (k.clone(), v.clone()))
                        .collect();
                    let patch = Value::Object(patch);
                    let valid = settings(&patch.to_string(), game);
                    crate::ui::panel_details::action_row(
                        ui,
                        &[
                            (icons::CHECK, "Apply & reset"),
                            (icons::ARROW_COUNTER_CLOCKWISE, "Revert draft"),
                        ],
                        |ui| {
                            if ui
                                .add_enabled_ui(valid.is_ok() && !stale, |ui| {
                                    dialog::primary_action_button(ui, icons::CHECK, "Apply & reset")
                                })
                                .inner
                                .clicked()
                            {
                                requests.push(Request {
                                    path: CONFIGURE,
                                    form: vec![("settings".into(), patch.to_string())],
                                });
                            }
                            if dialog::action_button(
                                ui,
                                icons::ARROW_COUNTER_CLOCKWISE,
                                "Revert draft",
                            )
                            .clicked()
                            {
                                draft = Draft {
                                    base: current.clone(),
                                    edited: current.clone(),
                                };
                            }
                        },
                    );
                    if draft.edited != draft.base {
                        ui.weak("Unsaved settings · applying returns the game to Ready.");
                        if let Err(error) = valid {
                            ui.colored_label(ui.visuals().warn_fg_color, error);
                        }
                    }
                    ui.data_mut(|data| data.insert_temp(id, draft));
                });
            }
        },
    );
    requests
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn minigame_mine_field_success_reports_precise_elapsed_not_remaining() {
        let mut s = status();
        s.minigame["mode"] = json!("mine-field");
        s.minigame["phase"] = json!("success");
        s.minigame["timed"] = json!(true);
        s.minigame["elapsed_ms"] = json!(1234);
        s.minigame["remaining_ms"] = json!(58766);
        s.minigame["settings"]["limit_ms"] = json!(60000);
        assert_eq!(
            winning_elapsed_label(&s.minigame).as_deref(),
            Some("Winning time: 1.234 s")
        );
        let ctx = egui::Context::default();
        let _ = ctx.run_ui(Default::default(), |ui| {
            assert!(
                controls(ui, &s, true).is_empty(),
                "Viewing success must not start/reset a round"
            );
        });
        s.minigame["phase"] = json!("input");
        assert!(winning_elapsed_label(&s.minigame).is_none());
        s.minigame["phase"] = json!("success");
        s.minigame["mode"] = json!("simon");
        assert!(winning_elapsed_label(&s.minigame).is_none());
    }
    #[test]
    fn minigame_mine_field_preserves_board_catalog_and_contextual_pulse_settings() {
        let mut s = status();
        s.minigame["available_modes"] = json!([
            {"id":"reaction-chase","label":"Reaction chase"},
            {"id":"mine-field","label":"Mine Field"},
            {"id":"toggle","label":"Toggle"}
        ]);
        assert_eq!(
            modes(&s.minigame),
            vec![
                ("reaction-chase", "Reaction chase"),
                ("mine-field", "Mine Field"),
                ("toggle", "Toggle")
            ]
        );
        assert!(
            settings(
                r#"{"mode":"mine-field","limit_ms":60000,"show_ms":1000}"#,
                &s.minigame
            )
            .is_ok()
        );
        assert!(settings(r#"{"mode":"mine-field"}"#, &status().minigame).is_err());
        assert_eq!(mode_visual("mine-field").0, icons::DIAMOND);
        assert!(
            mode_visual("mine-field")
                .1
                .contains("all cyan or all yellow")
        );
        assert!(setting_visible("mine-field", "show_ms", true));
        assert!(!setting_visible("mine-field", "gap_ms", true));
        assert_eq!(
            setting_caption("mine-field", "show_ms"),
            Some("Pulse period")
        );
        assert_eq!(setting_caption("simon", "show_ms"), Some("Show duration"));
    }
    #[test]
    fn minigame_mine_field_defaults_apply_only_on_explicit_selection() {
        let mut draft = json!({"mode":"simon","limit_ms":9000,"show_ms":300,
            "outputs":[{"source":"winner","key":2}],"timer_beeps":false});
        select_mode(&mut draft, "mine-field");
        assert_eq!(draft["limit_ms"], json!(60000));
        assert_eq!(draft["show_ms"], json!(1000));
        assert_eq!(draft["outputs"], json!([{"source":"winner","key":2}]));
        assert_eq!(draft["timer_beeps"], json!(false));
        draft["show_ms"] = json!(1500);
        let edited = draft.clone();
        select_mode(&mut draft, "mine-field");
        assert_eq!(
            draft, edited,
            "Rendering/reopening preserves the pulse draft"
        );
    }
    #[test]
    fn minigame_simon_growth_is_bounded_and_visible_only_in_sequence_settings() {
        let s = status();
        for growth in [0, 2, 31] {
            assert!(settings(&json!({"sequence_growth":growth}).to_string(), &s.minigame).is_ok());
        }
        for growth in [json!(-1), json!(32), json!(2.5), json!("2")] {
            assert!(settings(&json!({"sequence_growth":growth}).to_string(), &s.minigame).is_err());
        }
        assert_eq!(
            bounds("sequence_growth"),
            Some((0, 31, "Additional steps per round"))
        );
        assert!(setting_visible("simon", "sequence_growth", true));
        for mode in ["press-to-win", "toggle", "memory-pairs", "illuminated"] {
            assert!(!setting_visible(mode, "sequence_growth", true));
        }
    }
    #[test]
    fn minigame_simon_defaults_apply_only_on_explicit_mode_change() {
        let original = json!({"mode":"press-to-win","rounds":8,"sequence_length":9,
            "sequence_growth":0,"show_ms":800,"gap_ms":300,"limit_ms":0,
            "boot_mode":"last","timer_beeps":false,"outputs":[{"source":"winner","key":2}]});
        let mut draft = original.clone();
        select_mode(&mut draft, "simon");
        for (field, expected) in [
            ("rounds", 3),
            ("sequence_length", 3),
            ("sequence_growth", 2),
            ("show_ms", 300),
            ("gap_ms", 120),
        ] {
            assert_eq!(draft[field], json!(expected), "{field}");
        }
        assert_eq!(draft["outputs"], original["outputs"]);
        assert_eq!(draft["boot_mode"], original["boot_mode"]);
        assert_eq!(draft["timer_beeps"], original["timer_beeps"]);
        draft["sequence_growth"] = json!(0);
        draft["show_ms"] = json!(700);
        let edited = draft.clone();
        select_mode(&mut draft, "simon");
        assert_eq!(draft, edited, "Repainting/reopening must preserve edits");
        select_mode(&mut draft, "press-to-win");
        assert_eq!(draft["limit_ms"], json!(0));
        assert_eq!(draft["outputs"], original["outputs"]);
        let mut earlier_board = original;
        earlier_board
            .as_object_mut()
            .unwrap()
            .remove("sequence_growth");
        select_mode(&mut earlier_board, "simon");
        assert!(earlier_board.get("sequence_growth").is_none());
    }
    #[test]
    fn minigame_growth_readback_must_match_requested_value() {
        let mut s = status();
        let form = vec![("settings".into(), r#"{"sequence_growth":2}"#.into())];
        s.minigame["settings"]["sequence_growth"] = json!(1);
        assert!(!verify(CONFIGURE, &form, &s));
        s.minigame["settings"]["sequence_growth"] = json!(2);
        assert!(verify(CONFIGURE, &form, &s));
    }
    #[test]
    fn untimed_press_and_toggle_hide_irrelevant_elapsed_timer() {
        assert!(!shows_elapsed("press-to-win"));
        assert!(!shows_elapsed("toggle"));
        assert!(shows_elapsed("clear-all"));
        let ctx = egui::Context::default();
        let mut s = status();
        s.minigame["mode"] = json!("press-to-win");
        s.minigame["timed"] = json!(false);
        s.minigame["elapsed_ms"] = json!(12345);
        for _ in 0..2 {
            let mut output = ctx.run_ui(Default::default(), |ui| {
                assert!(controls(ui, &s, true).is_empty());
                assert!(settings_controls(ui, &s, true).is_empty());
            });
            assert!(!format!("{:?}", output.shapes).contains("Elapsed"));
            output.textures_delta.clear();
        }
    }
    fn status() -> DeviceStatus {
        serde_json::from_value(json!({"board_id":"fixture","boot_id":"boot",
            "capabilities":{"role":"first-wins"},"minigame":{"ready":true,"revision":1,
            "mode":"future-game","phase":"ready","available_modes":[{"id":"future-game","label":"Future game"}],
            "settings":{"mode":"future-game","warning_ms":10000,"urgent_ms":3000,"rounds":5,
                "boot_mode":"first","segment_animation":true,"timer_beeps":true,"timer_alternate_tones":true}}})).unwrap()
    }
    #[test]
    fn minigame_catalog_and_partial_settings_are_board_owned() {
        let s = status();
        assert_eq!(modes(&s.minigame), vec![("future-game", "Future game")]);
        assert!(settings(r#"{"mode":"future-game"}"#, &s.minigame).is_ok());
        assert!(settings(r#"{"mode":"simon"}"#, &s.minigame).is_err());
        assert!(settings(r#"{"boot_mode":"last","segment_animation":true,"timer_beeps":true,"timer_alternate_tones":false}"#, &s.minigame).is_ok());
        assert!(settings(r#"{"boot_mode":"resume-winner"}"#, &s.minigame).is_err());
        assert!(settings(r#"{"timer_beeps":1}"#, &s.minigame).is_err());
        assert!(settings(r#"{"warning_ms":2000}"#, &s.minigame).is_err());
        assert_eq!(
            settings(r#"{"rounds":7}"#, &s.minigame).unwrap(),
            json!({"rounds":7})
        );
    }
    #[test]
    fn minigame_presence_readiness_and_input_bounds_fail_closed() {
        let mut s = status();
        assert!(validate(&s, INPUT, &input(9, "up").form).is_ok());
        assert!(validate(&s, INPUT, &input(10, "up").form).is_err());
        assert!(validate(&s, INPUT, &input(1, "invalid").form).is_err());
        s.minigame["ready"] = json!(false);
        assert!(validate(&s, INPUT, &input(1, "tap").form).is_err());
        s.minigame = Value::Null;
        assert!(snapshot(&s).is_none());
        s = status();
        s.capabilities.as_mut().unwrap().role = "table-selection".into();
        assert!(snapshot(&s).is_none());
    }
    #[test]
    fn minigame_configuration_readback_checks_only_the_requested_fields() {
        let mut s = status();
        let form = vec![("settings".into(), r#"{"rounds":7}"#.into())];
        assert!(!verify(CONFIGURE, &form, &s));
        s.minigame["settings"]["rounds"] = json!(7);
        assert!(verify(CONFIGURE, &form, &s));
        s.minigame["phase"] = json!("input");
        assert!(!verify(CONFIGURE, &form, &s));
    }
    #[test]
    fn minigame_panel_open_resize_and_readiness_changes_never_send_commands() {
        for width in [280.0, 720.0] {
            let ctx = egui::Context::default();
            let mut s = status();
            for ready in [true, false, true] {
                s.minigame["ready"] = json!(ready);
                let mut output = ctx.run_ui(
                    egui::RawInput {
                        screen_rect: Some(egui::Rect::from_min_size(
                            egui::Pos2::ZERO,
                            egui::vec2(width, 1000.0),
                        )),
                        ..Default::default()
                    },
                    |ui| {
                        assert!(controls(ui, &s, true).is_empty());
                        assert!(
                            ui.min_rect().right() <= width + 1.0,
                            "panel overflow at width {width}: {:?}",
                            ui.min_rect()
                        );
                    },
                );
                output.textures_delta.clear();
            }
        }
    }
    #[test]
    fn minigame_usb_packet_and_receipt_are_peer_and_connection_bound() {
        let mut s = status();
        s.boot_id = "a".repeat(32);
        s.http_peer_fencing = true;
        s.command_clock_ms = Some(1000);
        let gate = crate::connection::Gate::default();
        let epoch = gate.select(Some(crate::Transport::Serial));
        let (request, packet) = UsbRequest::prepare(
            &s,
            input(2, "down"),
            gate.stamp(crate::Transport::Serial, epoch),
        )
        .unwrap();
        assert_eq!(packet["command"], "minigame-input");
        assert_eq!(packet["key"], 2);
        assert_eq!(packet["expires_at_ms"], 9000);
        let receipt = json!({"type":"command-result","request_id":packet["request_id"],"board_id":s.board_id,"boot_id":s.boot_id,"accepted":true,"status":s});
        assert!(request.verify(receipt.clone()).is_ok());
        let mut wrong = receipt.clone();
        wrong["status"]["minigame"]["revision"] = json!(0);
        assert!(request.verify(wrong).is_err());
        gate.select(None);
        assert!(request.verify(receipt).is_err());
    }
}
