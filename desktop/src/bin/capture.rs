use std::{env, thread, time::Duration};

use xcap::Window;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // xcap mixes DWM physical bounds and GDI bitmaps. Match their coordinate
    // system before enumerating windows, including mixed-DPI laptops.
    #[cfg(windows)]
    unsafe {
        #[link(name = "user32")]
        unsafe extern "system" {
            fn SetProcessDpiAwarenessContext(context: isize) -> i32;
        }
        let _ = SetProcessDpiAwarenessContext(-4); // PER_MONITOR_AWARE_V2
    }
    let output = env::args().nth(1).ok_or("usage: capture <output.png>")?;
    let title = env::args()
        .nth(2)
        .unwrap_or_else(|| "PakarController".into());
    let expected_pid = env::args().nth(3).map(|v| v.parse::<u32>()).transpose()?;
    for _ in 0..30 {
        for window in Window::all()? {
            let actual_title = window.title()?;
            let matches = if title == "PakarController" {
                actual_title == title
            } else {
                actual_title.contains(&title)
            };
            if matches
                && expected_pid.is_none_or(|pid| window.pid().ok() == Some(pid))
                && !window.is_minimized()?
            {
                println!(
                    "window bounds: {},{} {}x{}",
                    window.x()?,
                    window.y()?,
                    window.width()?,
                    window.height()?
                );
                window.capture_image()?.save(&output)?;
                println!("captured {output}");
                return Ok(());
            }
        }
        thread::sleep(Duration::from_millis(500));
    }
    Err(format!("{title} window was not visible").into())
}
