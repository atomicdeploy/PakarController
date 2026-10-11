//! Pealayer dialog/rail/card primitives, bound to actual Pakar device contracts.
use crate::{
    device_settings::{Editor, Group, SecretEdit, SecretMode, Write},
    ui::{dialog, icons},
    *,
};
use eframe::egui;

include!("settings_catalog.rs");
pub fn page_index(name: &str) -> Option<usize> {
    PAGE_IDS.iter().position(|page| page.eq_ignore_ascii_case(name))
}
fn matches(page: usize, search: &str) -> bool {
    let query = search.trim().to_lowercase();
    query.is_empty()
        || format!("{} {}", PAGES[page].0, PAGES[page].2)
            .to_lowercase()
            .contains(&query)
}
pub(crate) fn available(app: &PakarControllerApp, page: usize) -> bool {
    PAGE_FEATURES.get(page).is_some_and(|features| features.is_empty() ||
        features.iter().any(|feature| if *feature == "table" { app.has_table() } else { app.has_feature(feature) }))
}
fn field(ui: &mut egui::Ui, icon: &str, label: &str, value: &mut String, secret: bool) {
    dialog::setting_row(ui, icon, label, None, |ui| {
        ui.add_sized(
            [ui.available_width(), 28.0],
            dialog::singleline_text_edit(value).password(secret),
        );
    });
}
fn secret(ui: &mut egui::Ui, id: &str, label: &str, stored: bool, value: &mut SecretEdit) {
    ui.push_id(id, |ui| {
        dialog::setting_row(ui, icons::LOCK, label, None, |ui| {
            egui::ComboBox::from_id_salt("secret-mode")
                .selected_text(match value.mode {
                    SecretMode::Keep => {
                        if stored {
                            "Keep stored secret"
                        } else {
                            "Leave unset"
                        }
                    }
                    SecretMode::Replace => "Replace secret",
                    SecretMode::Clear => "Clear stored secret",
                })
                .show_ui(ui, |ui| {
                    ui.selectable_value(&mut value.mode, SecretMode::Keep, "Keep stored value");
                    ui.selectable_value(&mut value.mode, SecretMode::Replace, "Replace…");
                    ui.selectable_value(&mut value.mode, SecretMode::Clear, "Clear");
                });
        });
        if value.mode == SecretMode::Replace {
            field(ui, icons::LOCK, "New value", &mut value.value, true);
        }
        if value.mode == SecretMode::Clear {
            ui.colored_label(
                ui.visuals().warn_fg_color,
                "The stored secret will be removed when you save cloud settings.",
            );
        }
    });
}
fn network_page(app: &mut PakarControllerApp, draft: &mut Editor, ui: &mut egui::Ui, ready: bool) {
    if app.command_route() == Some(Transport::Serial) {
        dialog::section(ui, icons::PLUG, "USB network provisioning", |ui| {
            ui.label("USB can provision Wi-Fi, but cannot read saved network settings. This is a NEW provisioning request, not observed device configuration. Verify via HTTP afterward.");
            if draft.usb_request.is_none() && ui.button("Create provisioning request").clicked() {
                draft.usb_request = Some(crate::device_settings::Network {
                    station_enabled: true,
                    ap_enabled: true,
                    station_ssid: String::new(),
                });
            }
            if let Some(request) = &mut draft.usb_request {
                ui.checkbox(
                    &mut request.station_enabled,
                    "Enable station in this request",
                );
                ui.checkbox(
                    &mut request.ap_enabled,
                    "Enable recovery AP in this request",
                );
                field(
                    ui,
                    icons::WIFI_HIGH,
                    "Requested SSID",
                    &mut request.station_ssid,
                    false,
                );
                ui.checkbox(
                    &mut draft.open_network,
                    "Open station network (no password)",
                );
                if !draft.open_network {
                    field(
                        ui,
                        icons::LOCK,
                        "Requested password",
                        &mut draft.wifi_password,
                        true,
                    );
                }
                let result = crate::device_settings::usb_payloads(
                    request,
                    &draft.wifi_password,
                    draft.open_network,
                );
                if let Err(error) = &result {
                    ui.label(
                        egui::RichText::new(error)
                            .small()
                            .color(ui.visuals().warn_fg_color),
                    );
                }
                if ui
                    .add_enabled(result.is_ok(), egui::Button::new("Provision via USB…"))
                    .clicked()
                {
                    draft.network_confirmation = true;
                }
            }
        });
        return;
    }
    let Some(network) = &mut draft.network else {
        ui.label("Connect and refresh to read the device's Wi-Fi policy.");
        return;
    };
    dialog::section(ui, icons::WIFI_HIGH, "Wireless network", |ui| {
        ui.add_enabled_ui(ready && draft.pending.is_none(), |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.checkbox(&mut network.value.station_enabled, "Join a Wi-Fi network");
                ui.checkbox(
                    &mut network.value.ap_enabled,
                    "Enable recovery access point",
                );
            });
            field(
                ui,
                icons::WIFI_HIGH,
                "SSID",
                &mut network.value.station_ssid,
                false,
            );
            if ui
                .add_enabled(
                    !draft.scan_pending,
                    egui::Button::new(format!("{}  Scan networks", icons::MAGNIFYING_GLASS)),
                )
                .clicked()
            {
                draft.scan_pending = true;
                let _ = app.http_tx.send(HttpCommand::Configuration(
                    app.endpoint(),
                    "/api/v1/wifi/scan",
                ));
            }
            if !draft.ssids.is_empty() {
                egui::ComboBox::from_id_salt("scanned-ssids")
                    .selected_text("Choose a discovered network…")
                    .show_ui(ui, |ui| {
                        for ssid in &draft.ssids {
                            ui.selectable_value(
                                &mut network.value.station_ssid,
                                ssid.clone(),
                                ssid,
                            );
                        }
                    });
            }
            ui.checkbox(
                &mut draft.open_network,
                "This station network is open (no password)",
            );
            if !draft.open_network {
                field(
                    ui,
                    icons::LOCK,
                    "Wi-Fi password",
                    &mut draft.wifi_password,
                    true,
                );
            }
        });
        ui.label(egui::RichText::new("Station credentials are write-only. This firmware replaces them on each network save; re-enter the password, or explicitly choose an open network.").small().weak());
        if !network.value.ap_enabled {
            ui.colored_label(
                ui.visuals().warn_fg_color,
                "Recovery AP disabled: losing the station connection may require USB recovery.",
            );
        }
        if network.conflict {
            ui.colored_label(
                ui.visuals().warn_fg_color,
                "Device settings changed while you were editing. Discard and reload before saving.",
            );
        }
    });
}
fn cloud_page(draft: &mut Editor, ui: &mut egui::Ui, ready: bool, websocket: bool) {
    let Some(cloud) = &mut draft.cloud else {
        ui.label("Connect and refresh to read the device's cloud settings.");
        return;
    };
    dialog::section(
        ui,
        if websocket {
            icons::LINK
        } else {
            icons::BROADCAST
        },
        if websocket {
            "Outbound WebSocket"
        } else {
            "MQTT connection"
        },
        |ui| {
            ui.add_enabled_ui(ready && draft.pending.is_none(), |ui| {
                let config = &mut cloud.value;
                if websocket {
                    ui.horizontal_wrapped(|ui| {
                        ui.checkbox(&mut config.websocket_enabled, "Enabled");
                        ui.checkbox(&mut config.websocket_tls, "TLS encryption");
                    });
                    field(
                        ui,
                        icons::GLOBE,
                        "Server hostname",
                        &mut config.websocket_host,
                        false,
                    );
                    dialog::setting_row(ui, icons::PLUG, "Port", None, |ui| {
                        ui.add(egui::DragValue::new(&mut config.websocket_port).range(1..=65535));
                    });
                    field(
                        ui,
                        icons::LINK,
                        "Endpoint path",
                        &mut config.websocket_path,
                        false,
                    );
                    secret(
                        ui,
                        "ws-token",
                        "Bearer token",
                        config.websocket_token_set,
                        &mut draft.websocket_token,
                    );
                } else {
                    ui.horizontal_wrapped(|ui| {
                        ui.checkbox(&mut config.mqtt_enabled, "Enabled");
                        ui.checkbox(&mut config.mqtt_tls, "TLS encryption");
                    });
                    field(
                        ui,
                        icons::GLOBE,
                        "Broker hostname",
                        &mut config.mqtt_host,
                        false,
                    );
                    dialog::setting_row(ui, icons::PLUG, "Port", None, |ui| {
                        ui.add(egui::DragValue::new(&mut config.mqtt_port).range(1..=65535));
                    });
                    field(
                        ui,
                        icons::LIST,
                        "Base topic",
                        &mut config.mqtt_base_topic,
                        false,
                    );
                    field(ui, icons::LOCK, "Username", &mut config.mqtt_user, false);
                    secret(
                        ui,
                        "mqtt-password",
                        "Password",
                        config.mqtt_password_set,
                        &mut draft.mqtt_password,
                    );
                }
                dialog::setting_row(ui, icons::GAUGE, "Telemetry interval", None, |ui| {
                    ui.add(
                        egui::DragValue::new(&mut config.telemetry_seconds)
                            .range(1..=3600)
                            .suffix(" s"),
                    );
                });
            });
            ui.label(egui::RichText::new("MQTT and WebSocket settings are stored together. Save applies both drafts; passwords and tokens are never read back or exported.").small().weak());
            if ui
                .add_enabled(
                    ready && draft.pending.is_none(),
                    egui::Button::new("Reconnect stored cloud sessions…"),
                )
                .clicked()
            {
                draft.maintenance_confirmation =
                    Some(crate::maintenance::Operation::ReconnectCloud);
            }
            if cloud.conflict {
                ui.colored_label(
                    ui.visuals().warn_fg_color,
                    "Cloud configuration changed remotely. Discard and reload before saving.",
                );
            }
        },
    );
}
fn submit(app: &mut PakarControllerApp, draft: &mut Editor, group: Group) {
    if draft.pending.is_some() || draft.maintenance_pending.is_some() {
        return;
    }
    if match group {
        Group::Network => draft.network.as_ref().is_some_and(|d| d.conflict),
        Group::Cloud => draft.cloud.as_ref().is_some_and(|d| d.conflict),
    } {
        draft.message = "Discard and refresh conflicting device settings before saving".into();
        return;
    }
    let id = draft.next_id();
    let result = match group {
        Group::Network => draft
            .network
            .as_ref()
            .ok_or("Network settings unavailable".into())
            .and_then(|d| Write::network(id, &d.value, &draft.wifi_password, draft.open_network)),
        Group::Cloud => draft
            .cloud
            .as_ref()
            .ok_or("Cloud settings unavailable".into())
            .and_then(|d| Write::cloud(id, &d.value, &draft.mqtt_password, &draft.websocket_token)),
    };
    match result {
        Ok(write) => {
            if app
                .http_tx
                .send(HttpCommand::Settings(app.endpoint(), write))
                .is_ok()
            {
                draft.pending = Some((id, group));
                draft.message = "Saving, then reading the stored configuration…".into();
            } else {
                draft.message = "Settings worker unavailable; nothing was sent".into();
            }
        }
        Err(error) => draft.message = error,
    }
}
fn maintenance_page(
    app: &mut PakarControllerApp,
    draft: &mut Editor,
    ui: &mut egui::Ui,
    ready: bool,
) {
    use crate::maintenance::Operation;
    let isp = app.has_feature("isp");
    let ota = app.has_feature("ota");
    if ota {
    dialog::section(ui, icons::GLOBE, "Verified remote firmware", |ui| {
        field(
            ui,
            icons::LINK,
            "Manifest HTTPS URL",
            &mut draft.firmware_url,
            false,
        );
        field(
            ui,
            icons::LOCK,
            "Pinned manifest SHA-256",
            &mut draft.firmware_sha256,
            false,
        );
        field(
            ui,
            icons::LOCK,
            "Endpoint bearer (optional)",
            &mut draft.firmware_bearer,
            true,
        );
        if !(1..=3).contains(&draft.firmware_targets) || (!isp && draft.firmware_targets != 1) {
            draft.firmware_targets = if isp { 3 } else { 1 };
        }
        dialog::setting_row(ui, icons::CPU, "Update target", None, |ui| {
            egui::ComboBox::from_id_salt("remote-target")
                .selected_text(match draft.firmware_targets {
                    1 => "ESP",
                    2 => "AVR",
                    _ => "Both boards",
                })
                .show_ui(ui, |ui| {
                    if isp { ui.selectable_value(&mut draft.firmware_targets, 3, "Both boards"); }
                    ui.selectable_value(
                        &mut draft.firmware_targets,
                        1,
                        "ESP (same-version repair)",
                    );
                    if isp { ui.selectable_value(
                        &mut draft.firmware_targets,
                        2,
                        "AVR (same-version repair)",
                    ); }
                });
        });
        let available =
            app.command_route() == Some(Transport::WiFi) && draft.maintenance_pending.is_none();
        ui.horizontal_wrapped(|ui| {
            if ui
                .add_enabled(available, egui::Button::new("Refresh update & telemetry"))
                .clicked()
            {
                for path in ["/api/v1/firmware/remote", "/api/v1/telemetry"] {
                    let _ = app
                        .http_tx
                        .send(HttpCommand::Configuration(app.endpoint(), path));
                }
            }
            let valid = crate::maintenance::validate_remote(
                &draft.firmware_url,
                &draft.firmware_sha256,
                draft.firmware_targets,
            );
            let busy = draft.remote_job.as_ref().is_some_and(|j| j.busy);
            if ui
                .add_enabled(
                    available && draft.remote_job.is_some() && !busy && valid.is_ok() &&
                        (ready || draft.firmware_targets==3),
                    egui::Button::new("Download & verify…"),
                )
                .clicked()
            {
                draft.maintenance_confirmation = Some(Operation::RemoteStage {
                    url: draft.firmware_url.clone(),
                    sha256: draft.firmware_sha256.clone(),
                    targets: draft.firmware_targets,
                    bearer: std::mem::take(&mut draft.firmware_bearer),
                });
            }
        });
        if let Some(job) = draft.remote_job.clone() {
            ui.label(format!(
                "Job {} · {} · {}/{} bytes",
                job.job_id, job.phase, job.received_bytes, job.expected_bytes
            ));
            if !job.error.is_empty() {
                ui.colored_label(ui.visuals().warn_fg_color, &job.error);
            }
            ui.horizontal_wrapped(|ui| {
                if ui
                    .add_enabled(
                        available && job.phase == "ready",
                        egui::Button::new("Apply this verified job…"),
                    )
                    .clicked()
                {
                    draft.maintenance_confirmation = Some(Operation::RemoteApply(job.clone()));
                }
                if ui
                    .add_enabled(
                        available && (job.cancellable || job.phase == "ready"),
                        egui::Button::new("Cancel this job…"),
                    )
                    .clicked()
                {
                    draft.maintenance_confirmation = Some(Operation::RemoteCancel(job));
                }
            });
        } else {
            ui.label("No remote transaction has been read from this device.");
        }
        ui.label(egui::RichText::new("Downloads never program automatically. Apply validates the exact job, boot identity and manifest digest. Matching board versions, durable Prog and recovery EEPROM readback are required. Credentials are memory-only.").small().weak());
        if let Some(value) = &draft.telemetry {
            ui.collapsing("Complete device & client telemetry", |ui| {
                for key in [
                    "esp",
                    "avr",
                    "network",
                    "clock",
                    "discovery",
                    "cloud",
                    "clients",
                ] {
                    ui.collapsing(key, |ui| {
                        let text = serde_json::to_string_pretty(&value[key]).unwrap_or_default();
                        ui.add(
                            egui::Label::new(egui::RichText::new(text).monospace())
                                .selectable(true)
                                .wrap(),
                        );
                    });
                }
            });
        }
    });
    }
    let recovery = isp && app.command_route() == Some(Transport::WiFi)
        && app.status.as_ref().is_some_and(|s| s.programming_latch)
        && draft.pending.is_none()
        && draft.maintenance_pending.is_none();
    if recovery {
        ui.colored_label(ui.visuals().warn_fg_color,"AVR programming latch is active. Normal device operations remain blocked; recovery uses the shared stored AVR image.");
        if ui.button("Resume stored AVR programming…").clicked() {
            draft.maintenance_confirmation = Some(Operation::ProgramAvr);
        }
    }
    dialog::section(ui, icons::FLOPPY_DISK, "Firmware and backup tools", |ui| {
        if isp { ui.label("Maintenance is explicit and never automatically retried. ISP operations interrupt the AVR. Confirm electrical readiness before programming."); }
        ui.add_enabled_ui(
            ready && draft.pending.is_none() && draft.maintenance_pending.is_none(),
            |ui| {
                if isp { ui.horizontal_wrapped(|ui| {
                    if ui.button("Probe AVR ISP…").clicked() {
                        draft.maintenance_confirmation = Some(Operation::Probe);
                    }
                    if ui.button("Back up AVR…").clicked() {
                        draft.maintenance_confirmation = Some(Operation::Backup);
                    }
                }); }
                if isp { ui.horizontal_wrapped(|ui| {
                    for (eeprom, label, name) in [
                        (false, "Export flash backup…", "avr-flash.bin"),
                        (true, "Export EEPROM backup…", "avr-eeprom.bin"),
                    ] {
                        if ui.button(label).clicked() {
                            if let Some(destination) =
                                rfd::FileDialog::new().set_file_name(name).save_file()
                            {
                                let id = draft.next_id();
                                if app
                                    .http_tx
                                    .send(HttpCommand::Maintenance(
                                        app.endpoint(),
                                        id,
                                        Operation::Download {
                                            eeprom,
                                            destination,
                                        },
                                    ))
                                    .is_ok()
                                {
                                    draft.maintenance_pending = Some(id);
                                    draft.message = "Downloading existing backup…".into();
                                }
                            }
                        }
                    }
                }); }
                ui.horizontal_wrapped(|ui| {
                    for (esp, label) in
                        [(false, "Select AVR image…"), (true, "Update ESP firmware…")]
                    {
                        if (esp && !ota) || (!esp && !isp) { continue; }
                        if ui.button(label).clicked() {
                            if let Some(path) = rfd::FileDialog::new()
                                .add_filter("Firmware binary", &["bin"])
                                .pick_file()
                            {
                                let result = (|| {
                                    let metadata =
                                        std::fs::metadata(&path).map_err(|e| e.to_string())?;
                                    if !metadata.is_file() || metadata.len() > 2 * 1024 * 1024 {
                                        return Err(
                                            "Firmware file exceeds the supported size".to_string()
                                        );
                                    }
                                    let bytes = std::fs::read(&path).map_err(|e| e.to_string())?;
                                    crate::maintenance::validate_image(esp, &bytes)?;
                                    Ok(if esp {
                                        Operation::UpdateEsp(bytes)
                                    } else {
                                        Operation::StageAvr(bytes)
                                    })
                                })();
                                match result {
                                    Ok(operation) => {
                                        draft.message = format!("Selected {}", path.display());
                                        draft.maintenance_confirmation = Some(operation);
                                    }
                                    Err(error) => draft.message = error,
                                }
                            }
                        }
                    }
                    if isp && ui.button("Program staged AVR…").clicked() {
                        draft.maintenance_confirmation = Some(Operation::ProgramAvr);
                    }
                });
            },
        );
        if isp { ui.label(egui::RichText::new("Flash export includes Urboot if present. EEPROM restore and fuse writes are not exposed by this firmware; no nonfunctional controls are shown. Header checks do not establish firmware authenticity.").small().weak()); }
    });
}
fn page(
    app: &mut PakarControllerApp,
    draft: &mut Editor,
    ui: &mut egui::Ui,
    index: usize,
    ready: bool,
) {
    if !available(app, index) { return; }
    ui.push_id(index, |ui| {
        ui.heading(PAGES[index].0); ui.add_space(12.0);
        match index {
            0 => {
                dialog::section(ui, icons::CIRCUITRY, "Connected controller", |ui| {
                    if let Some(status) = app.live_status().cloned() {
                        let mut values = vec![("Hostname",status.hostname.clone()),("Firmware",status.version.clone())];
                        if ["display", "text", "buzzer", "inputs", "outputs", "settings"].iter().any(|v| app.has_feature(v)) { values.push(("UART mode",status.link_mode.clone())); }
                        if app.has_feature("leds") { values.push(("LED pixels",status.pixels.to_string())); }
                        for (name,value) in values {
                            dialog::setting_row(ui, icons::INFO, name, None, |ui| { ui.label(&value); });
                        }
                        if app.has_feature("network") { ui.label(if status.ntp_synced { "Device clock synchronized by NTP" } else { "Device clock is not synchronized" }); }
                    } else { ui.label("Device state unavailable. Connect to read the controller's configuration."); }
                    if dialog::action_button(ui, icons::PLUG, "Connection…").clicked() && return_to_workspace(draft,ui) { app.show_connection = true; }
                });
                ui.add_space(12.0);
                dialog::section(ui, icons::GEAR, "Application preferences", |ui| {
                    ui.label("Appearance, language, window behavior, USB connection defaults and workspaces are saved on this PC, not on the boards.");
                    if dialog::action_button(ui, icons::GEAR, "Preferences…").clicked() && return_to_workspace(draft,ui) { app.open_preferences(); }
                });
            }
            1 => { ui.add_enabled_ui((ready || app.command_route()==Some(Transport::Serial)) && !app.identity_pending, |ui| { app.draw_device_identity(ui); }); }
            2 => network_page(app, draft, ui, ready),
            3 | 4 => cloud_page(draft, ui, ready, index == 4),
            5 => { ui.add_enabled_ui(ready, |ui| { app.draw_device_access(ui); }); }
            6 => {
                dialog::section(ui, icons::GRID_FOUR, "Table and LED controls", |ui| {
                    ui.label("Logical cell numbers are row-major. Physical addresses and names are separate; odd columns have a half-cell stagger.");
                    if dialog::action_button(ui, icons::GRID_FOUR, "Cell mapping…").clicked() && return_to_workspace(draft,ui) { app.show_mapping = true; }
                    if dialog::action_button(ui, icons::LIGHTBULB, "Effect controls").clicked() && return_to_workspace(draft,ui) { app.reveal_panels.push(ui::layout::Panel::EffectControls); }
                    ui.label(egui::RichText::new("Brightness and effects are live device controls. Chipset/channel order and pixel count are currently build-only; no unsupported runtime editor is shown.").small().weak());
                });
            }
            7 => app.draw_front_panel(ui),
            8 => {
                app.draw_hardware(ui);
                ui.add_space(12.0);
                ui.label(egui::RichText::new("Clock, fuse, pin ownership, debounce and busy-wire/polling selection are build-time settings in the current firmware—not remotely writable parameters.").small().weak());
            }
            9 => maintenance_page(app, draft, ui, ready),
            _ => {}
        }
        ui.add_space(16.0);
    });
}
fn return_to_workspace(draft: &mut Editor, ui: &egui::Ui) -> bool {
    if draft.dirty() || draft.pending.is_some() || draft.maintenance_pending.is_some() {
        draft.message="Save or discard device edits before opening another editor. In-flight writes cannot be cancelled.".into();
        return false;
    }
    draft.open = false;
    draft.clear_secrets();
    ui.ctx()
        .send_viewport_cmd_to(egui::ViewportId::ROOT, egui::ViewportCommand::Focus);
    true
}
fn editor(app: &mut PakarControllerApp, draft: &mut Editor, ui: &mut egui::Ui) {
    if !available(app, draft.page) { draft.page = 0; }
    let ready = app.command_route() == Some(Transport::WiFi)
        && app.transport != Transport::Serial
        && draft.maintenance_pending.is_none()
        && app.status.as_ref().is_some_and(|s| !s.programming_latch);
    ui.horizontal(|ui| {
        ui.label(
            egui::RichText::new(icons::CIRCUITRY)
                .size(30.0)
                .color(ui.visuals().selection.bg_fill),
        );
        ui.vertical(|ui| {
            ui.heading("Device settings");
            ui.label(egui::RichText::new(&app.base).small().weak());
        });
    });
    ui.add_space(10.0);
    ui.horizontal(|ui| {
        ui.label(icons::MAGNIFYING_GLASS);
        ui.add_sized(
            [ui.available_width().max(120.0) - 112.0, 28.0],
            dialog::singleline_text_edit(&mut draft.search).hint_text("Search device settings"),
        );
        if dialog::action_button(ui, icons::ARROW_CLOCKWISE, "Refresh").clicked() {
            app.configuration_loaded = false;
            draft.message = "Reading device configuration…".into();
        }
    });
    if !ready {
        ui.label(egui::RichText::new("Wi-Fi configuration is read-only until the device is connected and not programming. USB AVR controls remain available when advertised.").small().color(ui.visuals().warn_fg_color));
    }
    ui.add_space(10.0);
    let wide = ui.available_width() >= 620.0;
    if !wide {
        ui.horizontal_wrapped(|ui| {
            for (index, (title, icon, _)) in PAGES.iter().enumerate() {
                if available(app, index) && matches(index, &draft.search)
                    && ui
                        .selectable_label(index == draft.page, format!("{icon} {title}"))
                        .clicked()
                {
                    draft.page = index;
                    draft.search.clear();
                }
            }
        });
    }
    let height = (ui.available_height() - 105.0).max(80.0);
    ui.allocate_ui_with_layout(
        egui::vec2(ui.available_width(), height),
        egui::Layout::left_to_right(egui::Align::Min),
        |ui| {
            if wide {
                ui.allocate_ui_with_layout(
                    egui::vec2(172.0, height),
                    egui::Layout::top_down(egui::Align::Min),
                    |ui| {
                        for (index, (title, icon, _)) in PAGES.iter().enumerate() {
                            if !available(app, index) { continue; }
                            if index == 0 {
                                ui.label(egui::RichText::new("DEVICE").small().weak());
                            }
                            if index == 2 {
                                ui.add_space(12.0);
                                ui.label(egui::RichText::new("CONNECTIONS").small().weak());
                            }
                            if index == 6 {
                                ui.add_space(12.0);
                                ui.label(egui::RichText::new("CONTROLLERS").small().weak());
                            }
                            if available(app, index) && matches(index, &draft.search)
                                && dialog::navigation_button(
                                    ui,
                                    draft.page == index,
                                    icon,
                                    title,
                                    168.0,
                                )
                                .clicked()
                            {
                                draft.page = index;
                                draft.search.clear();
                            }
                        }
                    },
                );
                ui.separator();
                ui.add_space(10.0);
            }
            ui.allocate_ui_with_layout(
                egui::vec2(ui.available_width(), height),
                egui::Layout::top_down(egui::Align::Min),
                |ui| {
                    dialog::scroll_column(ui, "device-settings-body", Some(height), |ui| {
                        let results: Vec<_> = (0..PAGES.len())
                            .filter(|&index| available(app, index) && matches(index, &draft.search))
                            .collect();
                        if results.is_empty() {
                            ui.label("No settings match this search.");
                        } else if draft.search.trim().is_empty() {
                            page(app, draft, ui, draft.page, ready);
                        } else {
                            for index in results {
                                page(app, draft, ui, index, ready);
                            }
                        }
                    });
                },
            );
        },
    );
    ui.separator();
    if !draft.message.is_empty() {
        ui.label(egui::RichText::new(&draft.message).small());
    }
    let validation = match draft.page {
        2 => draft.network.as_ref().and_then(|d| {
            Write::network(0, &d.value, &draft.wifi_password, draft.open_network).err()
        }),
        3 | 4 => draft.cloud.as_ref().and_then(|d| {
            Write::cloud(0, &d.value, &draft.mqtt_password, &draft.websocket_token).err()
        }),
        _ => None,
    };
    if let Some(error) = validation {
        ui.label(
            egui::RichText::new(error)
                .small()
                .color(ui.visuals().warn_fg_color),
        );
    }
    let footer_status = if draft.maintenance_pending.is_some() {
        "Maintenance in progress"
    } else if draft.pending.is_some() {
        "Awaiting device readback"
    } else if draft.dirty() {
        "Unsaved device changes"
    } else {
        "Device configuration"
    };
    dialog::action_bar(
        ui,
        false,
        |ui| {
            ui.label(egui::RichText::new(footer_status).small().weak());
        },
        |ui| {
            let group = match draft.page {
                2 => Some(Group::Network),
                3 | 4 => Some(Group::Cloud),
                _ => None,
            };
            if let Some(group) = group {
                let valid = match group {
                    Group::Network => draft.network.as_ref().is_some_and(|d| {
                        !d.conflict
                            && Write::network(0, &d.value, &draft.wifi_password, draft.open_network)
                                .is_ok()
                    }),
                    Group::Cloud => draft.cloud.as_ref().is_some_and(|d| {
                        !d.conflict
                            && Write::cloud(
                                0,
                                &d.value,
                                &draft.mqtt_password,
                                &draft.websocket_token,
                            )
                            .is_ok()
                    }),
                };
                if ui
                    .add_enabled(
                        ready && draft.pending.is_none() && valid && draft.dirty(),
                        egui::Button::new(format!(
                            "{}  Save {}",
                            icons::CHECK,
                            if group == Group::Network {
                                "network"
                            } else {
                                "cloud"
                            }
                        )),
                    )
                    .clicked()
                {
                    if group == Group::Network {
                        draft.network_confirmation = true;
                    } else {
                        submit(app, draft, group);
                    }
                }
            }
            if ui
                .add_enabled(
                    draft.pending.is_none() && draft.dirty(),
                    egui::Button::new("Discard & reload"),
                )
                .clicked()
            {
                draft.discard();
                app.configuration_loaded = false;
            }
        },
    );
    if draft.network_confirmation {
        egui::Modal::new(egui::Id::new("network-save-confirm")).show(ui.ctx(),|ui| {
            ui.set_width(390.0); ui.heading("Apply network changes?");
            ui.label("The ESP will reassociate Wi-Fi. This connection may drop; reconnect and refresh if readback is unavailable. The station password will be replaced.");
            dialog::action_row(ui,false,|ui| {
                let usb_ready=app.command_route()==Some(Transport::Serial) && draft.usb_request.is_some();
                if ui.add_enabled(ready || usb_ready,egui::Button::new("Apply network")).clicked() {
                    draft.network_confirmation=false;
                    if usb_ready {
                        let result=app.provision_usb_network(draft.usb_request.as_ref().unwrap(),&draft.wifi_password,draft.open_network);
                        match result { Ok(())=> { draft.usb_request=None; draft.clear_secrets(); draft.message=app.message.clone(); }, Err(error)=>draft.message=error }
                    } else { submit(app,draft,Group::Network); }
                }
                if ui.button("Keep editing").clicked() { draft.network_confirmation=false; }
            });
    });
    }
    if let Some(operation) = draft.maintenance_confirmation.clone() {
        egui::Modal::new(egui::Id::new("maintenance-confirm")).show(ui.ctx(), |ui| {
            ui.set_width(420.0);
            ui.heading("Confirm device maintenance");
            ui.label(operation.warning());
            if !draft.message.is_empty() {
                ui.label(&draft.message);
            }
            dialog::action_row(ui, false, |ui| {
                if ui
                    .add_enabled(
                        (ready
                            || (matches!(
                                operation,
                                crate::maintenance::Operation::RemoteApply(_)
                                    | crate::maintenance::Operation::RemoteCancel(_)
                            ) && app.command_route() == Some(Transport::WiFi)
                                && draft.maintenance_pending.is_none())
                            || (matches!(operation, crate::maintenance::Operation::ProgramAvr)
                                && app.command_route() == Some(Transport::WiFi)
                                && app.status.as_ref().is_some_and(|s| s.programming_latch)
                                && draft.maintenance_pending.is_none()))
                            && draft.pending.is_none(),
                        egui::Button::new("Proceed"),
                    )
                    .clicked()
                {
                    let id = draft.next_id();
                    if app
                        .http_tx
                        .send(HttpCommand::Maintenance(app.endpoint(), id, operation))
                        .is_ok()
                    {
                        draft.maintenance_pending = Some(id);
                        draft.message =
                            "Maintenance submitted; waiting for the device result…".into();
                    }
                    draft.maintenance_confirmation = None;
                }
                if ui.button("Cancel").clicked() {
                    draft.maintenance_confirmation = None;
                }
            });
        });
    }
}
fn request_close(draft: &mut Editor) {
    if draft.dirty() || draft.pending.is_some() || draft.maintenance_pending.is_some() {
        draft.close_confirmation = true;
    } else {
        draft.open = false;
        draft.clear_secrets();
        draft.maintenance_confirmation = None;
        draft.network_confirmation = false;
    }
}
fn close_modal(draft: &mut Editor, ctx: &egui::Context) {
    if !draft.close_confirmation {
        return;
    }
    egui::Modal::new(egui::Id::new("device-settings-close")).show(ctx,|ui| {
        let pending=draft.pending.is_some() || draft.maintenance_pending.is_some();
        ui.set_width(370.0); ui.heading(if pending { "Device operation in progress" } else { "Discard device changes?" });
        ui.label(if pending { "Wait for the result before closing. An in-flight device write cannot be safely cancelled." } else { "Unsaved edits and secret values will be cleared." });
        if !pending && ui.button("Discard and close").clicked() { draft.discard(); draft.open=false; draft.close_confirmation=false; draft.maintenance_confirmation=None; }
        if ui.button("Keep open").clicked() { draft.close_confirmation=false; }
    });
}
pub fn draw(app: &mut PakarControllerApp, ctx: &egui::Context) {
    if !app.device_settings.open {
        return;
    }
    let mut draft = std::mem::take(&mut app.device_settings);
    if app.config.native_dialogs && !ctx.embed_viewports() {
        ctx.show_viewport_immediate(
            egui::ViewportId::from_hash_of("pakar-device-settings"),
            egui::ViewportBuilder::default()
                .with_title("Device settings — PakarController")
                .with_inner_size([940.0, 760.0])
                .with_min_inner_size([640.0, 500.0]),
            |child, _| {
                config::apply(child, &app.config);
                if child.input(|i| i.viewport().close_requested()) {
                    request_close(&mut draft);
                    if draft.open {
                        child.send_viewport_cmd(egui::ViewportCommand::CancelClose);
                    }
                }
                egui::CentralPanel::default().show(child, |ui| {
                    egui::Frame::new()
                        .inner_margin(18)
                        .show(ui, |ui| editor(app, &mut draft, ui));
                });
                if dialog::escape_pressed(child) {
                    request_close(&mut draft);
                }
                close_modal(&mut draft, child);
            },
        );
    } else {
        let mut open = true;
        let g = dialog::bounded_geometry(
            ctx.content_rect(),
            16.0,
            egui::vec2(940.0, 760.0),
            egui::vec2(640.0, 500.0),
            egui::vec2(1100.0, 900.0),
        );
        egui::Window::new("Device settings")
            .id(egui::Id::new("pakar-device-settings"))
            .open(&mut open)
            .default_rect(g.default_rect)
            .min_size(g.min_size)
            .max_size(g.max_size)
            .constrain_to(g.bounds)
            .frame(dialog::opaque_window_frame_from_context(ctx))
            .collapsible(false)
            .show(ctx, |ui| editor(app, &mut draft, ui));
        if !open || dialog::escape_pressed(ctx) {
            request_close(&mut draft);
        }
        close_modal(&mut draft, ctx);
    }
    app.device_settings = draft;
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn close_guard_preserves_drafts_and_inflight_writes() {
        let mut draft = Editor {
            open: true,
            wifi_password: "fixture-pass".into(),
            ..Default::default()
        };
        request_close(&mut draft);
        assert!(draft.open && draft.close_confirmation);
        draft.discard();
        draft.close_confirmation = false;
        draft.maintenance_pending = Some(3);
        request_close(&mut draft);
        assert!(draft.open && draft.close_confirmation);
        draft.maintenance_pending = None;
        draft.close_confirmation = false;
        request_close(&mut draft);
        assert!(!draft.open && draft.wifi_password.is_empty());
    }
    #[test]
    fn page_deep_links_are_stable_and_invalid_names_are_not_guessed() {
        assert_eq!(page_index("WiFi"), Some(2));
        assert_eq!(page_index("firmware"), Some(9));
        assert!(page_index("unknown").is_none());
    }
    #[test]
    fn navigation_covers_real_parameter_domains_without_empty_search_results() {
        for query in [
            "SSID",
            "MQTT",
            "token",
            "TM1637",
            "EEPROM",
            "credentials",
            "physical",
            "UART",
        ] {
            assert!((0..PAGES.len()).any(|p| matches(p, query)), "{query}");
        }
        assert!(!(0..PAGES.len()).any(|p| matches(p, "not-a-setting")));
        assert_eq!(PAGES.len(), 10);
    }
}
