//! Imported Pealayer Windows Jump List publisher, using controller actions.
#![cfg(windows)]
use super::windows_quick_actions::WINDOWS_QUICK_ACTIONS;
fn wide_null(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(Some(0)).collect()
}
fn wide_null_path(path: &std::path::Path) -> Vec<u16> {
    use std::os::windows::ffi::OsStrExt;
    path.as_os_str().encode_wide().chain(Some(0)).collect()
}
pub fn install() -> Result<(), String> {
    use windows::{Win32::UI::Shell::SetCurrentProcessExplicitAppUserModelID, core::PCWSTR};
    let id = wide_null(crate::application_identity::WINDOWS_APP_ID);
    unsafe { SetCurrentProcessExplicitAppUserModelID(PCWSTR(id.as_ptr())) }
        .map_err(|e| e.to_string())?;
    build_windows_jump_list(true)
}
#[cfg(target_os = "windows")]
fn build_windows_jump_list(include_quick_actions: bool) -> Result<(), String> {
    use windows::Win32::Foundation::PROPERTYKEY;
    use windows::Win32::System::Com::StructuredStorage::PROPVARIANT;
    use windows::Win32::System::Com::{
        CLSCTX_INPROC_SERVER, CoCreateInstance,
    };
    use windows::Win32::UI::Shell::Common::{IObjectArray, IObjectCollection};
    use windows::Win32::UI::Shell::PropertiesSystem::IPropertyStore;
    use windows::Win32::UI::Shell::{
        DestinationList, EnumerableObjectCollection, ICustomDestinationList, IShellLinkW, ShellLink,
    };
    use windows::core::{GUID, Interface, PCWSTR};

    let executable = std::env::current_exe()
        .map_err(|error| format!("resolve executable for Windows quick actions: {error}"))?;
    let executable_wide = wide_null_path(&executable);
    let working_directory_wide =
        wide_null_path(executable.parent().ok_or_else(|| {
            "Windows quick-action executable has no containing directory".to_owned()
        })?);
    const PKEY_TITLE: PROPERTYKEY = PROPERTYKEY {
        fmtid: GUID::from_u128(0xf29f85e0_4ff9_1068_ab91_08002b27b3d9),
        pid: 2,
    };

    unsafe {
        let _apartment = super::windows_desktop::Apartment::new()?;
        let destination_list: ICustomDestinationList =
            CoCreateInstance(&DestinationList, None, CLSCTX_INPROC_SERVER)
                .map_err(|error| format!("create Windows Jump List: {error}"))?;
        let app_id = wide_null(crate::application_identity::WINDOWS_APP_ID);
        destination_list
            .SetAppID(PCWSTR(app_id.as_ptr()))
            .map_err(|e| e.to_string())?;
        let mut minimum_slots = 0;
        let _: IObjectArray = destination_list
            .BeginList(&mut minimum_slots)
            .map_err(|error| format!("begin Windows Jump List: {error}"))?;

        if include_quick_actions {
            let collection: IObjectCollection =
                CoCreateInstance(&EnumerableObjectCollection, None, CLSCTX_INPROC_SERVER)
                    .map_err(|error| format!("create Windows quick-action collection: {error}"))?;

            for action in WINDOWS_QUICK_ACTIONS {
                let link: IShellLinkW = CoCreateInstance(&ShellLink, None, CLSCTX_INPROC_SERVER)
                    .map_err(|error| format!("create Windows quick action: {error}"))?;
                let arguments = action
                    .arguments
                    .encode_utf16()
                    .chain(std::iter::once(0))
                    .collect::<Vec<_>>();
                let description = action
                    .title
                    .encode_utf16()
                    .chain(std::iter::once(0))
                    .collect::<Vec<_>>();
                link.SetPath(PCWSTR(executable_wide.as_ptr()))
                    .map_err(|error| format!("set Windows quick-action executable: {error}"))?;
                link.SetArguments(PCWSTR(arguments.as_ptr()))
                    .map_err(|error| format!("set Windows quick-action arguments: {error}"))?;
                link.SetWorkingDirectory(PCWSTR(working_directory_wide.as_ptr()))
                    .map_err(|error| {
                        format!("set Windows quick-action working directory: {error}")
                    })?;
                link.SetDescription(PCWSTR(description.as_ptr()))
                    .map_err(|error| format!("set Windows quick-action description: {error}"))?;
                link.SetIconLocation(
                    PCWSTR(executable_wide.as_ptr()),
                    action.icon_location_index(),
                )
                .map_err(|error| format!("set Windows quick-action icon: {error}"))?;

                let properties: IPropertyStore = link
                    .cast()
                    .map_err(|error| format!("open Windows quick-action properties: {error}"))?;
                let title = PROPVARIANT::from(action.title);
                properties
                    .SetValue(&PKEY_TITLE, &title)
                    .map_err(|error| format!("set Windows quick-action title: {error}"))?;
                properties
                    .Commit()
                    .map_err(|error| format!("commit Windows quick-action title: {error}"))?;
                collection
                    .AddObject(&link)
                    .map_err(|error| format!("append Windows quick action: {error}"))?;
            }
            let tasks: IObjectArray = collection
                .cast()
                .map_err(|error| format!("finalize Windows quick-action collection: {error}"))?;
            destination_list
                .AddUserTasks(&tasks)
                .map_err(|error| format!("publish Windows quick actions: {error}"))?;
        }

        destination_list
            .CommitList()
            .map_err(|error| format!("commit Windows Jump List: {error}"))?;
    }
    Ok(())
}
