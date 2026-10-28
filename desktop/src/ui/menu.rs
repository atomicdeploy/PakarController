//! Actual Pealayer src/ui/menu.rs root-menu helper, with only the application
//! state ID and visibility adapted. Shortcut rows in shell.rs use its actual
//! Button::shortcut_text idiom; no popup height/font/spacing override is added.
use eframe::egui;

/// Imported Pealayer icons::submenu: the same Phosphor caret as other controls.
pub fn submenu(ui: &mut egui::Ui, label: String, contents: impl FnOnce(&mut egui::Ui)) {
    let button = egui::Button::new(label).right_text(super::icons::CARET_RIGHT);
    let _ = egui::containers::menu::SubMenuButton::from_button(button).ui(ui, contents);
}

/// Switch sibling menus on hover while the menubar is active, matching the
/// interaction of native Windows menu bars. egui's root menu buttons otherwise
/// only toggle on click.
pub fn top_menu_button<R>(
    ui: &mut egui::Ui,
    title: String,
    add_contents: impl FnOnce(&mut egui::Ui) -> R,
) -> egui::InnerResponse<Option<R>> {
    let state_id = egui::Id::new("pakar_top_menu_hover_state");
    let active_popup = ui.ctx().data(|data| data.get_temp::<egui::Id>(state_id));
    let another_heading_is_open =
        active_popup.is_some_and(|popup_id| egui::Popup::is_id_open(ui.ctx(), popup_id));

    let result = ui.menu_button(title, add_contents);
    let popup_id = egui::Popup::default_response_id(&result.response);

    if result.response.hovered() && another_heading_is_open && active_popup != Some(popup_id) {
        // Opening one popup closes the previously open root popup.
        egui::Popup::open_id(ui.ctx(), popup_id);
        ui.ctx()
            .data_mut(|data| data.insert_temp(state_id, popup_id));
        ui.ctx().request_repaint();
    } else if egui::Popup::is_id_open(ui.ctx(), popup_id) {
        ui.ctx()
            .data_mut(|data| data.insert_temp(state_id, popup_id));
    } else if active_popup == Some(popup_id) {
        ui.ctx().data_mut(|data| data.remove::<egui::Id>(state_id));
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn painted_text_colors(
        ctx: &egui::Context,
        text: &egui::epaint::TextShape,
        scale: f32,
    ) -> Vec<egui::Color32> {
        // Layout jobs cache PLACEHOLDER colors. The renderer resolves the
        // TextShape fallback/override; inspect its real tessellated vertices.
        ctx.tessellate(
            vec![egui::epaint::ClippedShape {
                clip_rect: egui::Rect::EVERYTHING,
                shape: egui::Shape::Text(text.clone()),
            }],
            scale,
        )
        .into_iter()
        .flat_map(|primitive| match primitive.primitive {
            egui::epaint::Primitive::Mesh(mesh) => mesh
                .vertices
                .into_iter()
                .map(|v| v.color)
                .collect::<Vec<_>>(),
            egui::epaint::Primitive::Callback(_) => Vec::new(),
        })
        .collect()
    }

    #[test]
    fn shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes() {
        for (dark, palette) in [
            (false, None),
            (true, None),
            (false, Some(crate::ui::palette::Palette::Studio)),
            (true, Some(crate::ui::palette::Palette::Studio)),
            (false, Some(crate::ui::palette::Palette::Native)),
            (true, Some(crate::ui::palette::Palette::Native)),
        ] {
            let ctx = egui::Context::default();
            let mut visuals = if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            };
            if let Some(palette) = palette {
                crate::ui::palette::apply(&mut visuals, palette);
            }
            ctx.set_visuals(visuals);
            let style = ctx.style_of(ctx.theme());
            let weak = style.visuals.weak_text_color();
            // egui 0.36.2 Button::atom_ui uses widget_style(Inactive) for
            // this first, pointer-free frame. Visuals::text_color instead
            // belongs to noninteractive labels (different in stock themes).
            let primary = style
                .visuals
                .override_text_color
                .unwrap_or(style.visuals.widgets.inactive.fg_stroke.color);
            if palette.is_none() {
                assert_ne!(primary, style.visuals.text_color());
            } else {
                assert_eq!(
                    primary,
                    crate::ui::palette::color(palette.unwrap(), dark, "text")
                );
                assert_eq!(
                    weak,
                    crate::ui::palette::color(palette.unwrap(), dark, "muted")
                );
            }
            assert_ne!(weak, primary);
            let rows = [
                ("Device settings…", "Ctrl+Shift+,"),
                ("Connect…", "Ctrl+K"),
                ("Cell mapping…", "Ctrl+M"),
                ("Preferences…", "Ctrl+,"),
                ("Toggle fullscreen", "F11"),
                ("Refresh", "F5"),
            ];
            let mut output = ctx.run_ui(egui::RawInput::default(), |ui| {
                ui.style_mut().spacing.interact_size.y = 28.0;
                ui.style_mut().spacing.item_spacing.y = 6.0;
                let height = ui.spacing().interact_size.y;
                let spacing = ui.spacing().item_spacing;
                egui::containers::menu::menu_style(ui.style_mut());
                ui.with_layout(egui::Layout::top_down_justified(egui::Align::Min), |ui| {
                    ui.set_width(320.0);
                    for (label, shortcut) in rows {
                        let response = ui.add(egui::Button::new(label).shortcut_text(shortcut));
                        assert_eq!(response.rect.height(), height);
                    }
                });
                assert_eq!(ui.spacing().interact_size.y, height);
                assert_eq!(ui.spacing().item_spacing, spacing);
            });
            output.textures_delta.clear();
            let text = |value: &str| {
                output
                    .shapes
                    .iter()
                    .find_map(|shape| match &shape.shape {
                        egui::epaint::Shape::Text(text) if text.galley.job.text == value => {
                            Some(text)
                        }
                        _ => None,
                    })
                    .unwrap_or_else(|| panic!("missing separate text atom: {value}"))
            };
            let right = text(rows[0].1).pos.x + text(rows[0].1).galley.rect.right();
            for (label, shortcut) in rows {
                let binding = text(shortcut);
                assert!((binding.pos.x + binding.galley.rect.right() - right).abs() < 0.5);
                let binding_colors = painted_text_colors(&ctx, binding, output.pixels_per_point);
                let label_colors = painted_text_colors(&ctx, text(label), output.pixels_per_point);
                assert!(!binding_colors.is_empty());
                assert!(!label_colors.is_empty());
                assert!(
                    binding_colors.iter().all(|color| *color == weak),
                    "shortcut ink: dark={dark}, palette={palette:?}, row={shortcut}, expected={weak:?}, actual={binding_colors:?}"
                );
                assert!(
                    label_colors.iter().all(|color| *color == primary),
                    "button ink: dark={dark}, palette={palette:?}, row={label}, expected={primary:?}, actual={label_colors:?}"
                );
                assert!(binding.pos.x > text(label).pos.x + text(label).galley.rect.right());
            }
        }
    }

    fn headings(ctx: &egui::Context, events: Vec<egui::Event>) -> [egui::Response; 2] {
        let mut responses = None;
        ctx.run_ui(
            egui::RawInput {
                events,
                ..Default::default()
            },
            |ui| {
                egui::MenuBar::new().ui(ui, |ui| {
                    let file = top_menu_button(ui, "File".into(), |ui| {
                        ui.label("File contents");
                    });
                    let workspace = top_menu_button(ui, "Workspace".into(), |ui| {
                        ui.label("Workspace contents");
                    });
                    responses = Some([file.response, workspace.response]);
                });
            },
        )
        .drop_without_applying_deltas();
        responses.unwrap()
    }

    #[test]
    fn sibling_hover_switches_only_an_already_open_menu_and_clears_closed_state() {
        let ctx = egui::Context::default();
        let initial = headings(&ctx, vec![]);
        let first_id = egui::Popup::default_response_id(&initial[0]);
        let second_id = egui::Popup::default_response_id(&initial[1]);
        headings(
            &ctx,
            vec![egui::Event::PointerMoved(initial[1].rect.center())],
        );
        assert!(
            !egui::Popup::is_id_open(&ctx, second_id),
            "idle hover must not open a menu"
        );
        egui::Popup::open_id(&ctx, first_id);
        headings(&ctx, vec![egui::Event::PointerGone]);
        headings(
            &ctx,
            vec![egui::Event::PointerMoved(initial[1].rect.center())],
        );
        assert!(egui::Popup::is_id_open(&ctx, second_id));
        assert!(!egui::Popup::is_id_open(&ctx, first_id));
        egui::Popup::close_all(&ctx);
        headings(&ctx, vec![egui::Event::PointerGone]);
        assert!(
            ctx.data(|data| data.get_temp::<egui::Id>(egui::Id::new("pakar_top_menu_hover_state")))
                .is_none()
        );
        let isolated = egui::Context::default();
        assert!(
            isolated
                .data(|data| data.get_temp::<egui::Id>(egui::Id::new("pakar_top_menu_hover_state")))
                .is_none()
        );
    }

    #[test]
    fn production_menu_rows_use_shortcut_atoms_and_imported_root_menu_helper() {
        let shell = include_str!("shell.rs");
        let menu = shell
            .split("egui::Panel::top(\"pakar-menu\")")
            .nth(1)
            .unwrap()
            .split("ui.with_layout(egui::Layout::right_to_left")
            .next()
            .unwrap();
        assert_eq!(menu.matches("menu::top_menu_button(").count(), 4);
        assert_eq!(menu.matches(".shortcut_text(").count(), 6);
        for shortcut in [
            "device_settings",
            "connect",
            "cell_mapping",
            "preferences",
            "fullscreen",
            "refresh",
        ] {
            assert!(menu.contains(&format!(".shortcut_text(&shortcuts.{shortcut})")));
        }
        assert!(
            !menu.contains("Ctrl+"),
            "shortcut text must come from persisted bindings, not labels"
        );
    }
}
