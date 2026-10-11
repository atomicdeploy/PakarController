//! One pacing policy for generated and sampled frames. No catch-up burst queue.
use std::time::Duration;

pub const DEFAULT_FPS: u16 = 60;
pub const MAX_FPS: u16 = 240;

pub fn valid_mapping(mapping: &[crate::CellMapping]) -> bool {
    if mapping.len() != 80 {
        return false;
    }
    let mut logical = [false; 80];
    let mut physical = [false; 80];
    for cell in mapping {
        if !(1..=80).contains(&cell.logical_id) || !(1..=80).contains(&cell.physical_cell) {
            return false;
        }
        let l = usize::from(cell.logical_id - 1);
        let p = usize::from(cell.physical_cell - 1);
        if logical[l] || physical[p] {
            return false;
        }
        logical[l] = true;
        physical[p] = true;
    }
    true
}

pub fn sample_positions(
    width: u32,
    height: u32,
    mapping: &[crate::CellMapping],
) -> Result<Vec<(usize, u32, u32)>, String> {
    if width == 0 || height == 0 || !valid_mapping(mapping) {
        return Err(
            "Sampling requires a nonempty screen and the board's complete validated cell mapping"
                .into(),
        );
    }
    Ok(mapping
        .iter()
        .map(|cell| {
            let logical = u32::from(cell.logical_id - 1);
            let col = logical % 10;
            let row = logical / 10;
            let x =
                (u64::from(col * 2 + 1) * u64::from(width) / 20).min(u64::from(width - 1)) as u32;
            // One-based odd columns have a half-row offset. Seventeen half-rows
            // cover the eight rows plus their stagger, without a legacy +1 shift.
            let half_row = row * 2 + 1 + u32::from(col % 2 == 0);
            let y =
                (u64::from(half_row) * u64::from(height) / 17).min(u64::from(height - 1)) as u32;
            (usize::from(cell.physical_cell - 1), x, y)
        })
        .collect())
}

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
    fn mapping() -> Vec<crate::CellMapping> {
        (1..=80)
            .map(|id| crate::CellMapping {
                logical_id: id,
                physical_cell: id,
                name: id.to_string(),
            })
            .collect()
    }
    #[test]
    fn sampling_uses_reported_physical_map_and_half_stagger() {
        let mut cells = mapping();
        cells[0].physical_cell = 80;
        cells[79].physical_cell = 1;
        let positions = sample_positions(200, 170, &cells).unwrap();
        assert_eq!(positions[0], (79, 10, 20));
        assert_eq!(positions[1], (1, 30, 10));
        assert_eq!(positions[10], (10, 10, 40));
        assert_eq!(positions[79], (0, 190, 150));
        assert!(positions.iter().all(|(_, x, y)| *x < 200 && *y < 170));
        assert!(
            sample_positions(1, 1, &cells)
                .unwrap()
                .iter()
                .all(|(_, x, y)| *x == 0 && *y == 0)
        );
    }
    #[test]
    fn sampling_rejects_missing_duplicate_or_invalid_board_maps() {
        let mut cells = mapping();
        assert!(!valid_mapping(&cells[..79]));
        cells[0].physical_cell = 0;
        assert!(!valid_mapping(&cells));
        cells[0].physical_cell = 2;
        assert!(!valid_mapping(&cells));
        cells[0].physical_cell = 1;
        cells[0].logical_id = 2;
        assert!(!valid_mapping(&cells));
        assert!(sample_positions(0, 170, &mapping()).is_err());
    }
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
