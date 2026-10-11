//! Application-owned framebuffer screenshots, independent of WGC services.
//! Typed IPC/startup harness, with no desktop input, synthetic peer state or overwrite.
use eframe::egui;
use image::ImageEncoder;
use std::{
    path::Path,
    time::{Duration, Instant},
};

pub fn validate_path(path: &str) -> Result<(), String> {
    let p = Path::new(path);
    if path.len() > 4096
        || path.contains('\0')
        || !p.is_absolute()
        || !p.extension().is_some_and(|e| e.eq_ignore_ascii_case("png"))
    {
        return Err("Screenshot needs an absolute PNG path".into());
    }
    Ok(())
}
struct Pending {
    path: String,
    id: u64,
    started: Instant,
    requested: bool,
}
#[derive(Default)]
pub struct State {
    pending: Option<Pending>,
    nonce: u64,
    request_path: String,
    pub result: String,
}
impl State {
    pub fn from_environment() -> Self {
        let mut state = Self::default();
        if let Ok(path) = std::env::var("PAKAR_SCREENSHOT_PATH") {
            if let Err(error) = state.request(path) {
                state.result = error;
            }
        }
        state
    }
    pub fn request(&mut self, path: String) -> Result<(), String> {
        validate_path(&path)?;
        if self.pending.is_some() {
            return Err("Screenshot already pending".into());
        }
        if Path::new(&path).exists() {
            return Err("Screenshot file exists; choose a new output".into());
        }
        self.nonce = self.nonce.wrapping_add(1).max(1);
        self.request_path = path.clone();
        self.pending = Some(Pending {
            path,
            id: self.nonce,
            started: Instant::now(),
            requested: false,
        });
        self.result = "Screenshot pending".into();
        Ok(())
    }
    pub fn status(&self) -> serde_json::Value {
        serde_json::json!({"pending":self.pending.is_some(),"request_id":self.nonce,"path":self.request_path,"result":self.result,"backend":"egui framebuffer"})
    }
    pub fn receive(&mut self, ctx: &egui::Context) {
        let images = ctx.input(|i| {
            i.events
                .iter()
                .filter_map(|e| {
                    if let egui::Event::Screenshot {
                        viewport_id,
                        user_data,
                        image,
                    } = e
                    {
                        if *viewport_id == egui::ViewportId::ROOT {
                            return user_data
                                .data
                                .as_ref()?
                                .downcast_ref::<u64>()
                                .map(|id| (*id, image.clone()));
                        }
                    }
                    None
                })
                .collect::<Vec<_>>()
        });
        for (id, image) in images {
            if self
                .pending
                .as_ref()
                .is_some_and(|p| p.id == id && p.requested)
            {
                let pending = self.pending.take().unwrap();
                self.result = match save_png(&pending.path, &image) {
                    Ok(()) => format!(
                        "Saved {}x{} PNG from egui framebuffer",
                        image.size[0], image.size[1]
                    ),
                    Err(error) => format!("Screenshot failed: {error}"),
                };
            }
        }
    }
    // Deadlines still run when the viewport cannot paint. Never reveal/focus it
    // merely to obtain a screenshot; the caller may use read-only Win32 instead.
    pub fn tick(&mut self, ctx: &egui::Context) {
        if self.pending.as_ref().is_some_and(|p| p.started.elapsed() > Duration::from_secs(10)) {
            self.pending = None;
            self.result = "Screenshot timed out; use the read-only Win32 fallback".into();
        }
        if self.pending.is_some() {
            ctx.request_repaint_after(Duration::from_millis(50));
        }
    }
    pub fn after_frame(&mut self, ctx: &egui::Context) {
        self.tick(ctx);
        let Some(pending) = &mut self.pending else {
            return;
        };
        if !pending.requested && pending.started.elapsed() >= Duration::from_millis(400) {
            ctx.send_viewport_cmd(egui::ViewportCommand::Screenshot(egui::UserData::new(
                pending.id,
            )));
            pending.requested = true;
        }
        ctx.request_repaint_after(Duration::from_millis(50));
    }
}
fn save_png(path: &str, image: &egui::ColorImage) -> Result<(), String> {
    validate_path(path)?;
    let [width, height] = image.size;
    if width == 0
        || height == 0
        || width > 8192
        || height > 8192
        || width.checked_mul(height) != Some(image.pixels.len())
    {
        return Err("Invalid framebuffer dimensions".into());
    }
    let rgba: Vec<u8> = image
        .pixels
        .iter()
        .flat_map(|p| p.to_srgba_unmultiplied())
        .collect();
    let file = std::fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(path)
        .map_err(|e| e.to_string())?;
    image::codecs::png::PngEncoder::new(file)
        .write_image(
            &rgba,
            width as u32,
            height as u32,
            image::ExtendedColorType::Rgba8,
        )
        .map_err(|e| e.to_string())
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hidden_viewport_deadline_and_request_identity_do_not_need_paint() {
        let mut state = State::default();
        let path = std::env::temp_dir().join("pakar-unpainted-test.png").to_string_lossy().into_owned();
        state.request(path.clone()).unwrap();
        assert_eq!(state.status()["path"], path);
        assert_eq!(state.status()["request_id"], 1);
        state.pending.as_mut().unwrap().started = Instant::now() - Duration::from_secs(11);
        let ctx = egui::Context::default();
        let mut input = egui::RawInput::default();
        input.viewports.get_mut(&egui::ViewportId::ROOT).unwrap().minimized = Some(true);
        let output = ctx.run_logic(&input, |ctx| state.tick(ctx));
        assert_eq!(state.status()["pending"], false);
        assert!(state.result.contains("timed out"));
        assert!(output.viewport_commands.is_empty());
        state.request(path).unwrap();
        assert_eq!(state.status()["request_id"], 2);
    }
    #[test]
    fn screenshot_paths_are_explicit_and_png_only() {
        for path in ["", "relative.png", "C:/test.txt"] {
            assert!(validate_path(path).is_err());
        }
        assert!(
            validate_path(
                &std::env::temp_dir()
                    .join("pakar-proof.png")
                    .to_string_lossy()
            )
            .is_ok()
        );
    }
    #[test]
    fn actual_framebuffer_roundtrip_and_no_overwrite() {
        let path =
            std::env::temp_dir().join(format!("pakar-capture-test-{}.png", uuid::Uuid::new_v4()));
        let path = path.to_str().unwrap();
        let image = egui::ColorImage::new([2, 2], vec![egui::Color32::from_rgb(20, 30, 40); 4]);
        save_png(path, &image).unwrap();
        let actual = image::open(path).unwrap().to_rgba8();
        assert_eq!(actual.dimensions(), (2, 2));
        assert_eq!(actual.get_pixel(0, 0).0, [20, 30, 40, 255]);
        assert!(save_png(path, &image).is_err());
        std::fs::remove_file(path).unwrap();
    }
}
