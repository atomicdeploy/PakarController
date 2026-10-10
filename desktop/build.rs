#[path = "src/platform/windows_quick_actions.rs"]
mod windows_quick_actions;
#[path = "src/platform/windows_shell_icons.rs"]
mod windows_shell_icons;
fn main() {
    let out = std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
    std::fs::write(
        out.join("pakarcontroller.ico"),
        windows_shell_icons::shell_icon_ico(egui_phosphor::regular::CIRCUITRY),
    )
    .unwrap();
    for size in [32, 64] {
        std::fs::write(
            out.join(format!("pakar-icon-{size}.rgba")),
            windows_shell_icons::shell_icon_rgba(egui_phosphor::regular::CIRCUITRY, size),
        )
        .unwrap();
    }
    #[cfg(windows)]
    {
        let mut resources = winres::WindowsResource::new();
        resources
            .set("FileDescription", "PakarController")
            .set("ProductName", "PakarController")
            .set("CompanyName", "Digitalogic")
            .set_icon(out.join("pakarcontroller.ico").to_str().unwrap());
        for action in windows_quick_actions::WINDOWS_QUICK_ACTIONS {
            let file = out.join(format!("quick-action-{}.ico", action.icon_resource_id));
            std::fs::write(
                &file,
                windows_shell_icons::shell_icon_ico(action.icon_glyph),
            )
            .unwrap();
            resources
                .set_icon_with_id(file.to_str().unwrap(), &action.icon_resource_id.to_string());
        }
        resources
            .compile()
            .expect("PakarController executable resources");
    }
    println!("cargo:rerun-if-changed=src/platform/windows_shell_icons.rs");
    println!("cargo:rerun-if-changed=src/platform/windows_quick_actions.rs");
}
