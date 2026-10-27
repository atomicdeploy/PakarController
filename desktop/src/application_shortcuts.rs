//! Persisted in-application accelerators adapted from Pealayer's
//! `src/application_shortcuts.rs` and physical-key matching contract.
//!
//! PakarController deliberately keeps these separate from board keys and RF
//! bindings: application accelerators only act while this window receives the
//! corresponding egui event.

use eframe::egui;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct ApplicationShortcuts {
    pub fullscreen: String,
    pub refresh: String,
    pub preferences: String,
    pub device_settings: String,
    pub connect: String,
    pub cell_mapping: String,
}

impl Default for ApplicationShortcuts {
    fn default() -> Self {
        let command = if cfg!(target_os = "macos") {
            "Cmd"
        } else {
            "Ctrl"
        };
        Self {
            fullscreen: "F11".into(),
            refresh: "F5".into(),
            preferences: format!("{command}+,"),
            device_settings: format!("{command}+Shift+,"),
            connect: format!("{command}+K"),
            cell_mapping: format!("{command}+M"),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ApplicationAction {
    Fullscreen,
    Refresh,
    Preferences,
    DeviceSettings,
    Connect,
    CellMapping,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct KeyChord {
    key: egui::Key,
    control: bool,
    alt: bool,
    shift: bool,
    super_key: bool,
}

fn key_code(key: egui::Key) -> Option<&'static str> {
    use egui::Key;
    Some(match key {
        Key::A => "KeyA",
        Key::B => "KeyB",
        Key::C => "KeyC",
        Key::D => "KeyD",
        Key::E => "KeyE",
        Key::F => "KeyF",
        Key::G => "KeyG",
        Key::H => "KeyH",
        Key::I => "KeyI",
        Key::J => "KeyJ",
        Key::K => "KeyK",
        Key::L => "KeyL",
        Key::M => "KeyM",
        Key::N => "KeyN",
        Key::O => "KeyO",
        Key::P => "KeyP",
        Key::Q => "KeyQ",
        Key::R => "KeyR",
        Key::S => "KeyS",
        Key::T => "KeyT",
        Key::U => "KeyU",
        Key::V => "KeyV",
        Key::W => "KeyW",
        Key::X => "KeyX",
        Key::Y => "KeyY",
        Key::Z => "KeyZ",
        Key::Num0 => "Digit0",
        Key::Num1 => "Digit1",
        Key::Num2 => "Digit2",
        Key::Num3 => "Digit3",
        Key::Num4 => "Digit4",
        Key::Num5 => "Digit5",
        Key::Num6 => "Digit6",
        Key::Num7 => "Digit7",
        Key::Num8 => "Digit8",
        Key::Num9 => "Digit9",
        Key::F1 => "F1",
        Key::F2 => "F2",
        Key::F3 => "F3",
        Key::F4 => "F4",
        Key::F5 => "F5",
        Key::F6 => "F6",
        Key::F7 => "F7",
        Key::F8 => "F8",
        Key::F9 => "F9",
        Key::F10 => "F10",
        Key::F11 => "F11",
        Key::F12 => "F12",
        Key::ArrowUp => "ArrowUp",
        Key::ArrowDown => "ArrowDown",
        Key::ArrowLeft => "ArrowLeft",
        Key::ArrowRight => "ArrowRight",
        Key::Escape => "Escape",
        Key::Tab => "Tab",
        Key::Backspace => "Backspace",
        Key::Enter => "Enter",
        Key::Space => "Space",
        Key::Insert => "Insert",
        Key::Delete => "Delete",
        Key::Home => "Home",
        Key::End => "End",
        Key::PageUp => "PageUp",
        Key::PageDown => "PageDown",
        Key::Comma => "Comma",
        Key::Slash | Key::Questionmark => "Slash",
        Key::Backslash | Key::Pipe => "Backslash",
        Key::OpenBracket | Key::OpenCurlyBracket => "BracketLeft",
        Key::CloseBracket | Key::CloseCurlyBracket => "BracketRight",
        Key::Backtick => "Backquote",
        Key::Minus => "Minus",
        Key::Period => "Period",
        Key::Plus | Key::Equals => "Equal",
        Key::Semicolon | Key::Colon => "Semicolon",
        Key::Quote => "Quote",
        _ => return None,
    })
}

fn normalized_key_name(value: &str) -> String {
    let trimmed = value.trim();
    match trimmed {
        "," => "comma".into(),
        "." => "period".into(),
        "/" | "?" => "slash".into(),
        "\\" | "|" => "backslash".into(),
        "[" | "{" => "bracketleft".into(),
        "]" | "}" => "bracketright".into(),
        "`" => "backquote".into(),
        "-" => "minus".into(),
        ";" | ":" => "semicolon".into(),
        "'" | "\"" => "quote".into(),
        "=" => "equal".into(),
        _ => {
            let compact = trimmed
                .chars()
                .filter(|c| !matches!(c, '-' | '_' | ' '))
                .collect::<String>()
                .to_ascii_lowercase();
            if compact.len() == 1 && compact.as_bytes()[0].is_ascii_alphabetic() {
                format!("key{compact}")
            } else if compact.len() == 1 && compact.as_bytes()[0].is_ascii_digit() {
                format!("digit{compact}")
            } else {
                compact
            }
        }
    }
}

fn parse_key(value: &str) -> Option<egui::Key> {
    let wanted = normalized_key_name(value);
    egui::Key::ALL.iter().copied().find(|key| {
        key_code(*key).is_some_and(|code| normalized_key_name(code) == wanted)
            || normalized_key_name(&format!("{key:?}")) == wanted
    })
}

pub fn parse_shortcut(value: &str) -> Result<Option<KeyChord>, String> {
    if value.trim().is_empty() {
        return Ok(None);
    }
    let mut chord = KeyChord {
        key: egui::Key::F1,
        control: false,
        alt: false,
        shift: false,
        super_key: false,
    };
    let mut key = None;
    for part in value.split('+').map(str::trim) {
        if part.is_empty() {
            return Err("shortcut contains an empty part".into());
        }
        match part.to_ascii_lowercase().as_str() {
            "ctrl" | "control" if !chord.control => chord.control = true,
            "shift" if !chord.shift => chord.shift = true,
            "alt" | "option" if !chord.alt => chord.alt = true,
            "cmd" | "command" | "super" | "win" | "meta" if !chord.super_key => {
                chord.super_key = true
            }
            name if matches!(
                name,
                "ctrl"
                    | "control"
                    | "shift"
                    | "alt"
                    | "option"
                    | "cmd"
                    | "command"
                    | "super"
                    | "win"
                    | "meta"
            ) =>
            {
                return Err(format!("shortcut repeats modifier {part}"));
            }
            _ if key.is_none() => key = parse_key(part),
            _ => return Err("shortcut contains more than one key".into()),
        }
        if key.is_none()
            && !matches!(
                part.to_ascii_lowercase().as_str(),
                "ctrl"
                    | "control"
                    | "shift"
                    | "alt"
                    | "option"
                    | "cmd"
                    | "command"
                    | "super"
                    | "win"
                    | "meta"
            )
        {
            return Err(format!("unsupported application shortcut key: {part}"));
        }
    }
    chord.key = key.ok_or_else(|| "shortcut has modifiers but no key".to_owned())?;
    Ok(Some(chord))
}

fn event_matches(event: &egui::Event, chord: KeyChord, text_editing: bool) -> bool {
    let egui::Event::Key {
        key,
        physical_key,
        pressed: true,
        repeat: false,
        modifiers,
    } = event
    else {
        return false;
    };
    if text_editing
        && !chord.control
        && !chord.super_key
        && !chord.alt
        && !key_code(chord.key).is_some_and(|name| name.starts_with('F'))
    {
        return false;
    }
    physical_key.unwrap_or(*key) == chord.key
        && modifiers.ctrl == chord.control
        && modifiers.alt == chord.alt
        && modifiers.shift == chord.shift
        && modifiers.mac_cmd == chord.super_key
}

impl ApplicationShortcuts {
    pub fn entries(&self) -> [(ApplicationAction, &str); 6] {
        [
            (ApplicationAction::Fullscreen, &self.fullscreen),
            (ApplicationAction::Refresh, &self.refresh),
            (ApplicationAction::Preferences, &self.preferences),
            (ApplicationAction::DeviceSettings, &self.device_settings),
            (ApplicationAction::Connect, &self.connect),
            (ApplicationAction::CellMapping, &self.cell_mapping),
        ]
    }

    pub fn validate(&self) -> Result<(), String> {
        let mut chords = std::collections::BTreeSet::new();
        for (action, value) in self.entries() {
            if value.len() > 64 {
                return Err(format!("{action:?}: shortcut is too long"));
            }
            if let Some(chord) =
                parse_shortcut(value).map_err(|error| format!("{action:?}: {error}"))?
                && !chords.insert(chord)
            {
                return Err(
                    "application shortcuts must not assign the same chord to multiple actions"
                        .into(),
                );
            }
        }
        Ok(())
    }

    pub fn action_for_event(
        &self,
        event: &egui::Event,
        text_editing: bool,
    ) -> Option<ApplicationAction> {
        self.entries().into_iter().find_map(|(action, value)| {
            let chord = parse_shortcut(value).ok().flatten()?;
            event_matches(event, chord, text_editing).then_some(action)
        })
    }
}

/// Consume exactly one initial key-down event. Repeats and releases remain
/// unhandled, and bare printable rebinding never steals normal text input.
pub fn take_action(
    ctx: &egui::Context,
    enabled: bool,
    shortcuts: &ApplicationShortcuts,
) -> Option<ApplicationAction> {
    if !enabled {
        return None;
    }
    let text_editing = ctx.text_edit_focused();
    ctx.input_mut(|input| {
        let (index, action) = input.events.iter().enumerate().find_map(|(index, event)| {
            shortcuts
                .action_for_event(event, text_editing)
                .map(|action| (index, action))
        })?;
        input.events.remove(index);
        Some(action)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn key(key: egui::Key, modifiers: egui::Modifiers, repeat: bool, pressed: bool) -> egui::Event {
        egui::Event::Key {
            key,
            physical_key: Some(key),
            pressed,
            repeat,
            modifiers,
        }
    }

    #[test]
    fn defaults_use_exact_modifiers_and_reject_repeat_and_release() {
        let shortcuts = ApplicationShortcuts::default();
        shortcuts.validate().unwrap();
        assert_eq!(
            shortcuts.action_for_event(
                &key(egui::Key::F11, egui::Modifiers::NONE, false, true),
                true
            ),
            Some(ApplicationAction::Fullscreen)
        );
        assert_eq!(
            shortcuts.action_for_event(
                &key(egui::Key::F11, egui::Modifiers::NONE, true, true),
                false
            ),
            None
        );
        assert_eq!(
            shortcuts.action_for_event(
                &key(egui::Key::F11, egui::Modifiers::NONE, false, false),
                false
            ),
            None
        );
        assert_eq!(
            shortcuts.action_for_event(
                &key(
                    egui::Key::K,
                    egui::Modifiers::CTRL | egui::Modifiers::SHIFT,
                    false,
                    true
                ),
                false
            ),
            None
        );
    }

    #[test]
    fn configurable_shortcuts_roundtrip_validate_and_preserve_text_editing() {
        let mut shortcuts = ApplicationShortcuts::default();
        shortcuts.fullscreen = "Alt+Enter".into();
        assert_eq!(
            shortcuts.action_for_event(
                &key(egui::Key::Enter, egui::Modifiers::ALT, false, true),
                false
            ),
            Some(ApplicationAction::Fullscreen)
        );
        shortcuts.fullscreen = "G".into();
        assert_eq!(
            shortcuts
                .action_for_event(&key(egui::Key::G, egui::Modifiers::NONE, false, true), true),
            None
        );
        assert_eq!(
            shortcuts.action_for_event(
                &key(egui::Key::G, egui::Modifiers::NONE, false, true),
                false
            ),
            Some(ApplicationAction::Fullscreen)
        );
        let decoded: ApplicationShortcuts =
            serde_json::from_str(&serde_json::to_string(&shortcuts).unwrap()).unwrap();
        assert_eq!(decoded, shortcuts);
        shortcuts.fullscreen = shortcuts.refresh.clone();
        assert!(shortcuts.validate().is_err());
        shortcuts.fullscreen = "not-a-key".into();
        assert!(shortcuts.validate().is_err());
    }

    #[test]
    fn take_action_consumes_only_the_matched_initial_event_once() {
        let shortcuts = ApplicationShortcuts::default();
        let ctx = egui::Context::default();
        let mut output = ctx.run_ui(
            egui::RawInput {
                events: vec![key(egui::Key::F5, egui::Modifiers::NONE, false, true)],
                ..Default::default()
            },
            |_| {
                assert_eq!(
                    take_action(&ctx, true, &shortcuts),
                    Some(ApplicationAction::Refresh)
                );
                assert_eq!(take_action(&ctx, true, &shortcuts), None);
                assert!(!ctx.input(|input| input.key_pressed(egui::Key::F5)));
            },
        );
        output.textures_delta.clear();
    }
}
