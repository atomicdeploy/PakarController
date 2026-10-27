import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {check,violations} from '../tools/check-living-contract.mjs';
import {manifest} from '../tools/create-remote-manifest.mjs';
assert(check()>50);
for(const source of ['fetch("/api/v1/status")','fetch("/api/v12")','{schemaVersion:1}',
  '{"api_version":1}','kEnvelopeSchema = 1','"pakarcontroller-v2"','"PKR1"',
  '{"schema":1}','{"schema":null}','d[F("schema")]=1','value.schema===1','apiVersion: 2']) {
  assert(violations('fixture',source).length,'Rejected fixture: '+source);
}
for(const source of ['"/api/status"','{"version":"0.2.7","revision":2,"acknowledged_features":["display"]}',
  'record_.schema = 2;', '{"jsonrpc":"2.0"}'])assert.equal(violations('fixture',source).length,0);
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const access=read('firmware/esp/src/WebAccess.h');
assert(access.indexOf('ApiPolicy::numberedRoute')<access.indexOf('publicRead'));
assert.match(access,/request->send\(404/);
assert(!/redirect.*\/api/.test(access));
const esp=Buffer.alloc(32);esp[0]=0xe9;esp[1]=2;
const avr=Buffer.alloc(32768,0xff);avr[0]=0x0c;avr[1]=0x94;avr[32767]=0;
const value=manifest(esp,avr,'https://updates.example.test/','0.2.7','busy-wire');
assert.equal(value.profile,'pakar-nodemcu-atmega328p');
for(const key of ['schema','schemaVersion','api_version','protocol'])assert(!Object.hasOwn(value,key));
assert.equal(value.avr.signature,'1E950F');
assert.equal(value.avr.bootloader_included,true);
assert.equal(value.esp.sha256.length,64);
console.log('Living-contract guard negative fixtures, unversioned remote manifest and pre-auth 404 wiring passed. Not a physical-board test.');
