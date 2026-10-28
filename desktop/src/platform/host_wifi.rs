//! Host Wi-Fi only. OS profiles own credentials; no network mutation on discovery.
#[derive(Clone, Debug, PartialEq)]
pub struct Profile {
    pub name: String,
    pub adapter: String,
    #[cfg(windows)]
    interface: windows::core::GUID,
}

#[cfg(windows)]
mod native {
    use super::Profile;
    use windows::{
        Win32::{Foundation::HANDLE, NetworkManagement::WiFi::*},
        core::PCWSTR,
    };
    struct Client(HANDLE);
    impl Drop for Client {
        fn drop(&mut self) {
            unsafe {
                WlanCloseHandle(self.0, None);
            }
        }
    }
    struct Allocation<T>(*mut T);
    impl<T> Drop for Allocation<T> {
        fn drop(&mut self) {
            if !self.0.is_null() {
                unsafe {
                    WlanFreeMemory(self.0.cast());
                }
            }
        }
    }
    fn check(code: u32) -> Result<(), String> {
        if code == 0 {
            Ok(())
        } else {
            Err(format!(
                "Windows Wi-Fi error {code}. Check the adapter, WLAN service and Wi-Fi settings."
            ))
        }
    }
    fn client() -> Result<Client, String> {
        let mut handle = HANDLE::default();
        let mut version = 0;
        check(unsafe { WlanOpenHandle(2, None, &mut version, &mut handle) })?;
        Ok(Client(handle))
    }
    fn text(value: &[u16]) -> String {
        String::from_utf16_lossy(
            &value[..value.iter().position(|c| *c == 0).unwrap_or(value.len())],
        )
    }
    pub fn profiles() -> Result<Vec<Profile>, String> {
        let client = client()?;
        let mut interfaces = Allocation(std::ptr::null_mut::<WLAN_INTERFACE_INFO_LIST>());
        check(unsafe { WlanEnumInterfaces(client.0, None, &mut interfaces.0) })?;
        if interfaces.0.is_null() {
            return Err("Windows returned no Wi-Fi adapters".into());
        }
        let mut result = Vec::new();
        unsafe {
            let count = (*interfaces.0).dwNumberOfItems as usize;
            if count > 64 {
                return Err("Unexpected Wi-Fi adapter count".into());
            }
            let items = std::slice::from_raw_parts(
                std::ptr::addr_of!((*interfaces.0).InterfaceInfo).cast::<WLAN_INTERFACE_INFO>(),
                count,
            );
            for interface in items {
                let mut profiles = Allocation(std::ptr::null_mut::<WLAN_PROFILE_INFO_LIST>());
                check(WlanGetProfileList(
                    client.0,
                    &interface.InterfaceGuid,
                    None,
                    &mut profiles.0,
                ))?;
                if profiles.0.is_null() {
                    continue;
                }
                let count = (*profiles.0).dwNumberOfItems as usize;
                if count > 4096 {
                    return Err("Unexpected saved Wi-Fi profile count".into());
                }
                for profile in std::slice::from_raw_parts(
                    std::ptr::addr_of!((*profiles.0).ProfileInfo).cast::<WLAN_PROFILE_INFO>(),
                    count,
                ) {
                    result.push(Profile {
                        name: text(&profile.strProfileName),
                        adapter: text(&interface.strInterfaceDescription),
                        interface: interface.InterfaceGuid,
                    });
                }
            }
        }
        result.sort_by(|a, b| a.name.cmp(&b.name).then(a.adapter.cmp(&b.adapter)));
        Ok(result)
    }
    pub fn join(profile: &Profile) -> Result<String, String> {
        // Recheck the exact adapter/profile; never select a different interface.
        if !profiles()?.contains(profile) {
            return Err("Saved profile changed; refresh the profile list.".into());
        }
        let client = client()?;
        let name: Vec<u16> = profile.name.encode_utf16().chain(Some(0)).collect();
        let parameters = WLAN_CONNECTION_PARAMETERS {
            wlanConnectionMode: wlan_connection_mode_profile,
            strProfile: PCWSTR(name.as_ptr()),
            dot11BssType: dot11_BSS_type_infrastructure,
            ..Default::default()
        };
        check(unsafe { WlanConnect(client.0, &profile.interface, &parameters, None) })?;
        Ok(format!(
            "Windows accepted the request for {}. Connect to the controller to verify reachability.",
            profile.name
        ))
    }
    pub fn settings() -> Result<String, String> {
        use std::os::windows::process::CommandExt;
        // Fixed OS URI, no shell interpolation or secret arguments.
        std::process::Command::new("explorer.exe")
            .arg("ms-settings:network-wifi")
            .creation_flags(0x08000000)
            .spawn()
            .map_err(|e| format!("Could not open Wi-Fi settings: {e}"))?;
        Ok(
            "Choose or add the network in Windows, then return and connect to the controller."
                .into(),
        )
    }
}
#[cfg(windows)]
pub use native::{join, profiles, settings};
#[cfg(not(windows))]
pub fn profiles() -> Result<Vec<Profile>, String> {
    Err("Use your system Wi-Fi settings to join the board’s network.".into())
}
#[cfg(not(windows))]
pub fn join(_: &Profile) -> Result<String, String> {
    Err("Use your system Wi-Fi settings.".into())
}
#[cfg(not(windows))]
pub fn settings() -> Result<String, String> {
    Err("Open your operating system’s Wi-Fi settings, then reconnect.".into())
}
