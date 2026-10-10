//! Pealayer shell preferences with PakarController-owned persistence and panels.
pub use crate::ui::palette::Palette as ColorPalette;
use serde::{Deserialize, Serialize};
use std::{
    collections::BTreeMap,
    path::PathBuf,
    sync::{OnceLock, RwLock},
};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AppTheme {
    #[default]
    System,
    Light,
    Dark,
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AppLanguage {
    #[default]
    English,
    Persian,
}
impl AppLanguage {
    pub fn is_rtl(self) -> bool {
        self == Self::Persian
    }
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct NumericInputSteps {
    pub normal: f64,
    pub fine: f64,
    pub coarse: f64,
}
impl NumericInputSteps {
    pub fn for_step(step: f64) -> Self {
        Self {
            normal: step,
            fine: step / 10.0,
            coarse: step * 10.0,
        }
    }
    pub fn is_valid(self) -> bool {
        [self.normal, self.fine, self.coarse]
            .iter()
            .all(|v| v.is_finite() && *v > 0.0)
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WorkspaceProfile {
    pub name: String,
    pub icon: String,
    pub layout: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct AppConfig {
    pub theme: AppTheme,
    pub color_palette: ColorPalette,
    pub accent: String,
    pub language: AppLanguage,
    pub ui_scale: f32,
    pub single_instance: bool,
    pub close_to_tray: bool,
    pub always_on_top: bool,
    pub native_dialogs: bool,
    pub endpoint: String,
    pub user: String,
    pub serial_port: String,
    pub dock_layout: String,
    pub workspace_profiles: BTreeMap<String, WorkspaceProfile>,
    pub numeric_steps: BTreeMap<String, NumericInputSteps>,
}
impl Default for AppConfig {
    fn default() -> Self {
        Self {
            theme: AppTheme::System,
            color_palette: ColorPalette::Studio,
            accent: "#38D27A".into(),
            language: AppLanguage::English,
            ui_scale: 1.0,
            single_instance: true,
            close_to_tray: false,
            always_on_top: false,
            native_dialogs: true,
            endpoint: "http://pakarcontroller".into(),
            user: "admin".into(),
            serial_port: String::new(),
            dock_layout: String::new(),
            workspace_profiles: BTreeMap::new(),
            numeric_steps: BTreeMap::new(),
        }
    }
}
impl AppConfig {
    pub fn path() -> PathBuf {
        if let Some(path) = std::env::var_os("PAKAR_CONFIG_FILE") {
            return path.into();
        }
        std::env::var_os("LOCALAPPDATA")
            .map(PathBuf::from)
            .unwrap_or_else(std::env::temp_dir)
            .join("Programs/PakarController/config/preferences.json")
    }
    pub fn load() -> Result<Self, String> {
        match std::fs::read(Self::path()) {
            Ok(bytes) => {
                serde_json::from_slice(&bytes).map_err(|e| format!("Read preferences: {e}"))
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(Self::default()),
            Err(e) => Err(format!("Read preferences: {e}")),
        }
    }
    pub fn save(&self) -> Result<(), String> {
        if !self.ui_scale.is_finite()
            || !(0.75..=2.0).contains(&self.ui_scale)
            || parse_rgb_hex(&self.accent).is_none()
        {
            return Err("Choose a valid accent and UI scale".into());
        }
        let path = Self::path();
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let bytes = serde_json::to_vec_pretty(self).map_err(|e| e.to_string())?;
        let staged = path.with_extension("pending");
        std::fs::write(&staged, bytes).map_err(|e| e.to_string())?;
        std::fs::rename(&staged, &path).map_err(|e| e.to_string())
    }
}
pub fn parse_rgb_hex(value: &str) -> Option<[u8; 3]> {
    let hex = value.strip_prefix('#').unwrap_or(value);
    if hex.len() != 6 || !hex.is_ascii() {
        return None;
    }
    Some([
        u8::from_str_radix(&hex[0..2], 16).ok()?,
        u8::from_str_radix(&hex[2..4], 16).ok()?,
        u8::from_str_radix(&hex[4..6], 16).ok()?,
    ])
}
fn live_slot() -> &'static RwLock<AppConfig> {
    static VALUE: OnceLock<RwLock<AppConfig>> = OnceLock::new();
    VALUE.get_or_init(Default::default)
}
pub fn live() -> AppConfig {
    live_slot().read().expect("preferences lock").clone()
}
pub fn set_live(config: &AppConfig) {
    *live_slot().write().expect("preferences lock") = config.clone();
}

pub fn apply(ctx: &eframe::egui::Context, config: &AppConfig) {
    use eframe::egui::{Color32, Stroke, Theme, ThemePreference};
    ctx.set_theme(match config.theme {
        AppTheme::System => ThemePreference::System,
        AppTheme::Dark => ThemePreference::Dark,
        AppTheme::Light => ThemePreference::Light,
    });
    let rgb = parse_rgb_hex(&config.accent).unwrap_or([56, 210, 122]);
    let accent = Color32::from_rgb(rgb[0], rgb[1], rgb[2]);
    for theme in [Theme::Dark, Theme::Light] {
        let mut visuals = if theme == Theme::Dark {
            eframe::egui::Visuals::dark()
        } else {
            eframe::egui::Visuals::light()
        };
        crate::ui::palette::apply(&mut visuals, config.color_palette);
        visuals.selection.bg_fill = accent;
        let bright =
            u32::from(rgb[0]) * 299 + u32::from(rgb[1]) * 587 + u32::from(rgb[2]) * 114 > 150_000;
        visuals.selection.stroke = Stroke::new(
            1.0,
            if bright {
                Color32::from_rgb(20, 20, 20)
            } else {
                Color32::WHITE
            },
        );
        visuals.hyperlink_color = accent;
        visuals.window_corner_radius = eframe::egui::CornerRadius::same(10);
        visuals.menu_corner_radius = eframe::egui::CornerRadius::same(8);
        ctx.set_visuals_of(theme, visuals);
    }
    ctx.all_styles_mut(|style| {
        style.interaction.selectable_labels = false;
        style.interaction.multi_widget_text_select = false;
        for font in style.text_styles.values_mut() {
            font.size = font.size.min(13.0);
        }
        style.spacing.item_spacing = eframe::egui::vec2(8.0, 6.0);
        style.spacing.button_padding = eframe::egui::vec2(8.0, 5.0);
        style.spacing.interact_size.y = 28.0;
    });
    ctx.set_zoom_factor(config.ui_scale);
    crate::ui::elegance_theme::set_source_palette(ctx, config.color_palette);
    set_live(config);
    ctx.send_viewport_cmd(eframe::egui::ViewportCommand::WindowLevel(
        if config.always_on_top {
            eframe::egui::WindowLevel::AlwaysOnTop
        } else {
            eframe::egui::WindowLevel::Normal
        },
    ));
}
