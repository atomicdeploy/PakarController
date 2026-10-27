//! Pealayer WindowsQuickAction contract with PakarController launch actions.
#![allow(dead_code)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct WindowsQuickAction {
    pub title: &'static str,
    pub arguments: &'static str,
    pub icon_resource_id: u16,
    pub icon_glyph: &'static str,
}
impl WindowsQuickAction {
    pub const fn icon_location_index(self) -> i32 {
        -(self.icon_resource_id as i32)
    }
}
pub const WINDOWS_QUICK_ACTIONS: [WindowsQuickAction; 8] = [
    WindowsQuickAction {
        title: "Connect to controller",
        arguments: "--connect-dialog",
        icon_resource_id: 101,
        icon_glyph: egui_phosphor::regular::PLUG,
    },
    WindowsQuickAction {
        title: "Device settings",
        arguments: "--device-settings",
        icon_resource_id: 106,
        icon_glyph: egui_phosphor::regular::CIRCUITRY,
    },
    WindowsQuickAction {
        title: "Refresh device state",
        arguments: "--refresh",
        icon_resource_id: 107,
        icon_glyph: egui_phosphor::regular::ARROW_CLOCKWISE,
    },
    WindowsQuickAction {
        title: "Toggle fullscreen",
        arguments: "--fullscreen",
        icon_resource_id: 108,
        icon_glyph: egui_phosphor::regular::ARROWS_OUT,
    },
    WindowsQuickAction {
        title: "Preferences",
        arguments: "--preferences",
        icon_resource_id: 102,
        icon_glyph: egui_phosphor::regular::GEAR,
    },
    WindowsQuickAction {
        title: "Workspaces",
        arguments: "--workspaces",
        icon_resource_id: 103,
        icon_glyph: egui_phosphor::regular::TABS,
    },
    WindowsQuickAction {
        title: "About PakarController",
        arguments: "--about",
        icon_resource_id: 104,
        icon_glyph: egui_phosphor::regular::INFO,
    },
    WindowsQuickAction {
        title: "Exit PakarController",
        arguments: "--quit",
        icon_resource_id: 105,
        icon_glyph: egui_phosphor::regular::SIGN_OUT,
    },
];
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn controller_shell_actions_have_unique_native_resource_ids() {
        let ids: std::collections::BTreeSet<_> = WINDOWS_QUICK_ACTIONS
            .iter()
            .map(|a| a.icon_resource_id)
            .collect();
        assert_eq!(ids.len(), WINDOWS_QUICK_ACTIONS.len());
        for action in WINDOWS_QUICK_ACTIONS {
            assert_eq!(
                action.icon_location_index(),
                -(action.icon_resource_id as i32)
            );
        }
    }
}
