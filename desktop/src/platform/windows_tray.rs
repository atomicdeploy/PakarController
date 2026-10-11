//! Native tray lifecycle adapted from Pealayer's Windows shell integration.
//!
//! The controller does not inherit Pealayer's media commands; it keeps the
//! same always-available Show/Refresh/Exit contract and stable native icon.

use tray_icon::{
    Icon, MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent,
    menu::{Menu, MenuEvent, MenuId, IconMenuItem, PredefinedMenuItem},
};

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
            .with_guid(0x5041_4b41_522d_434f_4e54_524f_4c4c_4552)
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
    pub fn set_tooltip(&self, text: &str) -> Result<(), String> {
        self._icon.set_tooltip(Some(text)).map_err(|e| e.to_string())
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
}
