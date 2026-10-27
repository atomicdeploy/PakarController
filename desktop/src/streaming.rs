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

/// A screen sample belongs to the current board's physical topology, not a
/// fixed six-pixel stride or a logical cell's position in the map array.
pub fn sampling_layout(status: &crate::DeviceStatus) -> Option<crate::pixel_groups::Layout> {
    crate::pixel_groups::for_status(status)
        .filter(|layout| layout.capacity == 480 && layout.group_count == 80)
}

pub fn sample_pixels(
    width: u32,
    height: u32,
    mapping: &[crate::CellMapping],
    layout: &crate::pixel_groups::Layout,
    sample: impl Fn(u32, u32) -> [u8; 3],
) -> Result<Vec<u8>, String> {
    layout.validate()?;
    if layout.capacity != 480 || layout.group_count != 80 {
        return Err("Screen sampling requires the table board's physical group layout".into());
    }
    let mut pixels = vec![0; usize::from(layout.capacity) * 3];
    for (physical_index, x, y) in sample_positions(width, height, mapping)? {
        let (start, count) = layout.span((physical_index + 1) as u8)
            .ok_or("Physical group missing from sampled layout")?;
        let color = sample(x, y);
        for pixel in usize::from(start)..usize::from(start) + usize::from(count) {
            pixels[pixel * 3..pixel * 3 + 3].copy_from_slice(&color);
        }
    }
    // The preallocated unused capacity remains black, including a shortened
    // final group. Neither screen capture nor a previous frame can fill it.
    Ok(pixels)
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
    fn pixel_groups_sampler_uses_physical_spans_and_blacks_unused_tail() {
        let mut cells = mapping();
        cells[0].physical_cell = 80;
        cells[79].physical_cell = 1;
        let mut layout = crate::pixel_groups::Layout::uniform(480, 6).unwrap();
        layout.revision = 7;
        layout.set_override(1, 5);
        layout.counts[0] = 5;
        for start in &mut layout.starts[1..] { *start -= 1; }
        layout.used_pixels = 479;
        let pixels = sample_pixels(200, 170, &cells, &layout, |x, y| [x as u8, y as u8, 42]).unwrap();
        assert!(pixels[..15].chunks_exact(3).all(|rgb| rgb == [190, 150, 42]), "Logical80 follows physical1's five-pixel override");
        assert!(pixels[473 * 3..479 * 3].chunks_exact(3).all(|rgb| rgb == [10, 20, 42]), "Logical1 follows physical80's actual start");
        assert_eq!(&pixels[479 * 3..], &[0, 0, 0]);
        layout.starts[1] = 6;
        assert!(sample_pixels(200, 170, &cells, &layout, |_, _| [255; 3]).is_err());
        let mut status: crate::DeviceStatus = serde_json::from_value(serde_json::json!({"pixels":480,
            "capabilities":{"board_kind":"table","role":"table-selection","acknowledged_features":[crate::pixel_groups::FEATURE]}})).unwrap();
        assert!(sampling_layout(&status).is_none(), "Advertised missing layout must not fall back to six");
        status.pixel_groups = serde_json::to_value(layout).unwrap();
        assert!(sampling_layout(&status).is_none(), "Malformed advertised topology must not be guessed");
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
