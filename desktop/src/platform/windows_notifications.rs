//! Actual Pealayer windows.rs::show_system_notification (2440..2471), adapted
//! to the existing tray-icon HWND/GUID rather than creating a second tray icon.
//! Source: 78563ff7501e13cb8e40a60dd181f69f127c550d. Shell notifications, not
//! a WinRT action/progress toast. Acceptance is not evidence of visibility.
use crate::messaging::Severity;
use windows::{
    Win32::{Foundation::{HWND, LPARAM, LRESULT, WPARAM}, UI::Shell::{
        DefSubclassProc, NIF_GUID, NIF_INFO, NIIF_ERROR, NIIF_INFO, NIIF_NOSOUND,
        NIIF_RESPECT_QUIET_TIME, NIIF_WARNING, NIM_MODIFY, NOTIFYICONDATAW,
        RemoveWindowSubclass, SetWindowSubclass, Shell_NotifyIconW,
    }},
    core::GUID,
};
pub const TRAY_GUID: u128 = 0x5041_4b41_522d_434f_4e54_524f_4c4c_4552;
const HOOK_ID: usize = 0x50414b;
// tray-icon =0.26.1's observed, pinned callback contract. Not a guessed HWND.
const TRAY_CALLBACK: u32 = 6002;
#[derive(Default)]
struct Events {
    shown: std::cell::Cell<u64>,
    hidden: std::cell::Cell<u64>,
    timed_out: std::cell::Cell<u64>,
    clicked: std::cell::Cell<u64>,
}
pub struct Hook {
    hwnd: HWND,
    events: Box<Events>,
    handled_clicks: u64,
}
unsafe extern "system" fn callback(hwnd: HWND, msg: u32, wp: WPARAM, lp: LPARAM, _: usize, data: usize) -> LRESULT {
    if msg == TRAY_CALLBACK {
        // The Box address stays stable until the subclass is removed in Drop.
        let events = unsafe { &*(data as *const Events) };
        let counter = match (lp.0 as u32) & 0xffff {
            0x402 => Some(&events.shown), // NIN_BALLOONSHOW
            0x403 => Some(&events.hidden),
            0x404 => Some(&events.timed_out),
            0x405 => Some(&events.clicked),
            _ => None,
        };
        if let Some(counter) = counter { counter.set(counter.get().saturating_add(1)); }
    }
    unsafe { DefSubclassProc(hwnd, msg, wp, lp) }
}
impl Hook {
    pub fn new(hwnd_raw: isize) -> Result<Self, String> {
        if hwnd_raw == 0 { return Err("Notification tray HWND is unavailable".into()); }
        let hwnd = HWND(hwnd_raw as *mut _);
        let events = Box::<Events>::default();
        if !unsafe { SetWindowSubclass(hwnd, Some(callback), HOOK_ID, (&*events as *const Events) as usize) }.as_bool() {
            return Err("Windows notification callback registration failed".into());
        }
        Ok(Self { hwnd, events, handled_clicks: 0 })
    }
    pub fn show(&self, title: &str, message: &str, severity: &Severity, sound: bool) -> Result<(), String> {
        let severity_flag = match severity {
            Severity::Error => NIIF_ERROR,
            Severity::Warning => NIIF_WARNING,
            _ => NIIF_INFO,
        };
        let nid = NOTIFYICONDATAW {
            cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
            hWnd: self.hwnd,
            uFlags: NIF_INFO | NIF_GUID,
            guidItem: GUID::from_u128(TRAY_GUID),
            szInfo: wide_buffer(message),
            szInfoTitle: wide_buffer(title),
            dwInfoFlags: severity_flag | NIIF_RESPECT_QUIET_TIME | if sound { Default::default() } else { NIIF_NOSOUND },
            ..Default::default()
        };
        unsafe { Shell_NotifyIconW(NIM_MODIFY, &nid) }.as_bool().then_some(())
            .ok_or_else(|| "Windows Shell rejected notification; check Explorer/notification settings".into())
    }
    pub fn take_click(&mut self) -> bool {
        let now = self.events.clicked.get();
        let changed = now != self.handled_clicks;
        self.handled_clicks = now;
        changed
    }
    pub fn snapshot(&self) -> serde_json::Value {
        serde_json::json!({"shown":self.events.shown.get(), "hidden":self.events.hidden.get(), "timed_out":self.events.timed_out.get(), "clicked":self.events.clicked.get()})
    }
}
impl Drop for Hook {
    fn drop(&mut self) {
        // Runs before tray HWND destruction. Never leave a dangling callback.
        if !unsafe { RemoveWindowSubclass(self.hwnd, Some(callback), HOOK_ID) }.as_bool() {
            // An unexpected removal failure must never free callback data while
            // Windows could still reference it. One tiny allocation is safer.
            let _ = Box::leak(std::mem::take(&mut self.events));
        }
    }
}
fn wide_buffer<const N: usize>(text: &str) -> [u16; N] {
    let mut out = [0; N];
    let mut cursor = 0;
    for ch in text.chars().filter(|ch| *ch != '\0') {
        let mut units = [0; 2];
        let encoded = ch.encode_utf16(&mut units);
        if cursor + encoded.len() >= N { break; }
        out[cursor..cursor + encoded.len()].copy_from_slice(encoded);
        cursor += encoded.len();
    }
    out
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn utf16_limits_preserve_complete_surrogates_and_termination() {
        assert_eq!(wide_buffer::<4>("AB😀"), [65,66,0,0]);
        assert_eq!(wide_buffer::<5>("AB😀"), [65,66,0xd83d,0xde00,0]);
        assert_eq!(wide_buffer::<4>("A\0B"), [65,66,0,0]);
        assert!(Hook::new(0).is_err());
        assert!(Hook::new(42).is_err());
    }
}
