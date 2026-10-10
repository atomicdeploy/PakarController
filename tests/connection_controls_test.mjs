// Source wiring guard only. Actual popup/pointer tests live in Rust and must run
// before claiming native runtime acceptance.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const controls = read('desktop/src/ui/connection_controls.rs');
const shell = read('desktop/src/ui/shell.rs');
const worker = read('desktop/src/main.rs');
const toolbar = shell.split('egui::Panel::top("pakar-workspace-toolbar")')[1]
  .split('self.open_panels.clear();')[0];
assert.ok(toolbar.includes('ui::connection_controls::websocket('));
assert.ok(toolbar.includes('ui::connection_controls::usb('));
assert.ok(!toolbar.includes('badge(ui, "WebSocket"'));
assert.ok(!toolbar.includes('badge(ui, "USB"'));
for (const action of ['Route(route)', 'Port(port)', 'ReconnectWebSocket', 'ReconnectUsb', 'Configure']) {
  assert.ok(toolbar.includes(`Action::${action}`));
}
assert.ok(toolbar.includes('serialport::available_ports()'));
assert.ok(toolbar.includes('self.config.serial_port = self.serial_port.clone();'));
assert.ok(toolbar.includes('self.serial_tx.send(SerialCommand::Port(self.serial_port.clone()))'));
assert.ok(toolbar.includes('self.last_ws_endpoint = None;'));
assert.ok(!worker.includes('Ok(WsCommand::Connect(_)) => {}'), 'same-endpoint reconnect must not be discarded');
assert.ok(worker.includes('Ok(WsCommand::Connect(new_endpoint)) => {'));
assert.ok(controls.includes('ui.push_id("websocket-control"'));
assert.ok(controls.includes('ui.push_id("usb-control"'));
assert.equal(controls.match(/ui.menu_button\(heading/g)?.length, 2);
for (const text of ['WebSocket offline', 'USB offline', 'Disconnected', 'Port scan failed:',
  'No serial ports detected', 'Connection settings…', 'Automatic · Wi-Fi, then USB']) assert.ok(controls.includes(text));
assert.ok(controls.includes('ui.add_enabled_ui(!identity_pending'));
assert.ok(controls.includes('names.sort();'));
assert.ok(controls.includes('names.dedup();'));
assert.ok(controls.includes('fn offline_dropdowns_open_independently_and_usb_enumerates_only_when_open()'));
assert.ok(shell.includes('"Port or hardware path"'));
console.log('Connection dropdown source wiring passed; native popup/pointer and running-app acceptance remain separate.');
