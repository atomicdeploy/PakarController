//! Adapt Pealayer update.rs's pinned journal/helper, exact hash/PE validation,
//! graceful parent exit and startup-health handshake. Preserve rollback; never
//! import its force-kill or broad orphan-deletion behavior. Managed paths only.
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    fs::{self, File, OpenOptions},
    io::{Read, Seek, SeekFrom, Write},
    path::{Path, PathBuf},
    time::{Duration, Instant},
};
const MARKER: &str = ".pakar-managed-installation.json";
const CACHE: &str = ".pakar-client-updates";
#[derive(Clone, Serialize, Deserialize)]
pub struct Managed {
    pub product: String,
    pub owner: String,
    pub executable: PathBuf,
    pub preferences: PathBuf,
}
#[derive(Clone, Serialize, Deserialize)]
struct Journal {
    owner: String,
    parent_pid: u32,
    current: PathBuf,
    current_sha: String,
    candidate: PathBuf,
    candidate_sha: String,
    helper: PathBuf,
    rollback: PathBuf,
    health: PathBuf,
    token: String,
    preferences: PathBuf,
}
pub fn hash(path: &Path) -> Result<String, String> {
    let mut file = File::open(path).map_err(|_| "Cannot read owned package")?;
    let mut digest = Sha256::new();
    let mut data = [0_u8; 65536];
    loop {
        let n = file
            .read(&mut data)
            .map_err(|_| "Cannot hash owned package")?;
        if n == 0 {
            break;
        }
        digest.update(&data[..n]);
    }
    Ok(digest
        .finalize()
        .iter()
        .map(|b| format!("{b:02x}"))
        .collect())
}
pub fn regular(path: &Path) -> Result<(), String> {
    let m = fs::symlink_metadata(path).map_err(|_| "Owned file unavailable")?;
    if !m.is_file() || m.file_type().is_symlink() {
        return Err("Expected an owned regular file, not a link".into());
    }
    #[cfg(windows)]
    {
        use std::os::windows::fs::MetadataExt;
        if m.file_attributes() & 0x400 != 0 {
            return Err("Reparse points are not updater targets".into());
        }
    }
    Ok(())
}
pub fn write_new(path: &Path, value: &impl Serialize) -> Result<(), String> {
    let bytes = serde_json::to_vec(value).map_err(|_| "Cannot encode update record")?;
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(path)
        .map_err(|_| "Owned record already exists or is unavailable")?;
    file.write_all(&bytes)
        .and_then(|_| file.sync_all())
        .map_err(|_| "Cannot persist owned record".into())
}
fn read<T: serde::de::DeserializeOwned>(path: &Path) -> Result<T, String> {
    regular(path)?;
    owner(path)?;
    if fs::metadata(path)
        .map_err(|_| "Cannot inspect owned record")?
        .len()
        > 32768
    {
        return Err("Owned record is too large".into());
    }
    serde_json::from_slice(&fs::read(path).map_err(|_| "Cannot read owned record")?)
        .map_err(|_| "Invalid owned update record".into())
}
pub fn root() -> Result<PathBuf, String> {
    let exe = std::env::current_exe()
        .map_err(|_| "Current executable unavailable")?
        .canonicalize()
        .map_err(|_| "Current executable unavailable")?;
    Ok(exe
        .parent()
        .ok_or("Executable directory unavailable")?
        .to_path_buf())
}
pub fn managed() -> Result<Managed, String> {
    managed_at(&root()?)
}
fn managed_at(root: &Path) -> Result<Managed, String> {
    let m: Managed = read(&root.join(MARKER))?;
    if m.product != "PakarController"
        || uuid::Uuid::parse_str(&m.owner).is_err()
        || m.executable.parent() != Some(root)
        || m.executable.file_name().is_none_or(|n| {
            !n.to_string_lossy()
                .eq_ignore_ascii_case("PakarController.exe")
        })
        || m.executable.canonicalize().ok().as_ref() != Some(&m.executable)
        || m.preferences.canonicalize().ok().as_ref() != Some(&m.preferences)
    {
        return Err("Managed installation identity does not match this package".into());
    }
    regular(&m.executable)?;
    regular(&m.preferences)?;
    owner(&m.executable)?;
    owner(&m.preferences)?;
    owner(&root.join(MARKER))?;
    Ok(m)
}
pub fn cache() -> Result<PathBuf, String> {
    let root = root()?;
    let directory = root.join(CACHE);
    if !directory.exists() {
        fs::create_dir(&directory).map_err(|_| "Cannot create private update staging")?;
    }
    let metadata = fs::symlink_metadata(&directory).map_err(|_| "Private staging unavailable")?;
    if !metadata.is_dir()
        || metadata.file_type().is_symlink()
        || directory.canonicalize().ok().as_ref() != Some(&directory)
    {
        return Err("Private staging must not be a link".into());
    }
    owner(&directory)?;
    Ok(directory)
}
pub fn initialize() -> Result<(), String> {
    let root = root()?;
    let executable = std::env::current_exe()
        .map_err(|_| "Executable unavailable")?
        .canonicalize()
        .map_err(|_| "Executable unavailable")?;
    if executable.file_name().is_none_or(|n| {
        !n.to_string_lossy()
            .eq_ignore_ascii_case("PakarController.exe")
    }) {
        return Err("Only the canonical PakarController.exe can initialize management".into());
    }
    let preferences = crate::config::AppConfig::path()
        .canonicalize()
        .map_err(|_| "Save an existing profile before initializing managed updates")?;
    regular(&executable)?;
    regular(&preferences)?;
    owner(&root)?;
    owner(&executable)?;
    owner(&preferences)?;
    if root.join(MARKER).exists() {
        let old = managed_at(&root)?;
        if old.executable == executable && old.preferences == preferences {
            return Ok(());
        }
        return Err("Managed installation marker belongs to another profile".into());
    }
    write_new(
        &root.join(MARKER),
        &Managed {
            product: "PakarController".into(),
            owner: uuid::Uuid::new_v4().to_string(),
            executable,
            preferences,
        },
    )?;
    cache()?;
    Ok(())
}
pub fn inspect_pe(path: &Path) -> Result<(), String> {
    regular(path)?;
    let mut f = File::open(path).map_err(|_| "Candidate unavailable")?;
    let mut dos = [0_u8; 64];
    f.read_exact(&mut dos)
        .map_err(|_| "Candidate is not a PE executable")?;
    if &dos[..2] != b"MZ" {
        return Err("Candidate is not a Windows executable".into());
    }
    let offset = u32::from_le_bytes(dos[60..64].try_into().unwrap());
    if offset > 1024 * 1024 {
        return Err("Invalid PE header location".into());
    }
    f.seek(SeekFrom::Start(u64::from(offset)))
        .map_err(|_| "Invalid PE header")?;
    let mut pe = [0_u8; 6];
    f.read_exact(&mut pe).map_err(|_| "Invalid PE header")?;
    if &pe[..4] != b"PE\0\0" || u16::from_le_bytes([pe[4], pe[5]]) != 0x8664 {
        return Err("Candidate is not a Windows x64 executable".into());
    }
    Ok(())
}
pub fn prepare(candidate: &Path, sha: &str) -> Result<(), String> {
    let installation = managed()?;
    let directory = cache()?;
    let candidate = candidate
        .canonicalize()
        .map_err(|_| "Candidate unavailable")?;
    let expected_name = format!("{sha}.pending.exe");
    if !crate::maintenance::valid_hash(sha)
        || candidate.parent() != Some(&directory)
        || candidate.file_name().and_then(|n| n.to_str()) != Some(expected_name.as_str())
        || hash(&candidate)? != sha
    {
        return Err("Candidate is not the verified owned staged package".into());
    }
    owner(&candidate)?;
    inspect_pe(&candidate)?;
    let current_sha = hash(&installation.executable)?;
    if current_sha == sha {
        return Err("This exact executable is already active".into());
    }
    let helper = directory.join(format!("{sha}.helper.exe"));
    let journal = directory.join(format!("{sha}.apply.json"));
    let rollback = directory.join(format!("{current_sha}.rollback.exe"));
    if helper.exists() || journal.exists() || rollback.exists() {
        return Err(
            "Preserved transaction/rollback already exists; inspect it before retrying".into(),
        );
    }
    let mut source =
        File::open(&installation.executable).map_err(|_| "Current package unavailable")?;
    let mut target = OpenOptions::new()
        .create_new(true)
        .write(true)
        .open(&helper)
        .map_err(|_| "Cannot create pinned helper")?;
    std::io::copy(&mut source, &mut target)
        .and_then(|_| target.sync_all())
        .map_err(|_| "Cannot preserve pinned helper")?;
    drop(target);
    if hash(&helper)? != current_sha {
        return Err("Helper hash mismatch".into());
    }
    write_new(
        &journal,
        &Journal {
            owner: installation.owner,
            parent_pid: std::process::id(),
            current: installation.executable,
            current_sha,
            candidate,
            candidate_sha: sha.into(),
            helper: helper.clone(),
            rollback,
            health: directory.join(format!("{sha}.health.json")),
            token: uuid::Uuid::new_v4().to_string(),
            preferences: installation.preferences,
        },
    )?;
    let mut process = std::process::Command::new(&helper);
    process.arg("--apply-client-update").arg(journal);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        process.creation_flags(0x08000000);
    }
    process
        .spawn()
        .map_err(|_| "Could not launch the verified update helper")?;
    Ok(())
}
pub fn helper(journal_path: &Path) -> Result<(), String> {
    let j: Journal = read(journal_path)?;
    let directory = j
        .current
        .parent()
        .ok_or("No managed installation directory")?;
    let installation = managed_at(directory)?;
    let cache = directory
        .join(CACHE)
        .canonicalize()
        .map_err(|_| "No owned staging directory")?;
    let own = std::env::current_exe()
        .map_err(|_| "Helper identity unavailable")?
        .canonicalize()
        .map_err(|_| "Helper identity unavailable")?;
    if installation.owner != j.owner
        || installation.executable != j.current
        || installation.preferences != j.preferences
        || own != j.helper
        || journal_path.canonicalize().ok()
            != Some(cache.join(format!("{}.apply.json", j.candidate_sha)))
        || j.candidate != cache.join(format!("{}.pending.exe", j.candidate_sha))
        || j.helper != cache.join(format!("{}.helper.exe", j.candidate_sha))
        || j.rollback != cache.join(format!("{}.rollback.exe", j.current_sha))
        || j.health != cache.join(format!("{}.health.json", j.candidate_sha))
        || j.parent_pid == 0
        || !crate::maintenance::valid_hash(&j.candidate_sha)
        || !crate::maintenance::valid_hash(&j.current_sha)
        || hash(&own)? != j.current_sha
    {
        return Err("Helper journal/package ownership mismatch".into());
    }
    wait_parent(j.parent_pid, &j.current, Duration::from_secs(120))?;
    owner(&j.current)?;
    owner(&j.candidate)?;
    if hash(&j.current)? != j.current_sha
        || hash(&j.candidate)? != j.candidate_sha
        || j.rollback.exists()
    {
        return Err("Package changed or rollback already exists; no replacement performed".into());
    }
    inspect_pe(&j.candidate)?;
    fs::rename(&j.current, &j.rollback).map_err(|_| "Cannot preserve previous executable")?;
    if fs::rename(&j.candidate, &j.current).is_err() {
        let _ = fs::rename(&j.rollback, &j.current);
        return Err("Candidate activation failed; rollback retained/restored".into());
    }
    let mut child = std::process::Command::new(&j.current);
    child
        .env("PAKAR_CONFIG_FILE", &j.preferences)
        .env("PAKAR_CLIENT_HEALTH_JOURNAL", journal_path);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        child.creation_flags(0x08000000);
    }
    let mut child = child
        .spawn()
        .map_err(|_| "Candidate launch failed; rollback remains preserved")?;
    let deadline = Instant::now() + Duration::from_secs(35);
    while Instant::now() < deadline {
        if let Ok(value) = read::<serde_json::Value>(&j.health) {
            if value["token"] == j.token && value["sha256"] == j.candidate_sha {
                return Ok(());
            }
        }
        if child
            .try_wait()
            .map_err(|_| "Candidate process state unavailable")?
            .is_some()
        {
            return Err("Candidate exited before health confirmation; rollback remains preserved for explicit recovery".into());
        }
        std::thread::sleep(Duration::from_millis(100));
    }
    Err("Candidate health is unverified; no process was killed and rollback is preserved".into())
}
pub fn acknowledge_health() -> Result<(), String> {
    let Some(path) = std::env::var_os("PAKAR_CLIENT_HEALTH_JOURNAL") else {
        return Ok(());
    };
    let path = PathBuf::from(path);
    let j: Journal = read(&path)?;
    let installation = managed()?;
    let own = std::env::current_exe()
        .map_err(|_| "Candidate identity unavailable")?
        .canonicalize()
        .map_err(|_| "Candidate identity unavailable")?;
    if own != j.current
        || installation.owner != j.owner
        || installation.preferences != j.preferences
        || hash(&own)? != j.candidate_sha
        || path.parent() != Some(&cache()?)
        || j.health != cache()?.join(format!("{}.health.json", j.candidate_sha))
    {
        return Err("Candidate health ownership mismatch".into());
    }
    if j.health.exists() {
        let old: serde_json::Value = read(&j.health)?;
        return if old["token"] == j.token && old["sha256"] == j.candidate_sha {
            Ok(())
        } else {
            Err("Existing health receipt differs; preserved for inspection".into())
        };
    }
    write_new(
        &j.health,
        &serde_json::json!({"token":j.token,"sha256":j.candidate_sha}),
    )
}
pub fn protected_candidates() -> Result<Vec<PathBuf>, String> {
    let directory = cache()?;
    let mut protected = Vec::new();
    for item in
        fs::read_dir(&directory).map_err(|_| "Cannot inspect pending update transactions")?
    {
        let path = item
            .map_err(|_| "Cannot inspect pending update transaction")?
            .path();
        let Some(name) = path.file_name().and_then(|n| n.to_str()) else {
            continue;
        };
        if !name.ends_with(".apply.json") {
            continue;
        }
        let journal: Journal = read(&path)?;
        // Preserved transactions are never inferred obsolete by cleanup, even
        // after a helper timeout. Explicit recovery must inspect them first.
        protected.push(journal.candidate);
    }
    Ok(protected)
}
#[cfg(windows)]
fn owner(path: &Path) -> Result<(), String> {
    use std::ffi::c_void;
    #[link(name = "advapi32")]
    unsafe extern "system" {
        fn GetNamedSecurityInfoW(
            path: *const u16,
            kind: u32,
            info: u32,
            owner: *mut *mut c_void,
            group: *mut *mut c_void,
            dacl: *mut *mut c_void,
            sacl: *mut *mut c_void,
            sd: *mut *mut c_void,
        ) -> u32;
        fn OpenProcessToken(process: *mut c_void, access: u32, token: *mut *mut c_void) -> i32;
        fn GetTokenInformation(
            token: *mut c_void,
            kind: u32,
            data: *mut c_void,
            length: u32,
            needed: *mut u32,
        ) -> i32;
        fn EqualSid(a: *const c_void, b: *const c_void) -> i32;
    }
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetCurrentProcess() -> *mut c_void;
        fn CloseHandle(handle: *mut c_void) -> i32;
        fn LocalFree(value: *mut c_void) -> *mut c_void;
    }
    use std::os::windows::ffi::OsStrExt;
    let name: Vec<u16> = path.as_os_str().encode_wide().chain(Some(0)).collect();
    let mut owner = std::ptr::null_mut();
    let mut sd = std::ptr::null_mut();
    let mut token = std::ptr::null_mut();
    let result = unsafe {
        GetNamedSecurityInfoW(
            name.as_ptr(),
            1,
            1,
            &mut owner,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            &mut sd,
        )
    };
    if result != 0 {
        return Err("Cannot verify current-user ownership".into());
    }
    let mut data = [0_usize; 128];
    let mut needed = 0;
    let ok = unsafe {
        OpenProcessToken(GetCurrentProcess(), 8, &mut token) != 0
            && GetTokenInformation(
                token,
                1,
                data.as_mut_ptr().cast(),
                std::mem::size_of_val(&data) as u32,
                &mut needed,
            ) != 0
            && EqualSid(owner, data[0] as *const c_void) != 0
    };
    unsafe {
        if !token.is_null() {
            CloseHandle(token);
        }
        LocalFree(sd);
    }
    if ok {
        Ok(())
    } else {
        Err("Updater target is not owned by the current user".into())
    }
}
#[cfg(not(windows))]
fn owner(_: &Path) -> Result<(), String> {
    Err("Managed updates are supported only on Windows".into())
}
#[cfg(windows)]
fn wait_parent(pid: u32, path: &Path, timeout: Duration) -> Result<(), String> {
    use windows::{
        Win32::{
            Foundation::{CloseHandle, WAIT_OBJECT_0},
            System::Threading::{
                OpenProcess, PROCESS_NAME_WIN32, PROCESS_QUERY_LIMITED_INFORMATION,
                PROCESS_SYNCHRONIZE, QueryFullProcessImageNameW, WaitForSingleObject,
            },
        },
        core::PWSTR,
    };
    unsafe {
        let handle = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SYNCHRONIZE,
            false,
            pid,
        )
        .map_err(|_| "Pinned parent process is unavailable")?;
        let mut text = vec![0_u16; 32768];
        let mut length = text.len() as u32;
        let query = QueryFullProcessImageNameW(
            handle,
            PROCESS_NAME_WIN32,
            PWSTR(text.as_mut_ptr()),
            &mut length,
        );
        let matches = query.is_ok()
            && PathBuf::from(String::from_utf16_lossy(&text[..length as usize]))
                .canonicalize()
                .ok()
                .as_deref()
                == Some(path);
        if !matches {
            let _ = CloseHandle(handle);
            return Err("Parent PID belongs to another executable".into());
        }
        let result =
            WaitForSingleObject(handle, timeout.as_millis().min(u128::from(u32::MAX)) as u32);
        let _ = CloseHandle(handle);
        if result == WAIT_OBJECT_0 {
            Ok(())
        } else {
            Err("Application did not finish graceful shutdown; no replacement performed".into())
        }
    }
}
#[cfg(not(windows))]
fn wait_parent(_: u32, _: &Path, _: Duration) -> Result<(), String> {
    Err("Managed updates require Windows".into())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn managed_records_keep_package_and_profile_explicit_without_secret_material() {
        let m = Managed {
            product: "PakarController".into(),
            owner: uuid::Uuid::new_v4().to_string(),
            executable: "PakarController.exe".into(),
            preferences: "preferences.json".into(),
        };
        let value = serde_json::to_value(&m).unwrap();
        assert_eq!(value["product"], "PakarController");
        assert!(value.get("token").is_none());
        assert!(value.get("password").is_none());
        let decoded: Managed = serde_json::from_value(value).unwrap();
        assert_eq!(decoded.preferences, m.preferences);
    }
    #[test]
    fn journals_bind_all_transaction_paths_and_parent_without_shell_commands() {
        let j = Journal {
            owner: uuid::Uuid::new_v4().to_string(),
            parent_pid: 123,
            current: "PakarController.exe".into(),
            current_sha: "a".repeat(64),
            candidate: "b.pending.exe".into(),
            candidate_sha: "b".repeat(64),
            helper: "b.helper.exe".into(),
            rollback: "a.rollback.exe".into(),
            health: "b.health.json".into(),
            token: uuid::Uuid::new_v4().to_string(),
            preferences: "preferences.json".into(),
        };
        let value = serde_json::to_value(j).unwrap();
        assert_eq!(value["parent_pid"], 123);
        assert!(value.get("command").is_none());
        assert!(value.get("arguments").is_none());
    }
}
