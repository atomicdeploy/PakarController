// Source wiring guard only. Actual popup/pointer tests live in Rust and must run
// before claiming native runtime acceptance.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const controls = read('desktop/src/ui/connection_controls.rs');
const shell = read('desktop/src/ui/shell.rs');
const worker = read('desktop/src/main.rs');
const portGuardPattern = /if\s+self\.connection_change_guard\(\)\s*\{\s*continue;\s*\}/;
const releaseGuardPattern = /if\s+self\.connection_enabled\s*\{\s*continue;\s*\}/;
function slice(source, start, end) {
  assert.ok(source.includes(start) && source.includes(end), `Missing source anchors: ${start}`);
  return source.split(start)[1].split(end)[0];
}
function check({ controls, shell, worker }) {
  const toolbar = slice(shell, 'egui::Panel::top("pakar-workspace-toolbar")', 'self.open_panels.clear();');
  assert.equal(toolbar.match(/ui::connection_controls::draw\(/g)?.length, 1, 'Exactly one connection control');
  assert.ok(!/connection_controls::(?:websocket|usb)\(/.test(toolbar), 'No independent transport controls');
  assert.ok(!/Transport::Both|Automatic ·|ReconnectWebSocket|ReconnectUsb/.test(controls + toolbar), 'No implicit dual/fallback method');
  for (const action of ['Method(method)', 'Lifecycle(enabled)', 'Port(port)', 'Configure']) {
    assert.ok(toolbar.includes(`Action::${action}`), `Explicit action ${action}`);
  }
  assert.ok(toolbar.includes('self.set_connection(method, false)'), 'Changing method disconnects explicitly');
  assert.ok(toolbar.includes('self.set_connection(self.transport, enabled)'), 'Lifecycle uses the owner setter');
  assert.ok(!toolbar.includes('self.transport ='), 'Painting must not mutate method before handoff');
  const port = slice(toolbar, 'Action::Port(port)', 'Action::Configure');
  const guard = port.search(portGuardPattern);
  const release = port.indexOf('self.set_connection(self.transport, false)');
  const assign = port.indexOf('self.serial_port = port');
  assert.ok(guard >= 0 && guard < release && release < assign, 'Port draft guard precedes release and mutation');
  assert.ok(releaseGuardPattern.test(port), 'Refused release cannot mutate port');
  assert.ok(port.includes('self.config.serial_port = self.serial_port.clone();'), 'Selected port persists');
  assert.ok(!/Action::Port\(String::new\(\)\)|dropdown_value\([^;]*String::new\(\)/s.test(controls), 'Disconnect retains configured port');
  assert.ok(/\|\|\s*\{\s*serialport::available_ports\(\)/.test(toolbar), 'OS enumeration remains a lazy callback');
  assert.equal(controls.match(/let menu = ui.menu_button\(/g)?.length, 1, 'One dropdown trigger');
  assert.equal(controls.match(/let lifecycle = ui.add_enabled\(/g)?.length, 1, 'One guarded lifecycle action');
  assert.ok(controls.includes('Action::Lifecycle(!state.enabled)'), 'Connect and disconnect share one lifecycle');
  assert.ok(controls.includes('!state.identity_pending && (state.enabled || endpoint_present)'), 'Busy identity locks lifecycle');
  assert.ok(controls.includes('if state.method == Transport::Serial && !state.identity_pending'), 'Only selected idle USB may enumerate ports');
  const ports = slice(controls, 'fn port_choices(', 'pub struct State');
  const sizingGuard = ports.indexOf('if !ui.is_enabled()');
  assert.ok(sizingGuard >= 0 && sizingGuard < ports.indexOf('match ports()'), 'Disabled sizing pass never probes OS');
  assert.ok(ports.includes('return;'), 'Disabled sizing must actually stop');
  assert.ok(ports.includes('names.sort();') && ports.includes('names.dedup();'), 'Stable unique real port entries');
  for (const text of ['Disconnected', 'Port scan failed:', 'No serial ports detected', 'Connection settings…', 'Port or hardware path']) {
    assert.ok(controls.includes(text), `Preserve factual/custom-path affordance: ${text}`);
  }
  const dialog = slice(shell, 'if self.show_connection {', 'if self.show_mapping {');
  assert.ok(dialog.indexOf('method_picker(') < dialog.indexOf('::details('), 'Dialog chooses method first');
  assert.ok(dialog.includes('connection_change_requested && !self.connection_change_guard()'), 'Dialog commits use the same draft guard');
  assert.ok(dialog.includes('method == self.transport && !self.connection_enabled && connection_draft_changed'), 'No field commit after refused method handoff');
  assert.ok(dialog.includes('lifecycle = None;'), 'Method change cannot implicitly connect in the same frame');
  const details = slice(controls, 'pub fn details(', '#[cfg(test)]');
  assert.ok(/match method\s*\{\s*Transport::WiFi =>/.test(details), 'Wi-Fi details are conditional');
  assert.ok(details.includes('Transport::Serial =>'), 'USB has its own conditional details');
  for (const test of ['one_contextual_control_lazy_usb_scan_and_busy_guard',
    'dialog_details_are_selected_method_only_and_do_not_probe_closed_ports',
    'lifecycle_is_one_explicit_action_and_disconnect_retains_port']) {
    assert.ok(controls.includes(`fn ${test}()`), `Retain real egui regression: ${test}`);
  }
  assert.ok(!worker.includes('Ok(WsCommand::Connect(_)) => {}'), 'Same-endpoint reconnect must not be discarded');
  assert.ok(/Ok\(WsCommand::Connect\(new_endpoint,\s*epoch\)\) => \{/.test(worker), 'Reconnect carries its revocable ownership epoch');
}
const source = { controls, shell, worker };
check(source);
// Mutation checks prove these guards reject regressions, not just today's text.
assert.throws(() => check({ ...source, shell: shell.replace(portGuardPattern, '') }), /Port draft guard/);
assert.throws(() => check({ ...source, shell: shell.replace(releaseGuardPattern, '') }), /Refused release/);
assert.throws(() => check({ ...source, controls: controls.replace('if state.method == Transport::Serial && !state.identity_pending', 'if !state.identity_pending') }), /Only selected idle USB/);
assert.throws(() => check({ ...source, controls: controls.replace('if !ui.is_enabled()', 'if false') }), /Disabled sizing pass/);
assert.throws(() => check({ ...source, controls: `${controls}\n// Transport::Both` }), /No implicit dual/);
assert.throws(() => check({ ...source, shell: shell.replace('ui::connection_controls::draw(', 'ui::connection_controls::draw(ui); ui::connection_controls::draw(') }), /Exactly one connection control/);
console.log('Single-method connection source wiring and six negative guards passed; real egui tests and running-app acceptance remain separate.');
