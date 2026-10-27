//! Selected-field presentation edits; status remains the source of applied state.
use eframe::egui;
use serde_json::Value;
const GLOBAL: [&str; 6] = [
    "flashes",
    "flash_on_ms",
    "flash_off_ms",
    "fade_ms",
    "pulse_ms",
    "chase_ms",
];
pub fn parse(text: &str) -> Result<Value, String> {
    let v: Value = serde_json::from_str(text).map_err(|_| "Invalid presentation JSON")?;
    let fields = v.as_object().ok_or("Presentation must be an object")?;
    if fields.is_empty() {
        return Err("Empty presentation".into());
    }
    for (name, value) in fields {
        if name == "groups" {
            let groups = value.as_array().ok_or("Invalid presentation groups")?;
            if groups.is_empty() || groups.len() > 18 {
                return Err("Invalid group count".into());
            }
            let mut seen = Vec::new();
            for group in groups {
                let id = group["id"]
                    .as_u64()
                    .filter(|id| (1..=18).contains(id))
                    .ok_or("Invalid group ID")?;
                if seen.contains(&id) {
                    return Err("Duplicate group".into());
                }
                seen.push(id);
                for (key, value) in group.as_object().ok_or("Invalid group")? {
                    let valid = match key.as_str() {
                        "id" => true,
                        "override_colors" => value.is_boolean(),
                        "win_rgb" | "lose_rgb" => value.as_u64().is_some_and(|v| v <= 0xffffff),
                        "win_motion" | "lose_motion" => value.as_u64().is_some_and(|v| v <= 3),
                        _ => false,
                    };
                    if !valid {
                        return Err("Invalid presentation group setting".into());
                    }
                }
            }
        } else if !value.as_u64().is_some_and(|v| match name.as_str() {
            "flashes" => v <= 10,
            "flash_on_ms" | "flash_off_ms" => (50..=5000).contains(&v),
            "fade_ms" => v <= 10000,
            "pulse_ms" => (100..=10000).contains(&v),
            "chase_ms" => (100..=20000).contains(&v),
            _ => false,
        }) {
            return Err("Invalid presentation timing".into());
        }
    }
    Ok(v)
}
pub fn verify(actual: &Value, patch: &Value) -> bool {
    patch.as_object().is_some_and(|fields| {
        fields.iter().all(|(name, value)| {
            if name == "groups" {
                value.as_array().is_some_and(|groups| {
                    groups.iter().all(|group| {
                        actual["groups"]
                            .as_array()
                            .and_then(|items| items.iter().find(|item| item["id"] == group["id"]))
                            .is_some_and(|item| verify(item, group))
                    })
                })
            } else {
                actual[name] == *value
            }
        })
    })
}
pub fn payload(text: &str, current: &Value) -> Result<Vec<u8>, String> {
    let patch = parse(text)?;
    if let Some(groups) = patch["groups"].as_array() {
        if groups.len() != 1 || patch.as_object().unwrap().len() != 1 {
            return Err("USB edits one group at a time".into());
        }
        let group = &groups[0];
        let existing = current["groups"]
            .as_array()
            .and_then(|items| items.iter().find(|item| item["id"] == group["id"]))
            .ok_or("Missing group readback")?;
        let get = |name: &str| group.get(name).unwrap_or(&existing[name]);
        // Validate all inherited readback values before narrowing to wire widths.
        let mut merged = serde_json::json!({"id": group["id"]});
        for name in [
            "override_colors",
            "win_rgb",
            "lose_rgb",
            "win_motion",
            "lose_motion",
        ] {
            merged[name] = get(name).clone();
        }
        parse(&serde_json::json!({"groups": [merged]}).to_string())?;
        let mut out = vec![
            1,
            group["id"].as_u64().unwrap() as u8,
            u8::from(
                get("override_colors")
                    .as_bool()
                    .ok_or("Missing group colors")?,
            ),
        ];
        for name in ["win_rgb", "lose_rgb"] {
            out.extend_from_slice(
                &(get(name).as_u64().ok_or("Missing group color")? as u32).to_le_bytes()[..3],
            );
        }
        for name in ["win_motion", "lose_motion"] {
            out.push(get(name).as_u64().ok_or("Missing group motion")? as u8);
        }
        Ok(out)
    } else {
        let mut merged = serde_json::json!({});
        for name in GLOBAL {
            merged[name] = patch.get(name).unwrap_or(&current[name]).clone();
        }
        parse(&merged.to_string())?;
        let mut out = vec![0];
        for (i, name) in GLOBAL.iter().enumerate() {
            let value = patch
                .get(*name)
                .unwrap_or(&current[*name])
                .as_u64()
                .ok_or("Missing presentation readback")?;
            if i == 0 {
                out.push(value as u8);
            } else {
                out.extend_from_slice(&(value as u16).to_le_bytes());
            }
        }
        Ok(out)
    }
}
pub fn decode(p: &[u8]) -> Option<Value> {
    let mut value = serde_json::json!({});
    match p {
        [0, ..] if p.len() == 12 => {
            value["flashes"] = p[1].into();
            for (i, name) in GLOBAL[1..].iter().enumerate() {
                value[*name] = u16::from_le_bytes([p[2 + i * 2], p[3 + i * 2]]).into();
            }
        }
        [1, ..] if p.len() == 11 => {
            let rgb = |i| u32::from_le_bytes([p[i], p[i + 1], p[i + 2], 0]);
            value = serde_json::json!({"groups":[{"id":p[1],"override_colors":p[2]!=0,"win_rgb":rgb(3),"lose_rgb":rgb(6),"win_motion":p[9],"lose_motion":p[10]}]});
            if p[2] > 1 {
                return None;
            }
        }
        _ => return None,
    }
    parse(&value.to_string()).ok()
}

pub fn controls(ui: &mut egui::Ui, current: &Value, peer: &str) -> Option<Value> {
    let id = ui.make_persistent_id(("game-presentation-draft", peer));
    let mut draft = ui
        .data_mut(|data| data.get_temp::<Value>(id))
        .unwrap_or_else(|| current.clone());
    let mut send = None;
    ui.collapsing("Winner presentation", |ui| {
        for (name, label, min, max) in [
            ("flashes", "Flashes", 0, 10),
            ("flash_on_ms", "Flash on · ms", 50, 5000),
            ("flash_off_ms", "Flash gap · ms", 50, 5000),
            ("fade_ms", "Fade · ms", 0, 10000),
            ("pulse_ms", "Pulse period · ms", 100, 10000),
            ("chase_ms", "Chase period · ms", 100, 20000),
        ] {
            if let Some(mut value) = draft[name].as_u64() {
                ui.horizontal(|ui| {
                    ui.label(label);
                    if ui
                        .add(egui::DragValue::new(&mut value).range(min..=max))
                        .changed()
                    {
                        draft[name] = value.into();
                    }
                });
            }
        }
        ui.horizontal(|ui| {
            if ui.button("Apply timing").clicked() {
                let mut patch = serde_json::json!({});
                for name in GLOBAL {
                    patch[name] = draft[name].clone();
                }
                send = Some(patch);
            }
            if ui.button("Use board settings").clicked() {
                draft = current.clone();
            }
        });
        if let Some(groups) = draft["groups"].as_array_mut() {
            for group in groups {
                let group_id = group["id"].as_u64().unwrap_or(0);
                ui.collapsing(
                    format!(
                        "Box {} · {}",
                        group_id.div_ceil(2),
                        if group_id % 2 == 1 {
                            "Button"
                        } else {
                            "Indicator"
                        }
                    ),
                    |ui| {
                        if let Some(mut custom) = group["override_colors"].as_bool() {
                            if ui.checkbox(&mut custom, "Custom colors").changed() {
                                group["override_colors"] = custom.into();
                            }
                            if custom {
                                for (name, label) in [("win_rgb", "Winner"), ("lose_rgb", "Others")]
                                {
                                    if let Some(rgb) = group[name].as_u64() {
                                        let mut color =
                                            [(rgb >> 16) as u8, (rgb >> 8) as u8, rgb as u8];
                                        ui.horizontal(|ui| {
                                            ui.label(label);
                                            if ui.color_edit_button_srgb(&mut color).changed() {
                                                group[name] = ((u32::from(color[0]) << 16)
                                                    | (u32::from(color[1]) << 8)
                                                    | u32::from(color[2]))
                                                .into();
                                            }
                                        });
                                    }
                                }
                            }
                        }
                        for (name, label) in [
                            ("win_motion", "Winner motion"),
                            ("lose_motion", "Others motion"),
                        ] {
                            ui.horizontal_wrapped(|ui| {
                                ui.label(label);
                                for (motion, title) in ["Solid", "Pulse", "Ring", "Shared chase"]
                                    .iter()
                                    .enumerate()
                                {
                                    if motion == 3 && (name == "win_motion" || group_id % 2 == 1) {
                                        continue;
                                    }
                                    if ui
                                        .selectable_label(
                                            group[name].as_u64() == Some(motion as u64),
                                            *title,
                                        )
                                        .clicked()
                                    {
                                        group[name] = (motion as u64).into();
                                    }
                                }
                            });
                        }
                        if ui.button("Apply group").clicked() {
                            send = Some(serde_json::json!({"groups":[group.clone()]}));
                        }
                    },
                );
            }
        }
    });
    ui.data_mut(|data| data.insert_temp(id, draft));
    send
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_invalid_inherited_values_before_wire_narrowing() {
        let mut current = serde_json::json!({"flashes":2,"flash_on_ms":500,"flash_off_ms":250,"fade_ms":750,"pulse_ms":1400,"chase_ms":1800});
        current["pulse_ms"] = 66936.into(); // Truncation would produce valid 1400.
        assert!(payload(r#"{"flashes":3}"#, &current).is_err());
        let mut groups = serde_json::json!({"groups":[{"id":2,"override_colors":false,"win_rgb":65280,"lose_rgb":255,"win_motion":257,"lose_motion":3}]});
        assert!(payload(r#"{"groups":[{"id":2,"override_colors":true}]}"#, &groups).is_err());
        groups["groups"][0]["win_motion"] = 1.into();
        groups["groups"][0]["win_rgb"] = 16777216.into();
        assert!(payload(r#"{"groups":[{"id":2,"override_colors":true}]}"#, &groups).is_err());
    }
    #[test]
    fn partial_group_roundtrip_and_readback() {
        let current = serde_json::json!({"groups":[{"id":2,"override_colors":false,"win_rgb":65280,"lose_rgb":255,"win_motion":1,"lose_motion":3}]});
        let patch = r#"{"groups":[{"id":2,"override_colors":true}]}"#;
        let bytes = payload(patch, &current).unwrap();
        assert_eq!(bytes.len(), 11);
        let changed = decode(&bytes).unwrap();
        assert!(verify(&changed, &parse(patch).unwrap()));
        assert!(!verify(&current, &parse(patch).unwrap()));
        assert_eq!(changed["groups"][0]["lose_rgb"], 255);
        assert!(parse(r#"{"pulse_ms":0}"#).is_err());
        assert!(parse(r#"{"groups":[{"id":1},{"id":1}]}"#).is_err());
    }
}
