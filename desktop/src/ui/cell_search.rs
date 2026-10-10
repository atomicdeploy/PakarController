//! Logical cell lookup only: searching never changes the physical LED state.
use std::collections::BTreeSet;

pub fn parse(query: &str) -> Result<Vec<u8>, String> {
    if query.len() > 256 {
        return Err("Search is too long".into());
    }
    let normalized = query.replace(['–', '—'], "-");
    let mut cells = BTreeSet::new();
    for token in normalized.split(|c: char| c == ',' || c == ';' || c.is_whitespace()) {
        if token.is_empty() {
            continue;
        }
        let number = |value: &str| -> Result<u8, String> {
            value
                .trim_start_matches('#')
                .parse::<u8>()
                .ok()
                .filter(|id| (1..=80).contains(id))
                .ok_or_else(|| {
                    "Use cell IDs 1–80, separated by commas, or ranges such as 20–28".into()
                })
        };
        if let Some((start, end)) = token.split_once('-') {
            let (start, end) = (number(start)?, number(end)?);
            if start > end {
                return Err("Range start must not exceed its end".into());
            }
            cells.extend(start..=end);
        } else {
            cells.insert(number(token)?);
        }
    }
    if !query.trim().is_empty() && cells.is_empty() {
        return Err("Enter a cell ID or range".into());
    }
    Ok(cells.into_iter().collect())
}

pub fn navigate(matches: &[u8], current: u8, backwards: bool) -> Option<u8> {
    if matches.is_empty() {
        return None;
    }
    Some(match matches.iter().position(|id| *id == current) {
        Some(index) if backwards => matches[(index + matches.len() - 1) % matches.len()],
        Some(index) => matches[(index + 1) % matches.len()],
        None if backwards => *matches.last().unwrap(),
        None => matches[0],
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn exact_numbers_lists_ranges_and_duplicates() {
        assert_eq!(parse("1").unwrap(), [1]); // not 11, 21, or 71
        assert_eq!(parse("#3, 12;20–23 21").unwrap(), [3, 12, 20, 21, 22, 23]);
        assert_eq!(parse("01, 80").unwrap(), [1, 80]);
        assert!(parse("  ").unwrap().is_empty());
    }
    #[test]
    fn invalid_input_never_silently_matches_a_different_cell() {
        for query in ["0", "81", "256", "4-2", "2-", "-4", "a", "3,,bad", ","] {
            assert!(parse(query).is_err(), "{query}");
        }
        assert!(parse(&"1".repeat(257)).is_err());
    }
    #[test]
    fn navigation_wraps_in_both_directions() {
        assert_eq!(navigate(&[3, 12, 80], 80, false), Some(3));
        assert_eq!(navigate(&[3, 12, 80], 3, true), Some(80));
        assert_eq!(navigate(&[3, 12, 80], 7, false), Some(3));
        assert_eq!(navigate(&[], 1, false), None);
    }
}
