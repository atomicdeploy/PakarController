//! Actual Pealayer image decoder adapted to Pakar's single cross-surface master.
pub use crate::brand_image::icon_bytes;
use eframe::egui;

// Imported unchanged from Pealayer src/branding.rs.
pub fn icon_data_from_bytes(bytes: &[u8]) -> Option<egui::IconData> {
    let image = image::load_from_memory(bytes).ok()?.into_rgba8();
    let (width, height) = image.dimensions();
    Some(egui::IconData {
        rgba: image.into_raw(),
        width,
        height,
    })
}

pub fn window_icon() -> egui::IconData {
    icon_data_from_bytes(icon_bytes()).expect("bundled application artwork")
}

pub fn logo(ui: &mut egui::Ui, size: f32) {
    let id = egui::Id::new("pakar-shared-brand-texture");
    let texture = ui
        .ctx()
        .data_mut(|data| data.get_temp::<egui::TextureHandle>(id));
    let texture = texture.unwrap_or_else(|| {
        let icon = window_icon();
        let texture = ui.ctx().load_texture(
            "PakarController application icon",
            egui::ColorImage::from_rgba_unmultiplied(
                [icon.width as usize, icon.height as usize],
                &icon.rgba,
            ),
            egui::TextureOptions::LINEAR,
        );
        ui.ctx()
            .data_mut(|data| data.insert_temp(id, texture.clone()));
        texture
    });
    ui.add(egui::Image::new(&texture).fit_to_exact_size(egui::vec2(size, size)))
        .on_hover_text("PakarController application logo");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bundled_artwork_decodes_without_files_or_theme_specific_replacements() {
        let icon = window_icon();
        assert_eq!((icon.width, icon.height), (64, 64));
        assert_eq!(
            icon.rgba.as_slice(),
            &include_bytes!(concat!(env!("OUT_DIR"), "/pakar-icon-64.rgba"))[..]
        );
        assert_eq!(
            crate::brand_image::rgba_at_size(32).as_slice(),
            &include_bytes!(concat!(env!("OUT_DIR"), "/pakar-icon-32.rgba"))[..]
        );
        assert!(icon.rgba.chunks_exact(4).any(|pixel| pixel[3] > 0));
        assert!(icon_data_from_bytes(b"broken artwork").is_none());
    }

    // Adapted donor shell_derivative_preserves_native_master_and_multiple_resolutions:
    // decode the ICO's PNG frames, since Pakar's image dependency is PNG-only.
    #[test]
    fn shell_derivative_preserves_native_master_and_multiple_resolutions() {
        let bytes = crate::brand_image::native_icon_bytes(icon_bytes()).unwrap();
        assert_eq!(&bytes[..4], &[0, 0, 1, 0]);
        let count = u16::from_le_bytes([bytes[4], bytes[5]]) as usize;
        assert_eq!(count, 7);
        for index in 0..count {
            let at = 6 + index * 16;
            let size = if bytes[at] == 0 {
                256
            } else {
                bytes[at] as u32
            };
            let length = u32::from_le_bytes(bytes[at + 8..at + 12].try_into().unwrap()) as usize;
            let offset = u32::from_le_bytes(bytes[at + 12..at + 16].try_into().unwrap()) as usize;
            let frame = icon_data_from_bytes(&bytes[offset..offset + length]).unwrap();
            assert_eq!((frame.width, frame.height), (size, size));
        }
        assert_eq!(
            crate::brand_image::native_icon_bytes(&bytes).unwrap(),
            bytes
        );
    }

    #[test]
    fn header_and_about_share_one_cached_texture_in_both_themes() {
        for dark in [false, true] {
            let ctx = egui::Context::default();
            ctx.set_visuals(if dark {
                egui::Visuals::dark()
            } else {
                egui::Visuals::light()
            });
            let output = ctx.run_ui(egui::RawInput::default(), |ui| {
                logo(ui, 18.0);
                logo(ui, 48.0);
            });
            assert!(
                output
                    .textures_delta
                    .set
                    .iter()
                    .any(|(_, delta)| delta.image.size() == [64, 64])
            );
            let output = ctx.run_ui(egui::RawInput::default(), |ui| {
                logo(ui, 18.0);
                logo(ui, 48.0);
            });
            assert!(
                !output
                    .textures_delta
                    .set
                    .iter()
                    .any(|(_, delta)| delta.image.size() == [64, 64]),
                "do not decode/upload on every frame"
            );
        }
    }
}
