//! One pacing policy for generated and sampled frames. No catch-up burst queue.
use std::time::Duration;

pub const DEFAULT_FPS: u16 = 60;
pub const MAX_FPS: u16 = 240;

pub fn interval(fps: u16) -> Duration {
    if fps == 0 {
        Duration::ZERO
    } else {
        Duration::from_nanos(1_000_000_000 / u64::from(fps.min(MAX_FPS)))
    }
}

pub fn read_budget(active: bool, fps: u16, elapsed: Duration) -> Duration {
    if !active {
        return Duration::from_millis(40);
    }
    // TCP timeout cannot be zero. The 1 ms floor is a responsive receive
    // opportunity, not a 20/25 FPS animation throttle. Writes/capture provide
    // natural backpressure in uncapped mode; no pending frames accumulate.
    interval(fps)
        .saturating_sub(elapsed)
        .max(Duration::from_millis(1))
        .min(Duration::from_millis(40))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn both_sources_use_configurable_deadlines_not_fixed_caps() {
        assert_eq!(interval(0), Duration::ZERO);
        assert_eq!(interval(60), Duration::from_nanos(16_666_666));
        assert_eq!(interval(120), Duration::from_nanos(8_333_333));
        assert_eq!(interval(65_535), interval(MAX_FPS));
        assert_eq!(
            read_budget(true, 60, Duration::from_millis(10)),
            Duration::from_nanos(6_666_666)
        );
        assert_eq!(
            read_budget(true, 0, Duration::ZERO),
            Duration::from_millis(1)
        );
        assert_eq!(
            read_budget(true, 60, Duration::from_secs(5)),
            Duration::from_millis(1)
        );
        assert_eq!(
            read_budget(false, 0, Duration::ZERO),
            Duration::from_millis(40)
        );
    }
}
