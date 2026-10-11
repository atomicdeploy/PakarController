import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const access=read('firmware/esp8266/src/WebAccess.h');
assert.ok(access.indexOf('if (!allowed(request))') < access.indexOf('if (!peerAllowed(request))'));
assert.match(access,/peer_changed.*accepted/);
assert.match(access,/PeerFence::accepts/);
const firmware=read('firmware/esp8266/esp8266.ino');
assert.match(firmware,/webAccess.configurePeerFence\(pcBoardId, pcBootId\)/);
assert.match(firmware,/http_peer_fencing/);
// Upload callbacks precede middleware: both actual mutating upload paths must
// use the identical fence before begin()/opening/writing a pending image.
for (const path of ['/update','/api/avr/image']) {
  const start=firmware.indexOf(`server.native().on("${path}", HTTP_POST`);
  assert.ok(start>=0);
  const upload=firmware.slice(start,firmware.indexOf('static uint32_t',start));
  assert.match(upload,/!webAccess.peerAllowed\(request\)/);
}
const tui=read('desktop/src/terminal_ui.rs');
assert.match(tui,/text_command_output\(&pinned_client\(&current\)\?/);
assert.match(tui,/let response = pinned_client\(&s\)\?/);
console.log('Peer fence wiring guards passed (source assertions, not hardware/network acceptance).');
