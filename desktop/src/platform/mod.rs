pub mod discovery;
pub mod instance;
pub mod notifications;
#[cfg(windows)]
pub mod windows_notifications;
#[cfg(windows)]
pub mod windows_desktop;
#[cfg(windows)]
pub mod windows_jump_list;
pub mod windows_quick_actions;
pub mod windows_tray;
