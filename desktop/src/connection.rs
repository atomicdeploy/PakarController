//! One selected transport, with no fallback or overlapping physical owners.
//! Adapts PCController Session acquireWrite's checks before/after admission and
//! close-only ownership: revocation cannot retract I/O already handed to the OS.
use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Method {
    #[default]
    WiFi,
    Serial,
}
impl Method {
    pub fn label(self) -> &'static str {
        match self {
            Self::WiFi => "Wi-Fi",
            Self::Serial => "USB",
        }
    }
}
#[derive(Default)]
struct State {
    desired: Option<Method>,
    epoch: u64,
    owner: Option<(Method, u64, usize)>,
}
#[derive(Clone, Default)]
pub struct Gate(Arc<Mutex<State>>);
#[derive(Clone)]
pub struct Stamp {
    gate: Gate,
    pub method: Method,
    pub epoch: u64,
}
pub struct Lease {
    stamp: Stamp,
}
impl Gate {
    pub fn select(&self, desired: Option<Method>) -> u64 {
        let mut state = self.0.lock().expect("connection ownership");
        state.epoch = state.epoch.wrapping_add(1);
        state.desired = desired;
        state.epoch
    }
    pub fn epoch(&self) -> u64 {
        self.0.lock().expect("connection ownership").epoch
    }
    pub fn stamp(&self, method: Method, epoch: u64) -> Stamp {
        Stamp {
            gate: self.clone(),
            method,
            epoch,
        }
    }
    pub fn acquire(&self, method: Method, epoch: u64) -> Option<Lease> {
        let mut state = self.0.lock().expect("connection ownership");
        if state.desired != Some(method) || state.epoch != epoch {
            return None;
        }
        match &mut state.owner {
            None => state.owner = Some((method, epoch, 1)),
            Some((owner, generation, count))
                if *owner == method && *generation == epoch && method == Method::WiFi =>
            {
                *count += 1
            } // HTTP and WS are one Wi-Fi method.
            _ => return None, // A revoked owner must really close before handoff.
        }
        Some(Lease {
            stamp: Stamp {
                gate: self.clone(),
                method,
                epoch,
            },
        })
    }
    pub fn releasing(&self) -> Option<Method> {
        let state = self.0.lock().expect("connection ownership");
        state
            .owner
            .filter(|(method, epoch, _)| state.desired != Some(*method) || state.epoch != *epoch)
            .map(|(method, _, _)| method)
    }
}
impl Stamp {
    pub fn current(&self) -> bool {
        let state = self.gate.0.lock().expect("connection ownership");
        state.desired == Some(self.method) && state.epoch == self.epoch
    }
}
impl Lease {
    pub fn stamp(&self) -> Stamp {
        self.stamp.clone()
    }
    pub fn current(&self) -> bool {
        self.stamp.current()
    }
}
impl Drop for Lease {
    fn drop(&mut self) {
        let mut state = self.stamp.gate.0.lock().expect("connection ownership");
        if let Some((method, epoch, count)) = &mut state.owner {
            if *method == self.stamp.method && *epoch == self.stamp.epoch {
                *count -= 1;
                if *count == 0 {
                    state.owner = None;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn usb_offline_never_admits_network_and_disconnect_revokes_everything() {
        let gate = Gate::default();
        let epoch = gate.select(Some(Method::Serial));
        assert!(gate.acquire(Method::WiFi, epoch).is_none());
        let usb = gate.acquire(Method::Serial, epoch).unwrap();
        assert!(gate.acquire(Method::Serial, epoch).is_none());
        gate.select(None);
        assert!(!usb.current());
        assert!(gate.acquire(Method::WiFi, gate.epoch()).is_none());
        assert!(gate.acquire(Method::Serial, gate.epoch()).is_none());
    }
    #[test]
    fn handoff_waits_for_all_actual_old_resources_and_rejects_stale_epoch() {
        let gate = Gate::default();
        let old = gate.select(Some(Method::WiFi));
        let http = gate.acquire(Method::WiFi, old).unwrap();
        let ws = gate.acquire(Method::WiFi, old).unwrap();
        let next = gate.select(Some(Method::Serial));
        assert!(!http.current() && !ws.current());
        assert_eq!(gate.releasing(), Some(Method::WiFi));
        assert!(gate.acquire(Method::Serial, next).is_none());
        drop(http);
        assert!(gate.acquire(Method::Serial, next).is_none());
        drop(ws);
        let usb = gate.acquire(Method::Serial, next).unwrap();
        assert!(gate.acquire(Method::WiFi, old).is_none());
        let stamp = usb.stamp();
        gate.select(None);
        let renewed = gate.select(Some(Method::Serial));
        assert!(!stamp.current());
        assert!(gate.acquire(Method::Serial, renewed).is_none());
        drop(usb);
        assert!(gate.acquire(Method::Serial, renewed).is_some());
    }
}
