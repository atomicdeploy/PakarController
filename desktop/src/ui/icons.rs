#![allow(dead_code, unused_imports)]
//! One cross-platform icon vocabulary. These are font-backed Phosphor vectors,
//! not emoji, so glyph availability and color do not vary by host OS.

pub use egui_phosphor::regular::{
    APP_WINDOW, ARROW_CLOCKWISE, ARROW_COUNTER_CLOCKWISE, ARROW_DOWN, ARROW_RIGHT,
    ARROW_SQUARE_OUT, ARROW_UP, ARROWS_IN, ARROWS_OUT, BELL, BROADCAST, CAR, CARET_DOWN,
    CARET_LEFT, CARET_RIGHT, CHECK, CHECK_SQUARE, CIRCUITRY, CLIPBOARD, CLOCK,
    CLOCK_COUNTER_CLOCKWISE, COPY, CPU, DIAMOND, DOOR, DOT_OUTLINE, DOTS_SIX_VERTICAL, DOTS_THREE,
    ERASER, EYE, EYE_SLASH, FAN, FAST_FORWARD, FILE_VIDEO, FIRE, FLOPPY_DISK, FOLDER_OPEN,
    FRAME_CORNERS, GAUGE, GEAR, GLOBE, GRID_FOUR, HAND_TAP, HEADPHONES, HOURGLASS_MEDIUM, IMAGE,
    INFO, KEYBOARD, LAMP, LIGHTBULB, LIGHTNING, LINK, LINK_SIMPLE, LIST, LIST_CHECKS, LOCK,
    MAGNIFYING_GLASS, MINUS, MONITOR_PLAY, MUSIC_NOTE, PALETTE, PAPER_PLANE_TILT, PAUSE,
    PENCIL_SIMPLE, PLAY, PLUG, PLUS, POWER, PROHIBIT, PUSH_PIN, PUSH_PIN_SLASH, RADIO, RECORD,
    REWIND, SCISSORS, SEAT, SELECTION_ALL, SKIP_BACK, SKIP_FORWARD, SLIDERS_HORIZONTAL, SNOWFLAKE,
    SPARKLE, SPEAKER_HIGH, SPEAKER_NONE, SPEAKER_SLASH, STOP_CIRCLE, SUBTITLES, TABS, TARGET,
    TEXT_ALIGN_CENTER, TEXT_ALIGN_LEFT, TEXT_ALIGN_RIGHT, THERMOMETER, TOGGLE_RIGHT, TRASH,
    WARNING, WAVEFORM, X,
};

pub const CONTROL_ICON_PRESETS: &[(&str, &str, &str)] = &[
    ("sparkle", "Sparkle", SPARKLE),
    ("folder", "Folder", FOLDER_OPEN),
    ("plug", "Plug", PLUG),
    ("lightning", "Lightning", LIGHTNING),
    ("lightbulb", "Light bulb", LIGHTBULB),
    ("lamp", "Lamp", LAMP),
    ("fan", "Fan", FAN),
    ("power", "Power", POWER),
    ("speaker", "Speaker", SPEAKER_HIGH),
    ("radio", "Radio", RADIO),
    ("seat", "Seat", SEAT),
    ("car", "Car", CAR),
    ("door", "Door", DOOR),
    ("bell", "Bell", BELL),
    ("fire", "Fire", FIRE),
    ("snowflake", "Snowflake", SNOWFLAKE),
    ("thermometer", "Thermometer", THERMOMETER),
    ("waveform", "Waveform", WAVEFORM),
    ("circuitry", "Circuitry", CIRCUITRY),
    ("gear", "Gear", GEAR),
];

pub const WORKSPACE_ICON_PRESETS: &[(&str, &str, &str)] = &[
    ("monitor", "Monitor", MONITOR_PLAY),
    ("timeline", "Timeline", WAVEFORM),
    ("tabs", "Tabs", TABS),
    ("window", "Window", APP_WINDOW),
    ("video", "Video", FILE_VIDEO),
    ("hardware", "Hardware", CIRCUITRY),
    ("effects", "Effects", SPARKLE),
    ("layout", "Layout", SELECTION_ALL),
];

pub fn workspace_icon(name: &str) -> &'static str {
    WORKSPACE_ICON_PRESETS
        .iter()
        .find(|(key, _, _)| key.eq_ignore_ascii_case(name.trim()))
        .map(|(_, _, glyph)| *glyph)
        .unwrap_or(APP_WINDOW)
}

pub fn workspace_icon_name(name: &str) -> &'static str {
    WORKSPACE_ICON_PRESETS
        .iter()
        .find(|(key, _, _)| key.eq_ignore_ascii_case(name.trim()))
        .map(|(_, label, _)| *label)
        .unwrap_or("Window")
}

pub fn named_control_icon(name: &str) -> Option<&'static str> {
    CONTROL_ICON_PRESETS
        .iter()
        .find(|(key, _, _)| key.eq_ignore_ascii_case(name.trim()))
        .map(|(_, _, glyph)| *glyph)
}

pub fn control_icon_name(name: &str) -> Option<&'static str> {
    CONTROL_ICON_PRESETS
        .iter()
        .find(|(key, _, _)| key.eq_ignore_ascii_case(name.trim()))
        .map(|(_, label, _)| *label)
}

fn icon_preset_matches(key: &str, label: &str, query: &str) -> bool {
    query.trim().is_empty()
        || !icon_match_ranges(key, query).is_empty()
        || !icon_match_ranges(label, query).is_empty()
}

/// Presentation and catalog data for the shared icon picker. Keeping this in
/// one component prevents workspace, effect, and hardware-channel selectors
/// from drifting back to unrelated text boxes and preset menus.
#[derive(Clone, Copy)]
pub struct IconPickerConfig<'a> {
    pub language: crate::config::AppLanguage,
    pub presets: &'a [(&'a str, &'a str, &'a str)],
    pub fallback_glyph: &'a str,
    pub fallback_name: &'a str,
    pub width: f32,
    pub show_selected_name: bool,
    pub search_hint: &'a str,
    pub presets_label: &'a str,
    pub no_matches_label: &'a str,
    pub clear_label: Option<&'a str>,
}

const ICON_PICKER_GRID_ID: &str = "shared-icon-picker-grid-view";

fn icon_popup_width(requested: f32, screen_width: f32) -> f32 {
    // A tiny card button must not inherit the card/sidebar's width. Keep enough
    // room for search and the view switches, but don't stretch into a wide menu.
    requested
        .clamp(280.0, 320.0)
        .min((screen_width - 24.0).max(180.0))
}

fn icon_combobox_popup_width(requested: f32, screen_width: f32) -> f32 {
    requested.max(280.0).min((screen_width - 24.0).max(180.0))
}

fn icon_match_ranges(text: &str, query: &str) -> Vec<std::ops::Range<usize>> {
    let query = query
        .chars()
        .filter(|character| !character.is_whitespace())
        .flat_map(char::to_lowercase)
        .collect::<String>();
    if query.is_empty() {
        return Vec::new();
    }
    let mut folded = String::new();
    let mut boundaries = Vec::new();
    for (start, character) in text.char_indices() {
        if character.is_whitespace() {
            continue;
        }
        let end = start + character.len_utf8();
        for lower in character.to_lowercase() {
            boundaries.push((folded.len(), start, end));
            folded.push(lower);
        }
    }
    let mut ranges: Vec<std::ops::Range<usize>> = Vec::new();
    for (start, matched) in folded.match_indices(&query) {
        let end = start + matched.len();
        let original_start = boundaries
            .iter()
            .find(|(offset, _, _)| *offset == start)
            .unwrap()
            .1;
        let original_end = boundaries
            .iter()
            .rev()
            .find(|(offset, _, _)| *offset < end)
            .unwrap()
            .2;
        if let Some(previous) = ranges
            .last_mut()
            .filter(|range| range.end >= original_start)
        {
            previous.end = previous.end.max(original_end);
        } else {
            ranges.push(original_start..original_end);
        }
    }
    ranges
}

fn icon_result_text(
    ui: &eframe::egui::Ui,
    glyph: &str,
    label: &str,
    query: &str,
    grid: bool,
) -> eframe::egui::text::LayoutJob {
    use eframe::egui;
    let mut job = egui::text::LayoutJob::default();
    let mut normal = egui::TextFormat {
        font_id: egui::TextStyle::Button.resolve(ui.style()),
        color: ui.visuals().text_color(),
        valign: egui::Align::Center,
        ..Default::default()
    };
    if grid {
        normal.font_id.size = 11.0;
        job.wrap.max_rows = 2;
    }
    let mut icon_format = normal.clone();
    if grid {
        icon_format.font_id.size = 18.0;
        job.halign = egui::Align::Center;
    }
    job.append(glyph, 0.0, icon_format);
    job.append(if grid { "\n" } else { "  " }, 0.0, normal.clone());
    let mut cursor = 0;
    for range in icon_match_ranges(label, query) {
        job.append(&label[cursor..range.start], 0.0, normal.clone());
        let mut highlighted = normal.clone();
        highlighted.background = ui.visuals().selection.bg_fill.linear_multiply(0.28);
        highlighted.underline = egui::Stroke::new(1.0, ui.visuals().selection.stroke.color);
        job.append(&label[range.clone()], 0.0, highlighted);
        cursor = range.end;
    }
    job.append(&label[cursor..], 0.0, normal);
    job
}

fn icon_preset<'a>(
    presets: &'a [(&'a str, &'a str, &'a str)],
    value: &str,
) -> Option<&'a (&'a str, &'a str, &'a str)> {
    presets
        .iter()
        .find(|(key, _, _)| key.eq_ignore_ascii_case(value.trim()))
}

fn icon_grid_tile(
    ui: &mut eframe::egui::Ui,
    size: eframe::egui::Vec2,
    text: eframe::egui::text::LayoutJob,
    selected: bool,
    hover_label: &str,
) -> eframe::egui::Response {
    use eframe::egui;
    let (rect, response) = ui.allocate_exact_size(size, egui::Sense::click());
    if ui.is_rect_visible(rect) {
        let visuals = ui.style().interact_selectable(&response, selected);
        let (fill, stroke) = if selected {
            (
                ui.visuals().selection.bg_fill,
                ui.visuals().selection.stroke,
            )
        } else if response.hovered() || response.highlighted() {
            (visuals.bg_fill, visuals.bg_stroke)
        } else {
            (
                visuals.bg_fill,
                ui.visuals().widgets.noninteractive.bg_stroke,
            )
        };
        ui.painter().rect(
            rect,
            visuals.corner_radius,
            fill,
            stroke,
            egui::StrokeKind::Inside,
        );
        let galley = ui.fonts_mut(|fonts| fonts.layout_job(text));
        let pos = egui::pos2(
            rect.center().x - galley.rect.center().x,
            rect.center().y - galley.rect.center().y,
        );
        let text_color = if selected {
            ui.visuals().selection.stroke.color
        } else {
            visuals.text_color()
        };
        ui.painter().galley(pos, galley, text_color);
    }
    response.on_hover_text(hover_label)
}

/// Draw the popup body used by both combobox selectors and compact icon
/// buttons. The first row is always search; optional defaults and preset
/// results form separate, consistently divided sections below it.
pub fn searchable_icon_picker_contents(
    ui: &mut eframe::egui::Ui,
    value: &mut String,
    search: &mut String,
    search_id: eframe::egui::Id,
    request_focus: bool,
    config: IconPickerConfig<'_>,
) -> bool {
    use eframe::egui;

    let previous = value.clone();
    let view_id = egui::Id::new(ICON_PICKER_GRID_ID);
    let mut grid = ui.data_mut(|data| data.get_persisted::<bool>(view_id).unwrap_or(false));
    let previous_grid = grid;
    let list_label = crate::ui::i18n::tr(config.language, "List view");
    let grid_label = crate::ui::i18n::tr(config.language, "Grid view");
    let search_response = ui
        .horizontal(|ui| {
            ui.label(egui::RichText::new(MAGNIFYING_GLASS).color(ui.visuals().weak_text_color()));
            let switch_width = 26.0;
            let search_width = (ui.available_width()
                - switch_width * 2.0
                - 20.0
                - ui.spacing().item_spacing.x * 3.0)
                .max(70.0);
            let search_response = ui.add_sized(
                [search_width, ui.spacing().interact_size.y],
                crate::ui::dialog::singleline_text_edit(search)
                    .id_salt(search_id.with("input"))
                    .hint_text(config.search_hint)
                    .desired_width(search_width),
            );
            if ui
                .add_sized(
                    [switch_width, ui.spacing().interact_size.y],
                    egui::Button::new(LIST).selected(!grid),
                )
                .on_hover_text(list_label)
                .clicked()
            {
                grid = false;
            }
            if ui
                .add_sized(
                    [switch_width, ui.spacing().interact_size.y],
                    egui::Button::new(GRID_FOUR).selected(grid),
                )
                .on_hover_text(grid_label)
                .clicked()
            {
                grid = true;
            }
            search_response
        })
        .inner;
    ui.data_mut(|data| data.insert_persisted(view_id, grid));
    if request_focus || grid != previous_grid {
        search_response.request_focus();
    }

    if let Some(clear_label) = config.clear_label {
        ui.separator();
        if ui
            .selectable_label(
                value.trim().is_empty(),
                format!("{}  {clear_label}", config.fallback_glyph),
            )
            .clicked()
        {
            value.clear();
            search.clear();
            ui.close();
        }
    }

    ui.separator();
    ui.label(
        egui::RichText::new(config.presets_label)
            .small()
            .strong()
            .color(ui.visuals().weak_text_color()),
    );

    let matching: Vec<_> = config
        .presets
        .iter()
        .filter(|(key, label, _)| icon_preset_matches(key, label, search))
        .collect();
    let mut selected = None;
    egui::ScrollArea::vertical()
        .max_height(260.0)
        .show(ui, |ui| {
            if grid {
                // Calculate after ScrollArea reserves its gutter, not from the
                // wider parent. Each tile has a bounded two-line label.
                let columns = ((ui.available_width() + 6.0) / 82.0).floor().max(1.0) as usize;
                let cell_width =
                    (ui.available_width() - 6.0 * (columns - 1) as f32) / columns as f32;
                egui::Grid::new(search_id.with("results-grid"))
                    .num_columns(columns)
                    .min_col_width(cell_width)
                    .max_col_width(cell_width)
                    .spacing([6.0, 6.0])
                    .show(ui, |ui| {
                        for (index, (key, label, glyph)) in matching.iter().enumerate() {
                            let text = icon_result_text(ui, glyph, label, search, true);
                            let selected_state = value.eq_ignore_ascii_case(key);
                            let response = icon_grid_tile(
                                ui,
                                egui::vec2(cell_width, 54.0),
                                text,
                                selected_state,
                                label,
                            );
                            if response.clicked() {
                                selected = Some(*key);
                            }
                            if (index + 1) % columns == 0 {
                                ui.end_row();
                            }
                        }
                    });
            } else {
                for (key, label, glyph) in &matching {
                    let text = icon_result_text(ui, glyph, label, search, false);
                    if ui
                        .selectable_label(value.eq_ignore_ascii_case(key), text)
                        .clicked()
                    {
                        selected = Some(*key);
                    }
                }
            }
        });
    if let Some(key) = selected {
        *value = key.to_string();
        search.clear();
        ui.close();
    }
    if matching.is_empty() {
        ui.label(
            egui::RichText::new(config.no_matches_label)
                .italics()
                .color(ui.visuals().weak_text_color()),
        );
    }

    *value != previous
}

/// A single searchable icon combobox shared by all native egui editors. The
/// closed control shows the selected Phosphor icon and, when space permits,
/// its human-readable name.
pub fn searchable_icon_picker(
    ui: &mut eframe::egui::Ui,
    id_salt: impl std::hash::Hash + std::fmt::Debug,
    value: &mut String,
    config: IconPickerConfig<'_>,
) -> bool {
    use eframe::egui;

    // ComboBox converts the supplied salt to IdSalt before deriving its
    // widget ID. Match that conversion so search survives successive frames
    // and focus is requested only when the popup first opens.
    let button_id = ui.make_persistent_id(egui::IdSalt::new(&id_salt));
    let search_id = button_id.with("search");
    let was_open = egui::ComboBox::is_open(ui.ctx(), button_id);
    let mut search = ui.data_mut(|data| data.get_temp::<String>(search_id).unwrap_or_default());
    let previous = value.clone();
    let selected = icon_preset(config.presets, value);
    let selected_glyph = selected
        .map(|(_, _, glyph)| *glyph)
        .unwrap_or(config.fallback_glyph);
    let selected_name = selected.map(|(_, label, _)| *label).unwrap_or_else(|| {
        if value.trim().is_empty() {
            config.fallback_name
        } else {
            value.trim()
        }
    });
    let selected_text = if config.show_selected_name && !selected_name.is_empty() {
        format!("{selected_glyph}  {selected_name}")
    } else {
        selected_glyph.to_string()
    };

    egui::ComboBox::from_id_salt(&id_salt)
        .width(config.width)
        .height(320.0)
        .truncate()
        .icon(|ui, rect, visuals, _| {
            ui.painter().text(
                rect.center(),
                egui::Align2::CENTER_CENTER,
                CARET_DOWN,
                egui::FontId::proportional(14.0),
                visuals.text_color(),
            );
        })
        .selected_text(selected_text)
        .close_behavior(egui::PopupCloseBehavior::CloseOnClickOutside)
        .show_ui(ui, |ui| {
            ui.set_width(icon_combobox_popup_width(
                config.width,
                ui.ctx().content_rect().width(),
            ));
            searchable_icon_picker_contents(ui, value, &mut search, search_id, !was_open, config);
        });

    if egui::ComboBox::is_open(ui.ctx(), button_id) {
        ui.data_mut(|data| data.insert_temp(search_id, search));
    } else {
        ui.data_mut(|data| data.remove::<String>(search_id));
    }
    *value != previous
}

/// Compact entry point for channel-card icons. It shares the combobox's
/// search, selection, default option, and popup lifecycle without adding a
/// second selector to the card.
pub fn searchable_icon_button(
    ui: &mut eframe::egui::Ui,
    id_salt: impl std::hash::Hash + std::fmt::Debug,
    value: &mut String,
    size: f32,
    tooltip: &str,
    config: IconPickerConfig<'_>,
) -> bool {
    use eframe::egui;

    let picker_id = ui.make_persistent_id(&id_salt);
    let popup_id = picker_id.with("popup");
    let search_id = picker_id.with("search");
    let was_open = egui::Popup::is_id_open(ui.ctx(), popup_id);
    let glyph = icon_preset(config.presets, value)
        .map(|(_, _, glyph)| *glyph)
        .unwrap_or(config.fallback_glyph);
    let response = ui
        .push_id(&id_salt, |ui| {
            ui.add(
                egui::Button::new(egui::RichText::new(glyph).size(size))
                    .frame(false)
                    .min_size(egui::vec2(size + 6.0, size + 6.0)),
            )
        })
        .inner
        .on_hover_text(tooltip);
    let mut search = ui.data_mut(|data| data.get_temp::<String>(search_id).unwrap_or_default());
    let previous = value.clone();
    egui::Popup::menu(&response)
        .id(popup_id)
        .width(icon_popup_width(
            config.width,
            ui.ctx().content_rect().width(),
        ))
        .layout(egui::Layout::top_down(egui::Align::Min))
        .close_behavior(egui::PopupCloseBehavior::CloseOnClickOutside)
        .show(|ui| {
            ui.set_width(icon_popup_width(
                config.width,
                ui.ctx().content_rect().width(),
            ));
            searchable_icon_picker_contents(ui, value, &mut search, search_id, !was_open, config);
        });
    if egui::Popup::is_id_open(ui.ctx(), popup_id) {
        ui.data_mut(|data| data.insert_temp(search_id, search));
    } else {
        ui.data_mut(|data| data.remove::<String>(search_id));
    }
    *value != previous
}

pub fn searchable_control_icon_picker(
    ui: &mut eframe::egui::Ui,
    id_salt: impl std::hash::Hash + std::fmt::Debug,
    value: &mut String,
    width: f32,
    search_hint: &str,
    presets_label: &str,
    no_matches_label: &str,
    language: crate::config::AppLanguage,
) -> bool {
    searchable_icon_picker(
        ui,
        id_salt,
        value,
        IconPickerConfig {
            language,
            presets: CONTROL_ICON_PRESETS,
            fallback_glyph: SPARKLE,
            fallback_name: "Sparkle",
            width,
            show_selected_name: true,
            search_hint,
            presets_label,
            no_matches_label,
            clear_label: None,
        },
    )
}

pub fn searchable_workspace_icon_picker(
    ui: &mut eframe::egui::Ui,
    id_salt: impl std::hash::Hash + std::fmt::Debug,
    value: &mut String,
    width: f32,
    search_hint: &str,
    presets_label: &str,
    no_matches_label: &str,
    language: crate::config::AppLanguage,
) -> bool {
    searchable_icon_picker(
        ui,
        id_salt,
        value,
        IconPickerConfig {
            language,
            presets: WORKSPACE_ICON_PRESETS,
            fallback_glyph: APP_WINDOW,
            fallback_name: "Window",
            width,
            show_selected_name: true,
            search_hint,
            presets_label,
            no_matches_label,
            clear_label: None,
        },
    )
}

pub fn control(kind: &str, advertised: &str) -> &'static str {
    if let Some(icon) = named_control_icon(advertised) {
        return icon;
    }
    match advertised.trim().to_ascii_lowercase().as_str() {
        "armchair" | "seat" => SEAT,
        "lightbulb" | "light" => LIGHTBULB,
        "radio" | "rf" => RADIO,
        _ => match kind {
            "seat" | "motion" => SEAT,
            "mosfet" | "pwm" => LIGHTBULB,
            "relay" => PLUG,
            _ => GAUGE,
        },
    }
}

pub fn action(verb: &str) -> &'static str {
    match verb {
        "up" => ARROW_UP,
        "down" => ARROW_DOWN,
        "stop" | "off" => STOP_CIRCLE,
        "on" => LIGHTNING,
        _ => PLAY,
    }
}

/// Draw a consistent Phosphor-backed disclosure control and persist its state.
/// This avoids platform/font-dependent triangle glyphs from egui's default
/// `CollapsingHeader` while preserving keyboard focus and a full-row hit target.
pub fn disclosure_header(
    ui: &mut eframe::egui::Ui,
    id_salt: impl std::hash::Hash + std::fmt::Debug,
    label: &str,
    default_open: bool,
) -> bool {
    let id = ui.make_persistent_id(id_salt);
    let mut open = ui.data_mut(|data| data.get_persisted::<bool>(id).unwrap_or(default_open));
    let icon = if open { CARET_DOWN } else { CARET_RIGHT };
    let response = ui.add(
        eframe::egui::Button::new(eframe::egui::RichText::new(format!("{icon}  {label}")).strong())
            .frame(false),
    );
    if response.clicked() {
        open = !open;
    }
    ui.data_mut(|data| data.insert_persisted(id, open));
    open
}

/// Draw a submenu entry with the same Phosphor caret used by the rest of the UI.
/// egui's stock submenu indicator is a text glyph (`⏵`), which looks out of
/// place next to the application's vector-icon font and varies across systems.
pub fn submenu(
    ui: &mut eframe::egui::Ui,
    label: String,
    add_contents: impl FnOnce(&mut eframe::egui::Ui),
) {
    let button = eframe::egui::Button::new(label).right_text(CARET_RIGHT);
    let _ = eframe::egui::containers::menu::SubMenuButton::from_button(button).ui(ui, add_contents);
}

pub use egui_phosphor::regular::{BUG, DOWNLOAD_SIMPLE, MOON, SUN, TERMINAL_WINDOW, WIFI_HIGH};
