// Adapted from RayanLamp haptics.test.ts and connection-preferences.test.ts.
// Node's built-in assertions replace Vitest; no additional toolchain/cache.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const source=read('assets/web/rayanlamp.js'),web=read('firmware/esp/src/WebUi.h');
assert.equal(web.split('// BEGIN RAYANLAMP ADAPTATIONS')[1].split('// END RAYANLAMP ADAPTATIONS')[0].trim(),source.trim());
const ctx=vm.createContext({});
vm.runInContext(source,ctx);
const run=code=>vm.runInContext(code,ctx);
run("var calls=[];var runtime={vibrate:p=>{calls.push(p);return true},isDocumentVisible:()=>true,prefersReducedMotion:()=>false};var h=new WebHaptics(runtime)");
assert.equal(run("h.supported"),true);
for(const cue of ['command.start','command.success','command.error','message.received'])
  assert.equal(run("h.trigger("+JSON.stringify(cue)+")"),true);
run("h.audioCue(2);h.audioCue(1000)");
assert.deepEqual(JSON.parse(run('JSON.stringify(calls)')),[12,[15,35,24],[28,42,28],[18,55,18],[20,65,20],[20,65,20,65,20]]);
for(const overrides of [
  '{vibrate:undefined}', '{isDocumentVisible:()=>false}', '{prefersReducedMotion:()=>true}',
  '{isDocumentVisible:()=>{throw Error()}}', '{prefersReducedMotion:()=>{throw Error()}}',
  '{vibrate:()=>{throw Error()}}'
])assert.equal(run("new WebHaptics({...runtime,..."+overrides+"}).trigger('command.success')"),false);
for(const invalid of ['0','NaN','Infinity','-1'])assert.equal(run('h.audioCue('+invalid+')'),false);
assert.equal(run("h.trigger('arbitrary raw frame')"),false,'JS also rejects cues the TypeScript type forbids');
run("new WebHaptics({...runtime,vibrate:p=>{if(Array.isArray(p))p[0]=999;return true}}).trigger('command.success')");
assert.equal(run('fixedHapticPatterns["command.success"][0]'),15);
assert.equal(run("commandHaptic('command.success')"),false,'Feedback is opt-in');
assert.equal(run("latinDigits('۰۱۲۳۴۵۶۷۸۹ / ٠١٢٣٤٥٦٧٨٩')"),'0123456789 / 0123456789');
const parser=web.split('\n').find(line=>line.startsWith('function parseSelectionQuery(query)'));
vm.runInContext(parser,ctx);
assert.deepEqual(JSON.parse(run("JSON.stringify(parseSelectionQuery('۱،٢;۳–۵ #۸۰'))")),[1,2,3,4,5,80]);
for(const bad of ['۰','۸۱','٤-٢','۹۹۹','۱a'])assert.throws(()=>run('parseSelectionQuery('+JSON.stringify(bad)+')'));

// Actual donor local-event + cross-tab delivery, hardened against invalid peers,
// blocked APIs, duplicate SSD writes, rebroadcast loops and leaked listeners.
class Events {
  listeners=new Map();
  addEventListener(name,fn){if(!this.listeners.has(name))this.listeners.set(name,new Set());this.listeners.get(name).add(fn);}
  removeEventListener(name,fn){this.listeners.get(name)?.delete(fn);}
  dispatchEvent(event){for(const fn of this.listeners.get(event.type)||[])fn(event);}
}
const channels=[],storage=new Map();let writes=0,broadcasts=0;
class Channel extends Events {
  constructor(){super();channels.push(this);}
  postMessage(value){broadcasts++;for(const other of channels)if(other!==this&&!other.closed)other.dispatchEvent({type:'message',data:value});}
  close(){this.closed=true;}
}
const host=new Events();
const prefs=vm.createContext({
  window:host,BroadcastChannel:Channel,
  CustomEvent:class{constructor(type,options){this.type=type;this.detail=options.detail;}},
  localStorage:{getItem:key=>storage.get(key),setItem(key,value){writes++;storage.set(key,value);}}
});
vm.runInContext(source,prefs);
const prefRun=code=>vm.runInContext(code,prefs);
prefRun('var received=[];var stop=subscribeLocalePreferences(value=>received.push(value));');
prefRun("announceLocalePreferences({language:'fa',direction:'auto'})");
assert.equal(prefRun('received.length'),2,'Local event and independent broadcast are both validated');
assert.equal(writes,2);assert.equal(broadcasts,1,'Receiver never echoes');
prefRun("announceLocalePreferences({language:'fa',direction:'auto'})");
assert.equal(writes,2,'No identical persistence writes');
const before=prefRun('received.length');
host.dispatchEvent({type:'pakar:locale-preferences',detail:{language:'invalid',direction:'rtl'}});
channels[0].dispatchEvent({type:'message',data:{language:'en',direction:'unsafe'}});
assert.equal(prefRun('received.length'),before);
storage.set('pakar-locale','en');host.dispatchEvent({type:'storage',key:'pakar-locale'});
assert.equal(prefRun('received.at(-1).language'),'en');
prefRun('stop()');assert.equal(channels[0].closed,true);
assert.equal(host.listeners.get('storage').size,0);
const stopped=prefRun('received.length');
host.dispatchEvent({type:'pakar:locale-preferences',detail:{language:'fa',direction:'auto'}});
assert.equal(prefRun('received.length'),stopped);
const blocked=vm.createContext({window:new Events(),BroadcastChannel:class{constructor(){throw Error();}},localStorage:{getItem(){throw Error();},setItem(){throw Error();}}});
vm.runInContext(source,blocked);
assert.equal(vm.runInContext("announceLocalePreferences({language:'fa',direction:'rtl'})",blocked),true);
// Real controls invoke feedback only for explicit commands, not status polling.
assert(web.includes('id="uiHaptics"'));
assert(web.includes("commandHaptic('command.success');await refresh()"));
assert(!web.split('\n').find(line=>line.startsWith('async function refresh()')).includes('commandHaptic'));
assert(read('desktop/src/ui/cell_search.rs').includes('super::digits::latin(query)'));
console.log('RayanLamp haptics, digit input and preference-channel adaptation tests passed.');
