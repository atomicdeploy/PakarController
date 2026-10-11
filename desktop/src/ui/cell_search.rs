//! Logical cell lookup only: searching never changes the physical LED state.
use std::collections::BTreeSet;

/// Exact selection, compacted into sorted ranges rather than the last cursor.
pub fn format_selection(ids: &BTreeSet<u8>) -> String {
    let mut parts = Vec::new();
    let mut cells = ids.iter().copied().peekable();
    while let Some(start) = cells.next() {
        let mut end = start;
        while cells.peek().is_some_and(|next| *next == end + 1) {
            end = cells.next().unwrap();
        }
        parts.push(if start == end {
            start.to_string()
        } else {
            format!("{start}–{end}")
        });
    }
    parts.join(", ")
}

/// A genuine selection gesture replaces the query; a partial draft stays editable.
pub fn synchronize_selection(
    ids: &BTreeSet<u8>,
    revision: u64,
    reflected: &mut u64,
    query: &mut String,
    active: &mut u8,
) -> bool {
    if revision == *reflected {
        return false;
    }
    *query = format_selection(ids);
    *reflected = revision;
    if !ids.contains(active)
        && let Some(first) = ids.first()
    {
        *active = *first;
    }
    true
}

pub fn parse(query: &str) -> Result<Vec<u8>, String> {
    if query.len() > 256 {
        return Err("Search is too long".into());
    }
    let normalized = super::digits::latin(query)
        .replace(['–', '—'], "-")
        .replace('،', ",");
    let mut cells = BTreeSet::new();
    for token in normalized.split(|c: char| c == ',' || c == ';' || c.is_whitespace()) {
        if token.is_empty() {
            continue;
        }
        let number = |value: &str| -> Result<u8, String> {
            let value = value.strip_prefix('#').unwrap_or(value);
            if value.is_empty() || !value.bytes().all(|byte| byte.is_ascii_digit()) {
                return Err(
                    "Use cell IDs 1–80, separated by commas, or ranges such as 20–28".into(),
                );
            }
            value
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
        assert_eq!(parse("۱،٢;۳–۵ #۸۰").unwrap(), [1, 2, 3, 4, 5, 80]);
        assert_eq!(
            format_selection(&parse("۱،٢,۳").unwrap().into_iter().collect()),
            "1–3"
        );
    }
    #[test]
    fn invalid_input_never_silently_matches_a_different_cell() {
        for query in [
            "0", "81", "256", "4-2", "2-", "-4", "a", "3,,bad", ",", "##3", "+3",
        ] {
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
    #[test]
    fn selection_query_roundtrips_every_cell_and_noncontiguous_ranges() {
        for ids in [
            BTreeSet::new(),
            BTreeSet::from([1]),
            BTreeSet::from([2, 3, 4, 12, 80]),
            (1..=80).collect(),
        ] {
            assert_eq!(
                parse(&format_selection(&ids))
                    .unwrap()
                    .into_iter()
                    .collect::<BTreeSet<_>>(),
                ids
            );
        }
        assert_eq!(
            format_selection(&BTreeSet::from([2, 3, 4, 12, 80])),
            "2–4, 12, 80"
        );
    }
    #[test]
    fn real_selection_replaces_stale_query_but_draft_and_navigation_do_not() {
        let mut reflected = 0;
        let (mut query, mut active) = ("12-".into(), 12);
        assert!(!synchronize_selection(
            &BTreeSet::new(),
            0,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert_eq!(query, "12-");
        let ids = BTreeSet::from([3, 4, 5, 12]);
        assert!(synchronize_selection(
            &ids,
            1,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert_eq!(query, "3–5, 12");
        active = 5;
        assert!(!synchronize_selection(
            &ids,
            1,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert!(synchronize_selection(
            &BTreeSet::from([80]),
            2,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert_eq!((query.as_str(), active), ("80", 80));
        assert!(synchronize_selection(
            &BTreeSet::new(),
            3,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert_eq!(query, "");
        query = "invalid draft".into();
        assert!(synchronize_selection(
            &BTreeSet::new(),
            4,
            &mut reflected,
            &mut query,
            &mut active
        ));
        assert_eq!(
            query, "",
            "Clear must work even when the selection was already empty"
        );
    }
}
