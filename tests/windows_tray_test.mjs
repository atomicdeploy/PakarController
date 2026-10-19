import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../' + path, import.meta.url), 'utf8');
const tray = read('desktop/src/platform/windows_tray.rs');
const hook = read('desktop/src/platform/windows_notifications.rs');
const app = read('desktop/src/main.rs');
assert.match(app, /#!\[cfg_attr\(windows, windows_subsystem = "windows"\)\]/);
assert.match(app, /cli_console::prepare\(&arguments\)/);
const consolePolicy = read('desktop/src/cli_console.rs');
assert.match(consolePolicy, /AttachConsole\(ATTACH_PARENT_PROCESS\)/);
assert.doesNotMatch(consolePolicy, /AllocConsole\s*\(/);
assert.match(consolePolicy, /SetStdHandle\(kind, handle\)/);
assert.doesNotMatch(app, /(?<!cli_)\b(?:println|eprintln|print)!/);
const identity = read('desktop/src/application_identity.rs');
assert.match(identity, /APP_NAME: &str = "PakarController"/);
assert.match(identity, /WINDOWS_APP_ID: &str = APP_NAME/);
const jumpList = read('desktop/src/platform/windows_jump_list.rs');
assert.equal((jumpList.match(/wide_null\(crate::application_identity::WINDOWS_APP_ID\)/g) || []).length, 2);
assert.doesNotMatch(jumpList, /Digitalogic\.PakarController/);
assert(app.indexOf('platform::windows_jump_list::install()') < app.indexOf('let options = eframe::NativeOptions'),
    'Set native shell identity before the first GUI window is created');
const build = read('desktop/build.rs');
for (const property of ['FileDescription', 'ProductName', 'CompanyName']) {
    assert.match(build, new RegExp(`set\\("${property}", application_identity::APP_NAME\\)`));
}
assert.match(tray, /\.with_guid\(TRAY_GUID\)/);
assert.match(tray, /hook\.set_tooltip\(text\)/);
assert.match(hook, /uFlags: NIF_TIP \| NIF_GUID,[\s\S]*guidItem: GUID::from_u128\(TRAY_GUID\)/);
assert.match(hook, /RegisterWindowMessageW\(w!\("TaskbarCreated"\)\)/);
assert.match(hook, /DefSubclassProc\(hwnd, msg, wp, lp\)[\s\S]*if restarted/);
assert.match(tray, /self\.applied = None; \/\/ Never cache a failed request as applied/);
assert.match(tray, /\(delay \* 2\)\.min\(30\)/);
assert.match(app, /event_log\.failure\("Windows tray tooltip", &error\)/);
assert.match(app, /Ok\(true\) => self\.event_log\.recover\("Windows tray tooltip"\)/);
assert.doesNotMatch(app, /tray_tooltip/);
assert.match(read('desktop/src/ui/shell.rs'), /value\["tray_shell"\]/);
console.log('Actual GUID-bound tooltip/restart/retry/incident wiring guards passed; source assertions, not Explorer visibility proof.');
