//! Operational client metadata only; no credentials or document/file inventory.
use std::time::Instant;
use network_interface::NetworkInterfaceConfig;
pub struct Reporter {
    id: String,
    started: Instant,
    last: Instant,
}
impl Reporter {
    pub fn new() -> Self {
        Self {
            id: crate::peer_identity::session_id().to_owned(),
            started: crate::peer_identity::process_started(),
            last: Instant::now() - std::time::Duration::from_secs(30),
        }
    }
    pub fn due(&self) -> bool {
        self.last.elapsed().as_secs() >= 30
    }
    pub fn websocket_introduction(&mut self)->Option<String>{
        if !self.due(){return None;}
        let packet=serde_json::json!({"type":"peer-introduction","report":self.value_for_transport("websocket")}).to_string();
        self.last=Instant::now();
        // Firmware's bounded existing WS text ingress remains unchanged.
        (packet.len()<=1300).then_some(packet)
    }
    pub fn report(&mut self, client: &reqwest::blocking::Client, endpoint: &crate::Endpoint) {
        self.last = Instant::now();
        let value = self.value();
        // Opportunistic, bounded, no retry: reporting cannot stop board control.
        let _ = client
            .post(format!(
                "{}/api/clients/report",
                endpoint.base.trim_end_matches('/')
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .timeout(std::time::Duration::from_secs(2))
            .form(&[("report", value.to_string())])
            .send();
    }
    pub fn value(&self) -> serde_json::Value {
        self.value_for_transport("http")
    }
    pub fn value_for_transport(&self, transport: &str) -> serde_json::Value {
        let mut d = serde_json::json!({"id":self.id,"os":std::env::consts::OS,
            "architecture":std::env::consts::ARCH,"application":"PakarController","version":env!("CARGO_PKG_VERSION"),
            "session_id":crate::peer_identity::session_id(),"transport":transport,"process_id":std::process::id(),"uptime_seconds":self.started.elapsed().as_secs().min(u32::MAX as u64),
            "capabilities":["cell-batch","pixel-diagnostics","firmware-transfer","board-mirroring","peer-bridge","websocket","usb"],
            "settings":{"transport":transport,"telemetry_enabled":true,"version_check":crate::peer_identity::enforced()},
            "hardware":{"cpu_count":std::thread::available_parallelism().map_or(1,|v|v.get())}});
        if let Some(settings)=crate::peer_identity::reported_settings().as_object(){for(key,value)in settings{d["settings"][key]=value.clone();}}
        d["build"]=crate::peer_identity::build_identity().into();
        if let Ok(name) = std::env::var("COMPUTERNAME").or_else(|_| std::env::var("HOSTNAME")) {
            if name.len() <= 64 && name.bytes().all(|b| (32..=126).contains(&b)) {
                d["hostname"] = name.into();
            }
        }
        if let Ok(count) = std::thread::available_parallelism() {
            d["cpu_count"] = (count.get() as u32).into();
        }
        if let Some(cpu)=std::env::var("PROCESSOR_IDENTIFIER").ok().filter(|v|v.len()<=96&&v.bytes().all(|b|(32..=126).contains(&b))) {d["hardware"]["cpu"]=cpu.into();}
        if let Ok(interfaces)=network_interface::NetworkInterface::show() {
            if let Some(ip)=interfaces.iter().flat_map(|v|&v.addr).map(|a|a.ip()).find(|ip|ip.is_ipv4()&&!ip.is_loopback()&&!ip.is_unspecified()&&!ip.is_multicast()) {d["reported_ip"]=ip.to_string().into();}
        }
        #[cfg(windows)]
        unsafe {
            use windows::Win32::{System::SystemInformation::{GlobalMemoryStatusEx,MEMORYSTATUSEX},
                UI::WindowsAndMessaging::{GetSystemMetrics,SM_CXSCREEN,SM_CYSCREEN,SM_CMONITORS}};
            let mut memory=MEMORYSTATUSEX{dwLength:std::mem::size_of::<MEMORYSTATUSEX>() as u32,..Default::default()};
            if GlobalMemoryStatusEx(&mut memory).is_ok() {
                d["memory_mb"]=((memory.ullTotalPhys/(1024*1024)) as u32).into();
                d["hardware"]["memory_mb"]=d["memory_mb"].clone();
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
        assert_eq!(v["id"], b.value()["id"]); // One actual process peer across all routes.
        assert_eq!(v["session_id"],crate::peer_identity::session_id());
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
