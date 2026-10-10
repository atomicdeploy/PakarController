// One bundled artwork source for build/runtime native and pre-auth Web branding.
// Pealayer HeaderBar consumes resolved artwork; its WebPlatform binds favicon
// to that same resolved icon. Pakar embeds it for the standalone ESP surface.
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
const asset = (await readFile(new URL('../assets/branding/app-icon.png.base64', import.meta.url), 'utf8')).trim();
assert.match(asset, /^[A-Za-z0-9+/]+={0,2}$/);
const png = Buffer.from(asset, 'base64');
assert.equal(png.subarray(0,8).toString('hex'), '89504e470d0a1a0a');
assert.equal(png.readUInt32BE(16), 64);
assert.equal(png.readUInt32BE(20), 64);
const url = new URL('../firmware/esp8266/src/WebUi.h', import.meta.url);
let web = await readFile(url, 'utf8');
const original = web;
const data = `data:image/png;base64,${asset}`;
const blocks = {
  'FAVICON': `<link rel="icon" type="image/png" href="${data}"><link rel="apple-touch-icon" href="${data}">`,
  'HEADER': `<img class="app-logo" src="${data}" width="42" height="42" alt="PakarController application logo">`,
  'LOGIN': `<img class="app-logo" src="${data}" width="42" height="42" alt="PakarController application logo">`,
};
for (const [name, body] of Object.entries(blocks)) {
  const begin = `<!-- BEGIN SHARED BRAND ${name} -->`, end = `<!-- END SHARED BRAND ${name} -->`;
  assert.equal(web.split(begin).length, 2, `Exactly one ${name} begin marker`);
  assert.equal(web.split(end).length, 2, `Exactly one ${name} end marker`);
  const from = web.indexOf(begin)+begin.length, to = web.indexOf(end);
  assert(from <= to, 'Marker order (empty bootstrap block is valid)');
  web = web.slice(0,from)+body+web.slice(to);
}
if (web !== original) {
  assert(!process.argv.includes('--check'), 'Embedded branding is stale; run node tools/generate-branding.mjs');
  await writeFile(url, web);
  console.log('Updated shared Web branding only.');
} else console.log('Shared Web branding is current; no file writes.');
const applicationHeader = new URL('../firmware/esp8266/src/ApplicationArtwork.h', import.meta.url);
const artwork = `// Generated from the same cross-surface master; do not edit by hand.\n#pragma once\n#include <Arduino.h>\nstatic const char kApplicationIconPngBase64[] PROGMEM = "${asset}";\n`;
let prior;
try { prior = await readFile(applicationHeader, 'utf8'); } catch (error) { if (error.code !== 'ENOENT') throw error; }
if (prior !== artwork) {
  assert(!process.argv.includes('--check'), 'Firmware application artwork is stale; regenerate shared branding');
  await writeFile(applicationHeader, artwork);
}
