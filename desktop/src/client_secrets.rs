//! Adapt PCController secretstore/backend_windows.go's current-user generic
//! Credential Manager backend. No plaintext fallback or secret-bearing Debug.
#[cfg(windows)]
mod native {
    use std::ffi::c_void;
    #[repr(C)]
    struct Credential {
        flags: u32,
        kind: u32,
        target: *mut u16,
        comment: *mut u16,
        written: [u32; 2],
        size: u32,
        blob: *mut u8,
        persist: u32,
        count: u32,
        attributes: *mut c_void,
        alias: *mut u16,
        user: *mut u16,
    }
    #[link(name = "advapi32")]
    unsafe extern "system" {
        fn CredReadW(target: *const u16, kind: u32, flags: u32, out: *mut *mut Credential) -> i32;
        fn CredWriteW(value: *const Credential, flags: u32) -> i32;
        fn CredFree(value: *mut c_void);
    }
    pub fn get(target: &str) -> Result<Option<String>, String> {
        let name: Vec<u16> = target.encode_utf16().chain(Some(0)).collect();
        let mut value = std::ptr::null_mut();
        if unsafe { CredReadW(name.as_ptr(), 1, 0, &mut value) } == 0 {
            return if std::io::Error::last_os_error().raw_os_error() == Some(1168) {
                Ok(None)
            } else {
                Err("Windows credential store unavailable".into())
            };
        }
        if value.is_null() {
            return Err("Windows credential store returned no value".into());
        }
        let result = unsafe {
            let item = &*value;
            if item.size == 0 || item.size > 4096 || item.blob.is_null() {
                Err("Invalid management credential".into())
            } else {
                String::from_utf8(
                    std::slice::from_raw_parts(item.blob, item.size as usize).to_vec(),
                )
                .map(Some)
                .map_err(|_| "Invalid management credential".into())
            }
        };
        unsafe { CredFree(value.cast()) };
        result
    }
    pub fn set(target: &str, value: &str) -> Result<(), String> {
        if value.is_empty() || value.len() > 4096 {
            return Err("Invalid management credential size".into());
        }
        let mut name: Vec<u16> = target.encode_utf16().chain(Some(0)).collect();
        let mut blob = value.as_bytes().to_vec();
        let item = Credential {
            flags: 0,
            kind: 1,
            target: name.as_mut_ptr(),
            comment: std::ptr::null_mut(),
            written: [0; 2],
            size: blob.len() as u32,
            blob: blob.as_mut_ptr(),
            persist: 2,
            count: 0,
            attributes: std::ptr::null_mut(),
            alias: std::ptr::null_mut(),
            user: std::ptr::null_mut(),
        };
        let result = unsafe { CredWriteW(&item, 0) };
        blob.fill(0);
        if result == 0 {
            Err("Could not save the current-user management credential".into())
        } else {
            Ok(())
        }
    }
}
#[cfg(windows)]
pub use native::{get, set};
#[cfg(not(windows))]
pub fn get(_: &str) -> Result<Option<String>, String> {
    Err("Management enrollment requires the Windows credential store".into())
}
#[cfg(not(windows))]
pub fn set(_: &str, _: &str) -> Result<(), String> {
    Err("Management enrollment requires the Windows credential store".into())
}
