import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8').replaceAll('\r\n','\n');
const source=JSON.parse(read('third_party/portable-shell/source.json'));
assert.equal(source.revision,'67c5dcb8a63f4055cbdaf02e9ff271c53b46410d');
assert.equal(source.files.length,17);
const ledger=JSON.parse(read('docs/file-comparison-evidence.json')).files.filter(f=>f.origin==='portable-shell');
assert.equal(ledger.length,17);
for(const file of source.files){
  const entry=ledger.find(e=>e.path===file.path);
  assert.equal(entry?.sha256,file.sha256);assert.equal(entry.source_read,'complete');
  const text=file.snapshot?read('third_party/portable-shell/'+file.snapshot):'.';
  assert.equal(Buffer.byteLength(text),file.bytes,file.path);
  assert.equal(createHash('sha256').update(text).digest('hex'),file.sha256,file.path);
  assert.ok(entry.omissions.length||entry.preserved.length,file.path);
}
// The reference C library/examples are deliberately not an executable firmware
// dependency; production imports the bounded adaptation through its real editor.
assert.ok(read('firmware/esp/src/TelnetShell.h').includes('#include "PortableShellCore.h"'));
const service=read('firmware/esp/src/ShellService.h');
for(const text of ['PakarShell::findCommand(verb.c_str())','verb=command->canonical',
  'PakarShell::renderHelp(','PakarShell::toneCommand(args.c_str(),tone)','d["command"]="buzzer"','if(!s.authenticated)return;',
  's.editor.complete(start,shellPropertyCount'])assert.ok(service.includes(text),text);
console.log('All 17 pinned portable-shell files accounted for; live integration wiring checked.');
