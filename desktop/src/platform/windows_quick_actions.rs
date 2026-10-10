//! Pealayer WindowsQuickAction contract with PakarController launch actions.
#![allow(dead_code)]
#[derive(Debug, Clone, Copy)]
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
pub const WINDOWS_QUICK_ACTIONS: [WindowsQuickAction; 5] = [
    WindowsQuickAction {
        title: "Connect to controller",
        arguments: "--connect-dialog",
        icon_resource_id: 101,
        icon_glyph: egui_phosphor::regular::PLUG,
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
