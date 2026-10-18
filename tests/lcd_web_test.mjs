import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const lcd={configured:true,started:true,detection_known:true,backpack_ack:true,initialized:true,
  available:true,columns:16,rows:2,address:0x27,lines:['ledboard        ','Cell 1 white    ']};
const root={textContent:'old'},context=vm.createContext({document:{getElementById:()=>root}});
for(const name of ['lcdObservation','renderLcd'])vm.runInContext(source.match(new RegExp('^function '+name+'\\(.*$','m'))[0],context);
context.status={lcd};
assert(vm.runInContext('lcdObservation(status)',context));
vm.runInContext('renderLcd(status)',context);assert.equal(root.textContent,lcd.lines.join('\n'));
for(const key of ['configured','started','detection_known','backpack_ack','initialized','available']){
 context.status={lcd:{...lcd,[key]:false}};
 assert.equal(vm.runInContext('lcdObservation(status)',context),null);
 vm.runInContext('renderLcd(status)',context);assert.equal(root.textContent,'');
}
context.status={lcd:{...lcd,lines:['<script>']}};assert.equal(vm.runInContext('lcdObservation(status)',context),null);
assert.match(source,/data-capability="lcd" hidden/);
assert.match(source,/if\(!lcdObservation\(s\)\)features.delete\('lcd'\)/);
console.log('Actual Web LCD preview: individual ACK gates, bounded complete frame, clearing unavailable output and no innerHTML injection passed.');
