// Actual RayanLamp i18n.test.tsx contracts adapted to the shipped Pakar module.
// Node/VM replaces Preact/Vitest; protocol names and saved drafts are not UI copy.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import vm from 'node:vm';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const source=read('assets/web/rayanlamp.js')+'\n'+read('assets/web/localization.js');
class Events {
  listeners=new Map();
  addEventListener(type,listener){if(!this.listeners.has(type))this.listeners.set(type,new Set());this.listeners.get(type).add(listener)}
  removeEventListener(type,listener){this.listeners.get(type)?.delete(listener)}
  emit(type,event={}){for(const listener of [...this.listeners.get(type)||[]])listener(event)}
}
function fixture({languages=['en-US'],language='system',direction='auto',blocked=false}={}){
  const events=new Events(),storage=new Map([['pakar-locale',language],['pakar-direction',direction]]);
  let writes=0,requests=0;
  const owned={nodeValue:' Connect ',isConnected:true,parentElement:{closest:()=>false}};
  const peer={nodeValue:'Connect',isConnected:true,parentElement:{closest:()=>true}};
  const protocol={value:'http://ledboard.local:8118/api',dir:'ltr'};
  const draft={value:'Secret ۱۲۳'};
  const controls={uiLanguage:{value:language},uiDirection:{value:direction},protocol,draft};
  const root={dataset:{}};
  const context=vm.createContext({window:events,navigator:{languages},NodeFilter:{SHOW_TEXT:4},
    localStorage:{getItem:key=>{if(blocked)throw Error('blocked');return storage.get(key)},setItem(key,value){writes++;if(blocked)throw Error('blocked');storage.set(key,value)}},
    document:{documentElement:root,title:'ACK Board Connect · ledboard.local',getElementById:id=>controls[id],querySelectorAll:()=>[],
      createTreeWalker(){let index=0;return {nextNode(){this.currentNode=[owned,peer][index++];return !!this.currentNode}}}},
    fetch(){requests++;throw Error('Locale must not contact board')}
  });
  const run=code=>vm.runInContext(code,context);
  run(source);run('initializeLocalization()');
  return {run,context,events,storage,owned,peer,protocol,draft,controls,root,get writes(){return writes},get requests(){return requests},
    change(langs){context.navigator.languages=langs;events.emit('languagechange')},
    select(lang,dir='auto'){controls.uiLanguage.value=lang;controls.uiDirection.value=dir;run('saveLocalization()')}};
}
test('Actual donor normalization and Persian-only detection with Pakar primary-language policy',()=>{
  const f=fixture();
  for(const [value,expected]of [['system','system'],['en','en'],['fa','fa'],[undefined,'system'],['auto','system'],['fa-IR','system']])assert.equal(f.run('normalizeUiLanguagePreference('+JSON.stringify(value)+')'),expected);
  assert.equal(f.run("resolveUiLanguage('system',['fa-IR','en-US'])"),'fa');
  assert.equal(f.run("resolveUiLanguage('system',['ar','en-US'])"),'en');
  assert.equal(f.run("resolveUiLanguage('system',['en-US','fa-IR'])"),'en');
});
test('Actual donor translation placeholders, number formatting and visual digits are executable',()=>{
  const f=fixture({language:'fa'});
  assert.equal(f.root.lang,'fa');assert.equal(f.root.dir,'rtl');assert.equal(f.root.dataset.resolvedLocale,'fa');
  assert.equal(f.run("uiTr('Connect')"),'اتصال');
  assert.equal(f.run("uiTr('{count} selected',{count:'سه'})"),'سه انتخاب‌شده');
  assert.equal(f.run("uiTr('Unknown {date} {missing}',{date:'امروز'})"),'Unknown امروز {missing}');
  assert.equal(f.run("uiTr('{toString}',{})"),'{toString}','Inherited properties are not placeholders');
  assert.match(f.run('uiNumber(123456)'),/[۱۲۳۴۵۶]/);
  assert.equal(f.run("uiDigits('Port 47831')"),'Port ۴۷۸۳۱');
  assert.equal(f.protocol.value,'http://ledboard.local:8118/api');assert.equal(f.protocol.dir,'ltr');
});
test('Automatic locale reacts to OS changes without preference writes, peer translation or draft replacement',()=>{
  const f=fixture();f.change(['fa-IR']);
  assert.equal(f.root.dir,'rtl');assert.equal(f.owned.nodeValue,' اتصال ');assert.equal(f.peer.nodeValue,'Connect');
  assert.equal(f.context.document.title,'ACK Board Connect · ledboard.local');assert.equal(f.draft.value,'Secret ۱۲۳');
  assert.equal(f.writes,0);assert.equal(f.requests,0);assert.equal(f.storage.get('pakar-locale'),'system');
  f.change(['en-US']);assert.equal(f.owned.nodeValue,' Connect ');
});
test('Explicit language and direction stay stable; saved selection skips identical writes',()=>{
  const f=fixture();f.select('fa','ltr');f.change(['en-US']);assert.equal(f.root.lang,'fa');assert.equal(f.root.dir,'ltr');
  const writes=f.writes;f.select('fa','ltr');assert.equal(f.writes,writes);
  f.select('en','rtl');f.change(['fa-IR']);assert.equal(f.root.lang,'en');assert.equal(f.root.dir,'rtl');
});
test('Restart and idempotent teardown remove all old listeners while preserving the new owner',()=>{
  const f=fixture();const old=f.run('stopUiLocaleController'),next=f.run('startUiLocaleController()');old();old();
  assert.equal(f.events.listeners.get('languagechange').size,1);assert.equal(f.events.listeners.get('storage').size,1);
  f.run('initializeLocalization()');assert.equal(f.run('ownedCopy.length'),1,'Restart must not snapshot translated captions');
  next();const stop=f.run('stopUiLocaleController');assert.equal(f.events.listeners.get('languagechange').size,1);
  stop();stop();assert.equal(f.events.listeners.get('languagechange').size,0);assert.equal(f.events.listeners.get('storage').size,0);
  f.change(['fa-IR']);assert.equal(f.root.lang,'en');
});
test('Storage-restricted sessions still change language; dynamic owned labels and numbers are reversible',()=>{
  const f=fixture({blocked:true});f.run("var label={isConnected:true};setUiText(label,'Station SSID');var progress={isConnected:true};setUiText(progress,'Verified {sent} cell states on the device',{sent:12})");
  f.select('fa');assert.equal(f.run('label.textContent'),'نام شبکهٔ ایستگاه');assert.equal(f.run('progress.textContent'),'وضعیت ۱۲ خانه در دستگاه تأیید شد');
  f.select('en');assert.equal(f.run('label.textContent'),'Station SSID');assert.equal(f.run('progress.textContent'),'Verified 12 cell states on the device');
  f.run('label.isConnected=false;applyLocalization()');assert.equal(f.run('ownedDynamicCopy.size'),1,'Detached labels are not leaked');
  assert.equal(f.requests,0);
});
test('Generated embedding, actual dynamic consumers and logical grid ownership stay coordinated',()=>{
  const web=read('firmware/esp8266/src/WebUi.h');
  assert.equal(web.split('// BEGIN SHARED LOCALIZATION')[1].split('// END SHARED LOCALIZATION')[0].trim(),read('assets/web/localization.js').trim());
  assert(web.includes("setUiText(label,title)"));assert(web.includes('setUiText(text,label)'));
  assert(web.includes("[uiLanguage,panels.map(p=>p.dataset.panelId),panelPreferences]"));
  assert(web.includes("uiTr('{count} selected',{count:uiNumber(selectedCells.size)})"));
  assert(web.includes('.grid,.segment-display{direction:ltr}'));
});
