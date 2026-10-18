//! Stable additive wire IDs. Only peer-advertised entries are exposed in UI.
pub const CUES: [(&str, &str); 15] = [
    ("welcome", "Welcome · Puzzles"),
    ("update-start", "Update start"),
    ("update-success", "Update complete"),
    ("error", "Error"),
    ("winner", "Winner"),
    ("finish", "Finish"),
    ("lost", "Lost"),
    ("incorrect", "Incorrect"),
    ("rapid-error", "Rapid error"),
    ("fault", "Fault"),
    ("increase", "Increase"),
    ("decrease", "Decrease"),
    ("welcome-puzzleboard", "Welcome · PuzzleBoard"),
    ("welcome-40led", "Welcome · 40LED"),
    ("attention", "Attention"),
];
pub fn id(name: &str) -> Option<u8> {
    CUES.iter()
        .position(|(cue, _)| *cue == name)
        .map(|i| i as u8)
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn catalog_ids_are_stable_and_unknown_is_not_welcome() {
        for (i, (name, _)) in CUES.iter().enumerate() {
            assert_eq!(id(name), Some(i as u8));
        }
        assert_eq!(id("WELCOME"), None);
        assert_eq!(id(""), None);
        assert_eq!(id("anything"), None);
        assert_eq!(id("decrease"), Some(11));
        assert_eq!(id("welcome-40led"), Some(13));
        assert_eq!(id("attention"), Some(14));
    }
}
