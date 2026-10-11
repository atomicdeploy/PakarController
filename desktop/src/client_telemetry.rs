//! Operational client metadata only; no credentials or document/file inventory.
use std::time::Instant;
pub struct Reporter {
    id: String,
    started: Instant,
    last: Instant,
}
impl Reporter {
    pub fn new() -> Self {
        Self {
            id: uuid::Uuid::new_v4().to_string(),
            started: Instant::now(),
            last: Instant::now() - std::time::Duration::from_secs(30),
        }
    }
    pub fn due(&self) -> bool {
        self.last.elapsed().as_secs() >= 30
    }
    pub fn report(&mut self, client: &reqwest::blocking::Client, endpoint: &crate::Endpoint) {
        self.last = Instant::now();
        let value = self.value();
        // Opportunistic, bounded, no retry: reporting cannot stop board control.
        let _ = client
            .post(format!(
                "{}/api/v1/clients/report",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .timeout(std::time::Duration::from_secs(2))
            .form(&[("report", value.to_string())])
            .send();
    }
    pub fn value(&self) -> serde_json::Value {
        let mut d = serde_json::json!({"schema":1,"id":self.id,"os":std::env::consts::OS,
            "architecture":std::env::consts::ARCH,"application":"PakarController","version":env!("CARGO_PKG_VERSION"),
            "transport":"http","process_id":std::process::id(),"uptime_seconds":self.started.elapsed().as_secs().min(u32::MAX as u64)});
        if let Ok(name) = std::env::var("COMPUTERNAME").or_else(|_| std::env::var("HOSTNAME")) {
            if name.len() <= 64 && name.bytes().all(|b| (32..=126).contains(&b)) {
                d["hostname"] = name.into();
            }
        }
        if let Ok(count) = std::thread::available_parallelism() {
            d["cpu_count"] = (count.get() as u32).into();
        }
        #[cfg(windows)]
        unsafe {
            use windows::Win32::{System::SystemInformation::{GlobalMemoryStatusEx,MEMORYSTATUSEX},
                UI::WindowsAndMessaging::{GetSystemMetrics,SM_CXSCREEN,SM_CYSCREEN,SM_CMONITORS}};
            let mut memory=MEMORYSTATUSEX{dwLength:std::mem::size_of::<MEMORYSTATUSEX>() as u32,..Default::default()};
            if GlobalMemoryStatusEx(&mut memory).is_ok() {
                d["memory_mb"]=((memory.ullTotalPhys/(1024*1024)) as u32).into();
                d["memory_available_mb"]=((memory.ullAvailPhys/(1024*1024)) as u32).into();
            }
            for (key,metric) in [("screen_width",SM_CXSCREEN),("screen_height",SM_CYSCREEN),("monitor_count",SM_CMONITORS)] {
                let value=GetSystemMetrics(metric);if value>0 {d[key]=(value as u32).into();}
            }
        }
        d
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reports_real_identity_without_secrets() {
        let a = Reporter::new();
        let b = Reporter::new();
        let v = a.value();
        assert_eq!(v["version"], env!("CARGO_PKG_VERSION"));
        assert_eq!(v["process_id"], std::process::id());
        assert_ne!(v["id"], b.value()["id"]);
        assert!(v.get("password").is_none());
        assert!(v.get("files").is_none());
        #[cfg(windows)]
        {
            assert!(v["memory_mb"].as_u64().unwrap()>0);
            assert!(v["memory_available_mb"].as_u64().unwrap()<=v["memory_mb"].as_u64().unwrap());
            assert!(v["monitor_count"].as_u64().unwrap()>0);
        }
    }
}
