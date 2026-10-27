import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const access=read('firmware/esp/src/WebAccess.h');
assert.ok(access.indexOf('if (!allowed(request))') < access.indexOf('if (!peerAllowed(request))'));
assert.match(access,/peer_changed.*accepted/);
assert.match(access,/PeerFence::accepts/);
const admission = access.slice(access.indexOf('bool peerAllowed('),access.indexOf('bool allowed('));
assert.doesNotMatch(admission,/HTTP_GET|HTTP_HEAD|request->method/,
  'Pinned GET/HEAD are not exempt: ISP probe affects hardware and readback is boot-bound');
const firmware=read('firmware/esp/application.ino');
assert.match(firmware,/webAccess.configurePeerFence\(pcBoardId,\s*pcBootId,\s*hostname\)/,
  'Peer fences keep exact board/boot identity while HTTP identity receives the configured hostname');
assert.doesNotMatch(firmware,/webAccess.configurePeerFence\(hostname,/,
  'A configurable hostname must not replace the stable board identity');
assert.match(firmware,/http_peer_fencing/);
const dispatchAuth = firmware.slice(firmware.indexOf('bool authenticate()'),firmware.indexOf('uint8_t snakeCell('));
assert.match(dispatchAuth,/!webAccess.peerAllowed\(server.request\(\)\)/,
  'Recheck the actual async continuation at loop dispatch before hardware access');
const probe = firmware.slice(firmware.indexOf('server.on("/api/avr/probe"'),firmware.indexOf('server.on("/api/avr/backup"'));
assert.ok(probe.indexOf('if (!authenticate()) return') < probe.indexOf('avrIsp.probe('),
  'Probe must not touch SPI/reset before dispatch authentication/fencing');
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
// Cell mutations moved from a local per-cell POST to the shared atomic batch
// path. Check its actual fresh-peer pin handoff and ACK/readback contract, not
// the obsolete local variable spelling from that removed implementation.
const batch=read('desktop/src/cell_batch.rs');
function checkTuiBatch(source,batchSource) {
  const start=source.indexOf('fn cell_request('),end=source.indexOf('fn command_allowed(');
  assert.ok(start>=0 && end>start);
  const cells=source.slice(start,end);
  assert.match(cells,/let current=blocking_status\(client,endpoint\)\?/);
  assert.match(cells,/if peer_key\(&current\)!=peer\{return Err/);
  assert.match(cells,/let pin=crate::peer_http::Pin::from_status\(&current\).ok_or\("Verified peer required"\)\?/);
  assert.match(cells,/crate::cell_batch::Request::new\(ids,state,"fade"\)\?/);
  assert.match(cells,/crate::cell_batch::send\(client,endpoint,&request,Some\(&pin\),Some\(stop\),None\)/);
  assert.doesNotMatch(cells,/\.post\(|for .*\bids\b/,'No unfenced scalar fallback');
  assert.match(batchSource,/expected.is_some_and\(\|expected\|expected!=&pin\) \|\| !Request::supported\(&?current\)/);
  assert.match(batchSource,/let pinned=pin.client\(\)\?/);
  assert.match(batchSource,/let response=pinned.post\(format!\("\{\}\/api\/cells"/);
  assert.match(batchSource,/receipt\["accepted"\].as_bool\(\)!=Some\(true\) \|\| receipt\["count"\].as_u64\(\)!=Some\(request.ids.len\(\) as u64\)/);
  assert.match(batchSource,/Pin::from_status\(&observed\).as_ref\(\)!=Some\(&pin\) \|\| !request.verify\(&observed\)/);
}
checkTuiBatch(tui,batch);
assert.throws(()=>checkTuiBatch(tui.replace('if peer_key(&current)!=peer','if false'),batch));
assert.throws(()=>checkTuiBatch(tui.replace('send(client,endpoint,&request,Some(&pin),Some(stop),None)','send(client,endpoint,&request,None,Some(stop),None)'),batch));
assert.throws(()=>checkTuiBatch(tui,batch.replace('let response=pinned.post','let response=client.post')));
assert.throws(()=>checkTuiBatch(tui,batch.replace(' || !request.verify(&observed)','')));
const native=read('desktop/src/main.rs');
assert.match(native,/if self\.command_route\(\) == Some\(Transport::WiFi\)[\s\S]*?self\.configuration_reads.take_due\(\)/,
  'Bounded optional settings dispatch must wait for fresh selected Wi-Fi status, not add offline preflight incidents');
// Remote progress now arrives in the ordinary, peer-bound status/snapshot.
// Do not require the deleted Firmware-page HTTP polling condition. Check the
// actual owner gates and both observation paths instead, including negatives.
function checkRemoteObservation(source) {
  const owner=source.indexOf('let Some(event) = current_event(event, &self.endpoint(), &self.serial_port)');
  const scoped=source.indexOf('Event::ForScope(scope, inner) if scope.current() => *inner',owner);
  const compact=source.replace(/\s+/g,'');
  const compactOwner=compact.indexOf('letSome(event)=current_event(event,&self.endpoint(),&self.serial_port)');
  const compactScoped=compact.indexOf('Event::ForScope(scope,inner)ifscope.current()=>*inner',compactOwner);
  const network=compact.indexOf('self.device_settings.remote_job=maintenance::parse_job(s.remote_firmware.clone()).ok()',compactScoped);
  const usb=compact.indexOf('self.device_settings.remote_job=maintenance::parse_job(snapshot.status.remote_firmware.clone()).ok()',network);
  assert.ok(owner>=0 && scoped>owner && compactOwner>=0 && compactScoped>compactOwner && network>compactScoped && usb>network,
    'Remote progress must follow selected route/scope ownership on both transports');
  assert.doesNotMatch(source,/self\.device_settings\.page\s*==\s*9[\s\S]{0,700}Operation::RemoteStatus/,
    'Do not restore per-page remote-job polling ahead of realtime status');
}
checkRemoteObservation(native);
assert.throws(()=>checkRemoteObservation(native.replace('let Some(event) = current_event(event, &self.endpoint(), &self.serial_port)','let Some(event) = unbound_event(event)')));
assert.throws(()=>checkRemoteObservation(native.replace('snapshot.status.remote_firmware.clone()','unrelated_job.clone()')));
console.log('Peer fence wiring guards passed (source assertions, not hardware/network acceptance).');
