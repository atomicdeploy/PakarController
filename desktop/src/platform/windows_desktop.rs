//! Adapted actual Pealayer windows.rs taskbar and DWM decoration operations.
//! Media-time percentages are deliberately replaced by indeterminate activity
//! only while an actual settings/maintenance request is pending. Never fake %.
use windows::{
    Win32::{
        Foundation::HWND,
        Graphics::Dwm::{
            DWMWA_CAPTION_COLOR, DWMWA_TEXT_COLOR, DWMWA_USE_IMMERSIVE_DARK_MODE,
            DwmSetWindowAttribute,
        },
        System::Com::{
            CLSCTX_INPROC_SERVER, COINIT_APARTMENTTHREADED, CoCreateInstance, CoInitializeEx,
            CoUninitialize,
        },
        UI::Shell::{ITaskbarList3, TBPF_INDETERMINATE, TBPF_NOPROGRESS, TaskbarList},
    },
    core::BOOL,
};

pub struct Desktop {
    hwnd: HWND,
    appearance: Option<(bool, crate::config::ColorPalette)>,
    busy: Option<bool>,
}
impl Desktop {
    pub fn new(context: &eframe::CreationContext<'_>) -> Result<Self, String> {
        use raw_window_handle::{HasWindowHandle, RawWindowHandle};
        let handle = context.window_handle().map_err(|e| e.to_string())?;
        let RawWindowHandle::Win32(handle) = handle.as_raw() else {
            return Err("Native Windows root HWND unavailable".into());
        };
        Ok(Self {
            hwnd: HWND(handle.hwnd.get() as *mut _),
            appearance: None,
            busy: None,
        })
    }
    pub fn sync(
        &mut self,
        dark: bool,
        palette: crate::config::ColorPalette,
        busy: bool,
    ) -> Vec<String> {
        let mut errors = Vec::new();
        if self.appearance != Some((dark, palette)) {
            self.appearance = Some((dark, palette));
            let colorref = |role| {
                let c = crate::ui::palette::color(palette, dark, role);
                u32::from(c.r()) | (u32::from(c.g()) << 8) | (u32::from(c.b()) << 16)
            };
            let dark_mode = BOOL::from(dark);
            let caption = colorref("surface-0");
            let text = colorref("text");
            unsafe {
                for result in [
                    DwmSetWindowAttribute(
                        self.hwnd,
                        DWMWA_USE_IMMERSIVE_DARK_MODE,
                        &dark_mode as *const _ as *const _,
                        std::mem::size_of::<BOOL>() as u32,
                    ),
                    DwmSetWindowAttribute(
                        self.hwnd,
                        DWMWA_CAPTION_COLOR,
                        &caption as *const _ as *const _,
                        4,
                    ),
                    DwmSetWindowAttribute(
                        self.hwnd,
                        DWMWA_TEXT_COLOR,
                        &text as *const _ as *const _,
                        4,
                    ),
                ] {
                    if let Err(error) = result {
                        errors.push(format!("Native caption theme unsupported: {error}"));
                    }
                }
            }
        }
        if self.busy != Some(busy) {
            self.busy = Some(busy);
            if let Err(error) = self.set_busy(busy) {
                errors.push(error);
            }
        }
        errors
    }
    fn set_busy(&self, busy: bool) -> Result<(), String> {
        // Balance COM initialization on every path (including creation errors).
        let _apartment = Apartment::new()?;
        unsafe {
            let taskbar: ITaskbarList3 = CoCreateInstance(&TaskbarList, None, CLSCTX_INPROC_SERVER)
                .map_err(|e| e.to_string())?;
            taskbar.HrInit().map_err(|e| e.to_string())?;
            taskbar
                .SetProgressState(
                    self.hwnd,
                    if busy {
                        TBPF_INDETERMINATE
                    } else {
                        TBPF_NOPROGRESS
                    },
                )
                .map_err(|e| e.to_string())
        }
    }
}
pub(crate) struct Apartment;
impl Apartment {
    pub fn new() -> Result<Self, String> {
        unsafe { CoInitializeEx(None, COINIT_APARTMENTTHREADED) }
            .ok()
            .map_err(|e| e.to_string())?;
        Ok(Self)
    }
}
impl Drop for Apartment {
    fn drop(&mut self) {
        unsafe {
            CoUninitialize();
        }
    }
}
