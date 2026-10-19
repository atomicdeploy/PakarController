// One truthful loopback peer for built-CLI fixtures, not a device deployment.
import assert from 'node:assert/strict';
import {toolRelease,toolPolicy} from '../tools/peer-identity.mjs';
export const cliBoard='cli-fixture-board',cliBoot='a'.repeat(32);
export function cliStatus({hostname='fixture-board',...additional}={}) {
  return {version:toolRelease.version,board_id:cliBoard,boot_id:cliBoot,http_peer_fencing:true,hostname,
    compatibility:{enforced:toolPolicy.enforce,minimum_app:toolPolicy.minimum_app,minimum_board:toolPolicy.minimum_board,maximum_app:toolPolicy.maximum_app,maximum_board:toolPolicy.maximum_board},
    capabilities:{board_kind:'controller',hostname,acknowledged_features:['network']},...additional};
}
export function cliHeaders(res) {
  for(const [key,value]of Object.entries({Server:'PakarController/'+toolRelease.version,'Content-Type':'application/json',
    'X-Pakar-Version':toolRelease.version,'X-Pakar-Board-Version':toolRelease.version,'X-Pakar-Build':String(toolRelease.build)}))res.setHeader(key,value);
}
export function assertCliIdentity(req,pinned=false) {
  assert.equal(req.headers['x-pakar-client'],'pakarcontroller-native');assert.equal(req.headers['x-pakar-version'],toolRelease.version);
  assert.match(req.headers['user-agent'],new RegExp('^PakarController/'+toolRelease.version.replaceAll('.','\\.')+' '));
  assert.match(req.headers['x-pakar-client-id'],/^[a-f0-9-]{36}$/);assert(req.headers['x-pakar-build']);
  if(pinned){assert.equal(req.headers['x-pakar-board'],cliBoard);assert.equal(req.headers['x-pakar-boot'],cliBoot);}
}
