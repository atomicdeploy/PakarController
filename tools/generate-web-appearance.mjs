// Mechanically embed the actual shared assets; do not maintain a second palette.
import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';

const webUrl = new URL('../firmware/esp8266/src/WebUi.h', import.meta.url);
const palette = JSON.parse(await readFile(new URL('../assets/themes/palettes.json', import.meta.url), 'utf8'));
const module = await readFile(new URL('../assets/themes/appearance.js', import.meta.url), 'utf8');
const browser = await readFile(new URL('../assets/themes/browser-appearance.js', import.meta.url), 'utf8');
const surfaces = await readFile(new URL('../assets/themes/controller-surfaces.css', import.meta.url), 'utf8');
const roles = ['canvas','surface-0','surface-1','surface-2','surface-3','line','line-strong','text','muted','subtle','green','blue','red','amber'];
for (const name of ['studio','native']) for (const theme of ['dark','light']) {
  assert.deepEqual(Object.keys(palette[name][theme]).sort(), [...roles].sort());
  for (const value of Object.values(palette[name][theme])) assert.match(value, /^#[0-9a-f]{6}$/i);
}
assert(!module.includes('</script>') && !module.includes(')HTML"'), 'Embedded source cannot break the container');
assert(!browser.includes('</script>') && !browser.includes(')HTML"'), 'Browser source cannot break the container');
let web = await readFile(webUrl, 'utf8');
const original = web;
function replaceBlock(begin, end, body) {
  assert.equal(web.split(begin).length, 2, 'Exactly one begin marker: '+begin);
  assert.equal(web.split(end).length, 2, 'Exactly one end marker: '+end);
  const start = web.indexOf(begin) + begin.length, finish = web.indexOf(end);
  assert(start < finish, 'Marker order');
  web = web.slice(0,start)+'\n'+body+'\n'+web.slice(finish);
}
replaceBlock('/* BEGIN PALETTE FALLBACK */', '/* END PALETTE FALLBACK */',
  ':root{color-scheme:dark;'+Object.entries(palette.studio.dark).map(([role,value])=>'--'+role+':'+value).join(';')+
  ';--accent:#0078d4;--accent-text:#ffffff;--accent-soft:color-mix(in srgb,var(--accent) 16%,transparent);--cyan:var(--blue);--gold:var(--amber)}');
replaceBlock('// BEGIN SHARED APPEARANCE', '// END SHARED APPEARANCE',
  module.replace(/^export /gm, '').replace(/\r\n/g,'\n').trim()+'\nconst PEALAYER_PALETTES='+JSON.stringify(palette)+';');
replaceBlock('// BEGIN BROWSER CONTROLLER', '// END BROWSER CONTROLLER', browser.replace(/\r\n/g,'\n').trim());
replaceBlock('/* BEGIN CONTROLLER SURFACES */','/* END CONTROLLER SURFACES */',surfaces.replace(/\r\n/g,'\n').trim());
if (web !== original) {
  assert(!process.argv.includes('--check'), 'Embedded appearance is stale; run node tools/generate-web-appearance.mjs');
  await writeFile(webUrl, web);
  console.log('Updated embedded shared appearance only.');
} else console.log('Embedded shared appearance is current; no file writes.');
