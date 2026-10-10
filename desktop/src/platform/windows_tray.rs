//! Native tray lifecycle adapted from Pealayer's Windows shell integration.
//!
//! The controller does not inherit Pealayer's media commands; it keeps the
//! same always-available Show/Refresh/Exit contract and stable native icon.

use tray_icon::{
    Icon, MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent,
    menu::{Menu, MenuEvent, MenuId, MenuItem, PredefinedMenuItem},
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TrayAction {
    Show,
    Preferences,
    Workspaces,
    Refresh,
    Exit,
}

pub struct TrayController {
    _icon: TrayIcon,
    show_id: MenuId,
    preferences_id: MenuId,
    workspaces_id: MenuId,
    refresh_id: MenuId,
    exit_id: MenuId,
}

impl TrayController {
    pub fn new() -> Result<Self, String> {
        let menu = Menu::new();
        let show = MenuItem::new("Show PakarController", true, None);
        let preferences = MenuItem::new("Preferences", true, None);
        let workspaces = MenuItem::new("Workspaces", true, None);
        let refresh = MenuItem::new("Refresh device state", true, None);
        let separator = PredefinedMenuItem::separator();
        let exit = MenuItem::new("Exit", true, None);
        menu.append_items(&[
            &show,
            &preferences,
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
            _icon: tray,
            show_id: show.id().clone(),
            preferences_id: preferences.id().clone(),
            workspaces_id: workspaces.id().clone(),
            refresh_id: refresh.id().clone(),
            exit_id: exit.id().clone(),
        })
    }

    pub fn poll(&self) -> Option<TrayAction> {
        while let Ok(event) = MenuEvent::receiver().try_recv() {
            if event.id == self.show_id {
                return Some(TrayAction::Show);
            }
            if event.id == self.preferences_id {
                return Some(TrayAction::Preferences);
            }
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
