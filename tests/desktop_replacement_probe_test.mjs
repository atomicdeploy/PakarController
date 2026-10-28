import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=readFileSync(new URL('../tools/replace-desktop.ps1',import.meta.url),'utf8');
const owner=source.slice(source.indexOf('function OwnerStatus {'),source.indexOf('$task=Get-ScheduledTask'));
assert.match(owner,/owner\.ExecutablePath -ine \$ExecutablePath/);
assert.match(owner,/owner\.SessionId -ne \$session/);
assert.match(owner,/AssertHash \$ExecutablePath \$ExpectedOldHash/);
assert.match(owner,/AssertHash \$CandidatePath \$ExpectedNewHash[\s\S]*Invoke-PackageProbe \$CandidatePath '--app-status'/);
assert.match(owner,/status\.process_id -ne \$ExpectedProcessId/);
assert.match(owner,/preferences_dirty/);
assert.match(owner,/maintenance_pending/);
assert.doesNotMatch(source,/Stop-Process|taskkill/i);
console.log('Replacement source guards passed: larger candidate reader retains exact owner, hash, session and draft checks; no force-close fallback.');
