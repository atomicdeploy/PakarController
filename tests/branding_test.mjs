import assert from 'node:assert/strict';
import { readFileSync, statSync } from 'node:fs';
import { inflateSync } from 'node:zlib';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const master = read('assets/branding/app-icon.png.base64').trim();
const decoderSource = read('desktop/src/branding.rs');
const decoder = decoderSource.slice(decoderSource.indexOf('pub fn icon_data_from_bytes'),
  decoderSource.indexOf('pub fn window_icon')).trim().replace(/\s+/g, '');
assert.equal(createHash('sha256').update(decoder).digest('hex'),
  '9f20bbd7a6f5fdb11c051eba6ba649fd829bbf07526b89c4eb67d55931900cec', 'actual Pealayer icon_data_from_bytes import');
const png = Buffer.from(master, 'base64');
assert.equal(createHash('sha256').update(png).digest('hex'),
  '05bc6ce48c57eb8bf6c8f1acd98f10389ae2e6fcc40b4bf41b09f9ce39c85783');
assert.equal(png.readUInt32BE(16), 64);
assert.equal(png.readUInt32BE(20), 64);
assert.deepEqual([...png.subarray(24,29)], [8,6,0,0,0]);
// This master preserves the exact existing native Phosphor raster, not a
// hand-drawn similar icon. All scanlines use PNG filter zero.
let idat = [];
for (let at = 8; at < png.length;) {
  const length = png.readUInt32BE(at), name = png.toString('ascii',at+4,at+8);
  if (name === 'IDAT') idat.push(png.subarray(at+8,at+8+length));
  at += length+12;
}
const raw = inflateSync(Buffer.concat(idat)), pixels = Buffer.alloc(64*64*4);
assert.equal(raw.length, 64*257);
for (let y = 0; y < 64; y++) {
  assert.equal(raw[y*257], 0);
  raw.copy(pixels,y*256,y*257+1,(y+1)*257);
}
assert.equal(createHash('sha256').update(pixels).digest('hex'),
  'd24d849f01bfeaa0cffef1cc9b861062b52e1a320f4c943608fb5fd952ded387');
const web = read('firmware/esp8266/src/WebUi.h');
const icons = [...web.matchAll(/(?:href|src)="data:image\/png;base64,([^"]+)"/g)].map(match=>match[1]);
assert.equal(icons.length, 4, 'header, login, favicon and touch artwork');
assert(icons.every(icon=>icon===master));
assert(!web.includes('class="brandmark">◆'));
assert.equal((web.match(/alt="PakarController application logo"/g)||[]).length, 2);
const build = read('desktop/build.rs'), native = read('desktop/src/main.rs'), shell = read('desktop/src/ui/shell.rs');
assert(build.includes('brand_image::native_icon_bytes(brand_image::icon_bytes())'));
assert(build.includes('brand_image::rgba_at_size(size)'));
assert(!build.includes('shell_icon_ico(egui_phosphor::regular::CIRCUITRY)'));
assert(native.includes('.with_icon(branding::window_icon())'));
assert.equal((shell.match(/crate::branding::logo\(/g)||[]).length, 2);
assert(read('desktop/src/platform/windows_tray.rs').includes('/pakar-icon-32.rgba'));
assert(read('desktop/src/brand_image.rs').includes('assets/branding/app-icon.png.base64'));
for (const name of ['bundled_artwork_decodes_without_files_or_theme_specific_replacements',
  'shell_derivative_preserves_native_master_and_multiple_resolutions',
  'header_and_about_share_one_cached_texture_in_both_themes']) assert(read('desktop/src/branding.rs').includes(`fn ${name}(`));
const path = new URL('../firmware/esp8266/src/WebUi.h', import.meta.url);
const before = statSync(path).mtimeMs;
const check = spawnSync(process.execPath, [new URL('../tools/generate-branding.mjs',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1'), '--check'], {encoding:'utf8'});
assert.equal(check.status, 0, check.stderr);
assert.equal(statSync(path).mtimeMs, before, 'unchanged check must not write SSD');
console.log('Branding checks passed: exact native pixels preserved, four identical Web assets, all native branding consumers share the master; no-write check. Rust/runtime acceptance remains separate.');
