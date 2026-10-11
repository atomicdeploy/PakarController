//! Native tray lifecycle adapted from Pealayer's Windows shell integration.
//!
//! The controller does not inherit Pealayer's media commands; it keeps the
//! same always-available Show/Refresh/Exit contract and stable native icon.

use tray_icon::{
    Icon, MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent,
    menu::{Menu, MenuEvent, MenuId, IconMenuItem, PredefinedMenuItem},
};
use std::time::{Duration, Instant};

pub const TRAY_GUID: u128 = 0x5041_4b41_522d_434f_4e54_524f_4c4c_4552;

/// Shell acceptance, not object presence or proof that the user saw an icon.
/// Failed read-only presentation updates retry, never device commands/notices.
#[derive(Default)]
struct TooltipSync {
    applied: Option<String>,
    epoch: u64,
    next_attempt: Option<Instant>,
    retry_seconds: u64,
    last_error: Option<String>,
    attempts: u64,
}
impl TooltipSync {
    fn sync(&mut self, text: &str, epoch: u64, now: Instant,
            apply: impl FnOnce() -> Result<(), String>) -> Result<bool, String> {
        if self.epoch != epoch {
            self.epoch = epoch;
            self.applied = None;
            self.next_attempt = None;
            self.retry_seconds = 0;
        }
        if self.applied.as_deref() == Some(text)
            || self.next_attempt.is_some_and(|deadline| now < deadline) {
            return Ok(false);
        }
        self.attempts = self.attempts.saturating_add(1);
        match apply() {
            Ok(()) => {
                self.applied = Some(text.into());
                self.next_attempt = None;
                self.retry_seconds = 0;
                self.last_error = None;
                Ok(true)
            }
            Err(error) => {
                self.applied = None; // Never cache a failed request as applied.
                let delay = self.retry_seconds.max(2);
                self.next_attempt = Some(now + Duration::from_secs(delay));
                self.retry_seconds = (delay * 2).min(30);
                self.last_error = Some(error.clone());
                Err(error)
            }
        }
    }
    fn snapshot(&self, now: Instant) -> serde_json::Value {
        serde_json::json!({"tooltip_applied":self.applied.is_some(),
            "explorer_epoch":self.epoch,"attempts":self.attempts,
            "last_error":self.last_error,"retry_ms":self.next_attempt
                .map(|d|d.saturating_duration_since(now).as_millis())})
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TrayAction {
    Show,
    Preferences,
    DeviceSettings,
    Workspaces,
    Refresh,
    Exit,
}

pub struct TrayController {
    // Declared before the HWND owner so the subclass is removed first.
    #[cfg(windows)]
    notification_hook: super::windows_notifications::Hook,
    _icon: TrayIcon,
    show_item: IconMenuItem,
    device_settings_id: MenuId,
    show_id: MenuId,
    preferences_id: MenuId,
    workspaces_id: MenuId,
    refresh_id: MenuId,
    exit_id: MenuId,
    tooltip: TooltipSync,
}

impl TrayController {
    pub fn new() -> Result<Self, String> {
        let menu = Menu::new();
        let show = icon_item("Show PakarController", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-106.rgba")))?;
        let preferences = icon_item("Preferences", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-102.rgba")))?;
        let device_settings = icon_item("Device settings", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-106.rgba")))?;
        let workspaces = icon_item("Workspaces", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-103.rgba")))?;
        let refresh = icon_item("Refresh device state", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-107.rgba")))?;
        let separator = PredefinedMenuItem::separator();
        let exit = icon_item("Exit", include_bytes!(concat!(env!("OUT_DIR"), "/quick-action-105.rgba")))?;
        menu.append_items(&[
            &show,
            &preferences,
            &device_settings,
            &workspaces,
            &refresh,
            &separator,
            &exit,
        ])
        .map_err(|error| error.to_string())?;

        let icon = Icon::from_rgba(controller_icon(), 32, 32).map_err(|error| error.to_string())?;
        let tray = TrayIconBuilder::new()
            .with_guid(TRAY_GUID)
            .with_tooltip("PakarController")
            .with_menu(Box::new(menu))
            .with_menu_on_left_click(false)
            .with_icon(icon)
            .build()
            .map_err(|error| error.to_string())?;

        Ok(Self {
            #[cfg(windows)]
            notification_hook: super::windows_notifications::Hook::new(tray.window_handle() as isize)?,
            _icon: tray,
            device_settings_id: device_settings.id().clone(),
            show_id: show.id().clone(),
            show_item: show,
            preferences_id: preferences.id().clone(),
            workspaces_id: workspaces.id().clone(),
            refresh_id: refresh.id().clone(),
            exit_id: exit.id().clone(),
            tooltip: TooltipSync::default(),
        })
    }

    pub fn poll(&mut self) -> Option<TrayAction> {
        #[cfg(windows)]
        if self.notification_hook.take_click() { return Some(TrayAction::Show); }
        while let Ok(event) = MenuEvent::receiver().try_recv() {
            if event.id == self.show_id {
                return Some(TrayAction::Show);
            }
            if event.id == self.preferences_id {
                return Some(TrayAction::Preferences);
            }
            if event.id == self.device_settings_id { return Some(TrayAction::DeviceSettings); }
            if event.id == self.workspaces_id {
                return Some(TrayAction::Workspaces);
            }
            if event.id == self.refresh_id {
                return Some(TrayAction::Refresh);
            }
            if event.id == self.exit_id {
                return Some(TrayAction::Exit);
            }
        }
        while let Ok(event) = TrayIconEvent::receiver().try_recv() {
            match event {
                TrayIconEvent::Click {
                    button: MouseButton::Left,
                    button_state: MouseButtonState::Up,
                    ..
                }
                | TrayIconEvent::DoubleClick {
                    button: MouseButton::Left,
                    ..
                } => return Some(TrayAction::Show),
                _ => {}
            }
        }
        None
    }
    pub fn refresh_tooltip(&mut self, text: &str, now: Instant) -> Result<bool, String> {
        #[cfg(windows)]
        {
            // tray-icon0.26.1 set_tooltip omits NIF_GUID. Keep its owned HWND,
            // RAII/menu/restart lifecycle, adapt Pealayer's direct NIM_MODIFY.
            let hook = &self.notification_hook;
            self.tooltip.sync(text, hook.explorer_epoch(), now, || hook.set_tooltip(text))
        }
        #[cfg(not(windows))]
        {
            let icon = &self._icon;
            self.tooltip.sync(text, 0, now, || icon.set_tooltip(Some(text)).map_err(|e|e.to_string()))
        }
    }
    pub fn shell_snapshot(&self) -> serde_json::Value {
        self.tooltip.snapshot(Instant::now())
    }
    pub fn set_application(&self, name: &str, icon: &eframe::egui::IconData) -> Result<(), String> {
        self.show_item.set_text(format!("Show {name}"));
        let image = image::RgbaImage::from_raw(icon.width, icon.height, icon.rgba.clone()).ok_or("Invalid icon extent")?;
        let image = image::imageops::resize(&image, 32, 32, image::imageops::FilterType::Lanczos3);
        self._icon.set_icon(Some(Icon::from_rgba(image.into_raw(), 32, 32).map_err(|e| e.to_string())?)).map_err(|e| e.to_string())
    }
    pub fn notify(&self, request: &crate::messaging::ToastRequest, sound: bool) -> Result<(), String> {
        #[cfg(windows)]
        {
            let (title, message) = super::notifications::private_text(request);
            self.notification_hook.show(title, message, &request.severity, sound)
        }
        #[cfg(not(windows))]
        { let _ = (request, sound); Err("System notifications are implemented on Windows only".into()) }
    }
    pub fn notification_snapshot(&self) -> serde_json::Value {
        #[cfg(windows)]
        { self.notification_hook.snapshot() }
        #[cfg(not(windows))]
        { serde_json::json!({"supported":false}) }
    }
}
fn icon_item(label: &str, rgba: &[u8]) -> Result<IconMenuItem, String> {
    let icon = tray_icon::menu::Icon::from_rgba(rgba.to_vec(), 16, 16).map_err(|e| e.to_string())?;
    Ok(IconMenuItem::new(label, true, Some(icon), None))
}

fn controller_icon() -> Vec<u8> {
    include_bytes!(concat!(env!("OUT_DIR"), "/pakar-icon-32.rgba")).to_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tray_icon_has_expected_rgba_extent() {
        assert_eq!(controller_icon().len(), 32 * 32 * 4);
        assert!(
            controller_icon()
                .chunks_exact(4)
                .any(|pixel| pixel[3] == 255)
        );
    }

    #[test]
    fn failed_tooltip_retries_latest_value_without_false_acceptance_or_flood() {
        let mut sync = TooltipSync::default();
        let now = Instant::now();
        assert!(sync.sync("old", 0, now, ||Err("Shell unavailable".into())).is_err());
        assert_eq!(sync.snapshot(now)["tooltip_applied"], false);
        assert!(!sync.sync("latest", 0, now + Duration::from_secs(1), ||panic!("No early retry")).unwrap());
        assert!(sync.sync("latest", 0, now + Duration::from_secs(2), ||Ok(())).unwrap());
        assert_eq!(sync.applied.as_deref(), Some("latest"));
        assert!(!sync.sync("latest", 0, now + Duration::from_secs(3), ||panic!("No repeated write")).unwrap());
        assert!(sync.last_error.is_none());
        assert_eq!(sync.attempts, 2);
    }

    #[test]
    fn explorer_epoch_invalidates_success_and_failure_backoff_is_bounded() {
        let mut sync = TooltipSync::default();
        let now = Instant::now();
        assert!(sync.sync("title", 0, now, ||Ok(())).unwrap());
        assert!(sync.sync("title", 1, now, ||Err("different failure".into())).is_err());
        assert_eq!(sync.snapshot(now)["tooltip_applied"], false);
        let mut clock = now;
        for delay in [2, 4, 8, 16, 30, 30] {
            assert_eq!(sync.next_attempt.unwrap().duration_since(clock), Duration::from_secs(delay));
            clock = sync.next_attempt.unwrap();
            assert!(sync.sync("title", 1, clock, ||Err("offline".into())).is_err());
        }
        // Observed restart retries immediately, but is not itself success.
        assert!(sync.sync("title", 2, clock, ||Ok(())).unwrap());
        assert_eq!(sync.snapshot(clock)["explorer_epoch"], 2);
        assert!(sync.next_attempt.is_none());
    }
}
