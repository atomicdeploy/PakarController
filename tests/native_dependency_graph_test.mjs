// A lockfile check, not compilation or installed-app acceptance.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

function coherent(source) {
  const packages=source.split('[[package]]').slice(1).map(block=>({
    name:/^name = "([^"]+)"/m.exec(block)?.[1],
    version:/^version = "([^"]+)"/m.exec(block)?.[1],
  }));
  const frame=packages.filter(p=>p.name==='eframe');
  assert.equal(frame.length,1,'Exactly one native framework');
  for(const name of ['egui','egui-winit','egui_glow','epaint','ecolor','emath']){
    const versions=packages.filter(p=>p.name===name).map(p=>p.version);
    assert.deepEqual(versions,[frame[0].version],`${name} must share the framework type family`);
  }
}
const lock=readFileSync(new URL('../desktop/Cargo.lock',import.meta.url),'utf8');
coherent(lock);
assert.throws(()=>coherent(lock+'\n[[package]]\nname = "egui"\nversion = "0.99.0"\n'));
assert.throws(()=>coherent(lock.replace(/(name = "eframe"\r?\nversion = ")[^"]+/, '$10.99.0')));
console.log('Native lockfile: coherent egui framework/dock/icon/theme type family; mixed-version negatives rejected.');
