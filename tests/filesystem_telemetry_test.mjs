import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const routes=readFileSync(new URL('../firmware/esp/src/MaintenanceRoutes.h',import.meta.url),'utf8');
const main=readFileSync(new URL('../firmware/esp/application.ino',import.meta.url),'utf8');
const platform=readFileSync(new URL('../firmware/esp/src/FirmwarePlatform.h',import.meta.url),'utf8');
function cachedTelemetry(source,sketch) {
  const begin=source.indexOf('String buildTelemetryJson()'),end=source.indexOf('bool prepareRemotePair(');
  assert.ok(begin>=0&&end>begin);
  const body=source.slice(begin,end);
  assert.doesNotMatch(body,/LittleFS\.(?:totalBytes|usedBytes|info)\s*\(|esp_littlefs_info\s*\(|readFilesystemTelemetry\s*\(|refreshFilesystemTelemetry\s*\(/);
  assert.match(body,/PakarFilesystem::append\([\s\S]*?filesystemTelemetry.snapshot\(commandClock\(\)\)\)/);
  assert.match(source,/esp_littlefs_info\("spiffs",&capacity,&occupied\)/);
  const setup=sketch.indexOf('void setup()'),loop=sketch.indexOf('void loop()');
  const boot=sketch.slice(setup,loop);
  const capture=boot.indexOf('captureBootFilesystemTelemetry(filesystemMounted);');
  assert.ok(capture>=0&&capture<boot.indexOf('grid.begin(false);'),'Only boot captures before LED initialization');
  assert.doesNotMatch(sketch.slice(loop),/captureBootFilesystemTelemetry\(|refreshFilesystemTelemetry\(/,'No automatic runtime filesystem refresh');
}
cachedTelemetry(routes,main);
assert.throws(()=>cachedTelemetry(routes.replace('String buildTelemetryJson() {','String buildTelemetryJson() { LittleFS.usedBytes();'),main));
assert.throws(()=>cachedTelemetry(routes,main.replace('captureBootFilesystemTelemetry(filesystemMounted);','')));
// Never substitute diagnostic cache data into irreversible storage admission.
assert.match(platform,/inline uint32_t freeStorage\(\) \{[\s\S]*?LittleFS.totalBytes\(\),used=LittleFS.usedBytes\(\)/);
assert.doesNotMatch(platform,/filesystemTelemetry|FilesystemTelemetry/);
console.log('Filesystem telemetry source guards passed: boot-before-LED, cached runtime, explicit idle refresh and fresh independent storage admission retained. Not a device timing result.');
