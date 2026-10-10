//! Pealayer DockArea / TabViewer shell, adapted to PakarController domains.
use crate::PakarControllerApp;
use eframe::egui;
use egui_dock::{DockState, NodeIndex, TabViewer};
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum Panel {
    Table,
    EffectControls,
    FrontPanel,
    Hardware,
    Network,
    Streaming,
    Events,
}
impl Panel {
    pub const ALL: [Self; 7] = [
        Self::Table,
        Self::EffectControls,
        Self::FrontPanel,
        Self::Hardware,
        Self::Network,
        Self::Streaming,
        Self::Events,
    ];
    pub fn name(self) -> &'static str {
        match self {
            Self::Table => "Table",
            Self::EffectControls => "Effect controls",
            Self::FrontPanel => "Front panel",
            Self::Hardware => "Hardware monitor",
            Self::Network => "Network & cloud",
            Self::Streaming => "Streaming",
            Self::Events => "Events",
        }
    }
    pub fn icon(self) -> &'static str {
        use super::icons::*;
        match self {
            Self::Table => GRID_FOUR,
            Self::EffectControls => SLIDERS_HORIZONTAL,
            Self::FrontPanel => GAUGE,
            Self::Hardware => CIRCUITRY,
            Self::Network => GLOBE,
            Self::Streaming => BROADCAST,
            Self::Events => LIST,
        }
    }
    pub fn from_name(name: &str) -> Option<Self> {
        Self::ALL.into_iter().find(|panel| {
            panel.name().eq_ignore_ascii_case(name)
                || format!("{panel:?}").eq_ignore_ascii_case(name)
        })
    }
}

pub fn create_initial_layout() -> DockState<Panel> {
    let mut dock = DockState::new(vec![Panel::Table]);
    let [center, right] =
        dock.main_surface_mut()
            .split_right(NodeIndex::root(), 0.72, vec![Panel::EffectControls]);
    dock.main_surface_mut()
        .split_below(right, 0.48, vec![Panel::FrontPanel, Panel::Hardware]);
    dock.main_surface_mut().split_below(
        center,
        0.80,
        vec![Panel::Events, Panel::Streaming, Panel::Network],
    );
    sanitize_dock_rects(&mut dock);
    dock
}

// Imported from Pealayer ui/layout.rs::sanitize_dock_rects.
pub fn sanitize_dock_rects<Tab>(dock_state: &mut DockState<Tab>) {
    for (_path, node) in dock_state.iter_all_nodes_mut() {
        if let Some(rect) = node.rect()
            && !rect.is_finite()
        {
            node.set_rect(egui::Rect::ZERO);
        }
        if let Some(leaf) = node.get_leaf_mut()
            && !leaf.viewport.is_finite()
        {
            leaf.viewport = egui::Rect::ZERO;
        }
    }
}
pub fn restore(json: &str) -> DockState<Panel> {
    serde_json::from_str::<DockState<Panel>>(json)
        .ok()
        .filter(|dock| dock.iter_all_tabs().count() > 0)
        .unwrap_or_else(create_initial_layout)
}
pub fn serialize(dock: &DockState<Panel>) -> Result<String, String> {
    let mut copy = dock.clone();
    sanitize_dock_rects(&mut copy);
    serde_json::to_string(&copy).map_err(|e| e.to_string())
}
pub fn reveal(dock: &mut DockState<Panel>, panel: Panel) {
    if let Some(path) = dock.find_tab(&panel) {
        let _ = dock.set_active_tab(path);
        dock.set_focused_node_and_surface(path.node_path());
    } else {
        dock.push_to_focused_leaf(panel);
    }
}
pub fn draw_workspace_tab_menu(app: &mut PakarControllerApp, ui: &mut egui::Ui) {
    super::cell_table::compact_menu(ui);
    for panel in Panel::ALL {
        let is_open = app.dock.find_tab(&panel).is_some() || app.open_panels.contains(&panel);
        let mut visible = is_open;
        if ui
            .checkbox(
                &mut visible,
                format!(
                    "{}  {}  {}",
                    if is_open {
                        super::icons::EYE
                    } else {
                        super::icons::EYE_SLASH
                    },
                    panel.icon(),
                    panel.name()
                ),
            )
            .changed()
        {
            if visible {
                app.reveal_panels.push(panel);
            } else {
                app.close_panels.push(panel);
            }
            ui.close();
        }
    }
}
// Imported from Pealayer ui/layout.rs::paint_dock_disclosure_icons.
pub fn paint_dock_disclosure_icons(ui: &mut egui::Ui, dock: &DockState<Panel>, height: f32) {
    let pointer = ui.ctx().pointer_hover_pos();
    for (_, leaf) in dock.iter_leaves() {
        if !leaf.rect.is_positive() || leaf.tabs.is_empty() {
            continue;
        }
        let rect = egui::Rect::from_min_size(
            leaf.rect.min,
            egui::vec2(24.0_f32.min(leaf.rect.width()), height),
        );
        let hovered = pointer.is_some_and(|position| rect.contains(position));
        let color = if hovered {
            ui.visuals().strong_text_color()
        } else {
            ui.visuals().text_color()
        };
        let icon = if leaf.collapsed {
            super::icons::CARET_RIGHT
        } else {
            super::icons::CARET_DOWN
        };
        ui.painter().text(
            rect.center(),
            egui::Align2::CENTER_CENTER,
            icon,
            egui::FontId::proportional(13.0),
            color,
        );
    }
}
pub struct PakarTabViewer<'a> {
    pub app: &'a mut PakarControllerApp,
}
impl TabViewer for PakarTabViewer<'_> {
    type Tab = Panel;
    fn id(&mut self, tab: &mut Panel) -> egui::Id {
        egui::Id::new(*tab)
    }
    fn title(&mut self, tab: &mut Panel) -> egui::WidgetText {
        format!("{}  {}", tab.icon(), tab.name()).into()
    }
    fn ui(&mut self, ui: &mut egui::Ui, tab: &mut Panel) {
        egui::Frame::new().inner_margin(12).show(ui, |ui| {
            self.app.draw_panel(ui, *tab);
        });
    }
    fn scroll_bars(&self, _: &Panel) -> [bool; 2] {
        [false, false]
    }
    // Actual Pealayer on_tab_button implementation: register rendered tab hit rects.
    fn on_tab_button(&mut self, _: &mut Panel, response: &egui::Response) {
        response.ctx.data_mut(|data| {
            let id = egui::Id::new("pakar-workspace-tab-button-rects");
            let mut rects = data.get_temp::<Vec<egui::Rect>>(id).unwrap_or_default();
            rects.push(response.rect);
            data.insert_temp(id, rects);
        });
    }
    fn context_menu(&mut self, ui: &mut egui::Ui, tab: &mut Panel, _: egui_dock::NodePath) {
        super::cell_table::compact_menu(ui);
        ui.strong(tab.name());
        ui.separator();
        if ui
            .button(format!("{}  Preferences…", super::icons::GEAR))
            .clicked()
        {
            self.app.open_preferences();
            ui.close();
        }
        ui.separator();
        super::icons::submenu(ui, format!("{}  Panels", super::icons::TABS), |ui| {
            draw_workspace_tab_menu(self.app, ui)
        });
        if ui
            .button(format!("{}  Close panel", super::icons::X))
            .clicked()
        {
            self.app.close_panels.push(*tab);
            ui.close();
        }
        if ui
            .button(format!("{}  Close other panels", super::icons::EYE_SLASH))
            .clicked()
        {
            self.app
                .close_panels
                .extend(self.app.open_panels.iter().copied().filter(|p| p != tab));
            ui.close();
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn layouts_survive_serialization_and_closed_panel_reveal() {
        let initial = create_initial_layout();
        let json = serialize(&initial).unwrap();
        let mut restored = restore(&json);
        assert_eq!(
            initial.iter_all_tabs().count(),
            restored.iter_all_tabs().count()
        );
        reveal(&mut restored, Panel::Network);
        reveal(&mut restored, Panel::Network);
        assert_eq!(
            restored
                .iter_all_tabs()
                .filter(|(_, p)| **p == Panel::Network)
                .count(),
            1
        );
        for (_, node) in restored.iter_all_nodes() {
            if let Some(rect) = node.rect() {
                assert!(rect.is_finite());
            }
        }
    }
}
