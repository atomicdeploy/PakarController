//! Edge-triggered presentation only. Commands and game decisions stay on ESP.
#[derive(Default)]
pub struct WinnerNotice {
    peer: Option<(String, String)>,
    previous: u8,
    round: u64,
}
impl WinnerNotice {
    pub fn observe(&mut self, state: Option<(&str, &str, &str, u8)>) -> Option<(u8, u64)> {
        let Some((board, boot, role, winner)) = state else {
            return None;
        };
        if role != "first-wins" || board.is_empty() || boot.is_empty() || winner > 9 {
            return None;
        }
        let peer = (board.to_owned(), boot.to_owned());
        if self.peer.as_ref() != Some(&peer) {
            self.peer = Some(peer);
            self.previous = winner;
            return None;
        }
        let previous = self.previous;
        self.previous = winner;
        if previous == 0 && winner != 0 {
            self.round += 1;
            Some((winner, self.round))
        } else {
            None
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn wins_not_snapshots_or_reconnects_notify() {
        let mut n = WinnerNotice::default();
        assert_eq!(n.observe(Some(("a", "boot", "first-wins", 0))), None);
        assert_eq!(
            n.observe(Some(("a", "boot", "first-wins", 2))),
            Some((2, 1))
        );
        assert_eq!(n.observe(None), None);
        assert_eq!(n.observe(Some(("a", "boot", "first-wins", 2))), None);
        assert_eq!(n.observe(Some(("a", "new", "first-wins", 2))), None);
        assert_eq!(n.observe(Some(("a", "new", "first-wins", 0))), None);
        assert_eq!(n.observe(Some(("a", "new", "first-wins", 2))), Some((2, 2)));
        assert_eq!(n.observe(Some(("a", "new", "table-selection", 1))), None);
    }
}
