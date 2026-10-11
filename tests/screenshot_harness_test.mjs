import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const native=read('desktop/src/ui/screenshot.rs'),capture=read('tools/capture-window-windows.ps1');
for(const token of ['ViewportCommand::Screenshot','Event::Screenshot','create_new(true)','Screenshot timed out','PAKAR_SCREENSHOT_PATH'])assert(native.includes(token),token);
for(const token of ['PrintWindow','BitBlt','ScreenSafe','ExpectedProcessId','CreateNew','SetThreadDpiAwarenessContext','Owner does not advertise']) {
 assert(capture.includes(token),token);
}
for(const forbidden of ['SetForegroundWindow','SetCursorPos','mouse_event','SendInput','ShowWindow','SetWindowPos'])assert(!capture.includes(forbidden),'Read-only tool must not inject input or activate/resize: '+forbidden);
assert(read('desktop/src/main.rs').includes('!screenshot_harness'));
assert(read('desktop/src/platform/instance.rs').includes('if enable_listener'));
assert(read('desktop/src/ui/shell.rs').includes('A capture request never changes focus'));
for(const token of ['taskBeforeRequestId','taskObservedPending','taskCorrelated','screenshot.path -eq $taskOutput']) assert(capture.includes(token),token);
assert(native.includes('pub fn tick('));
assert(native.includes('hidden_viewport_deadline_and_request_identity_do_not_need_paint'));
assert(native.includes('minimized_capture_does_not_emit_or_accept_even_if_integration_paints'));
assert.equal(native.match(/viewport\(\)\.visible\(\) == Some\(false\)/g)?.length,2);
assert(native.includes('image.size[0] >= 320 && image.size[1] >= 240'));
console.log('Screenshot harness wiring, no-overwrite, PID ownership and read-only Win32 fallback contracts pass (source guards; runtime PNG proof is separate).');
