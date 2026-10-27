// Actual RayanLamp theme.test.ts scenarios, adapted to the shipped Pakar code.
// No Vitest, signal runtime, donor build or physical board connection.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import vm from 'node:vm';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const web=read('firmware/esp/src/WebUi.h');
const bootstrap=web.match(/<script data-pakar-presentation>([\s\S]*?)<\/script>/)[1];
const controller=read('assets/themes/browser-appearance.js');
assert.equal(bootstrap.split('// BEGIN BROWSER CONTROLLER')[1].split('// END BROWSER CONTROLLER')[0].trim(),controller.trim());
function fixture({appearance={theme:'system'},locale='system',direction='auto',languages=['en-US'],light=false,blocked=false,missingMedia=false}={}){
  const storage=new Map([['pakar-appearance',JSON.stringify(appearance)],['pakar-locale',locale],['pakar-direction',direction]]);
  let writes=0;
  const listeners=new Set(),events=new Map(),properties=new Map();
  const media={matches:light,addEventListener(type,fn){assert.equal(type,'change');listeners.add(fn)},removeEventListener(type,fn){assert.equal(type,'change');listeners.delete(fn)}};
  const nodes={scheme:{content:'dark'},color:{content:'#080a0e'}};
  const root={dataset:{},style:{setProperty:(key,value)=>properties.set(key,value)}};
  const context=vm.createContext({navigator:{languages},document:{documentElement:root,title:'Peer name stays unchanged',querySelector:s=>s.includes('color-scheme')?nodes.scheme:nodes.color},
    matchMedia:missingMedia?undefined:()=>media,
    localStorage:{getItem:key=>{if(blocked)throw Error('blocked');return storage.get(key)},setItem(){writes++;throw Error('Bootstrap must not write')}},
    addEventListener(type,fn){if(!events.has(type))events.set(type,new Set());events.get(type).add(fn)},
    removeEventListener(type,fn){events.get(type)?.delete(fn)}});
  vm.runInContext(bootstrap,context);
  return {root,nodes,properties,storage,listeners,events,get writes(){return writes},run:code=>vm.runInContext(code,context),
    change(value){media.matches=value;for(const fn of [...listeners])fn()},
    storageEvent(event){for(const fn of [...events.get('storage')??[]])fn(event)},title:()=>context.document.title};
}
test('RayanLamp system mode updates browser chrome metadata without rewriting saved preference',()=>{
  const f=fixture();assert.equal(f.root.dataset.themePreference,'system');assert.equal(f.nodes.scheme.content,'dark');assert.equal(f.nodes.color.content,'#080a0e');
  f.change(true);assert.equal(f.root.dataset.resolvedTheme,'light');assert.equal(f.root.style.colorScheme,'light');assert.equal(f.nodes.scheme.content,'light');assert.equal(f.nodes.color.content,'#edf1f5');
  assert.equal(JSON.parse(f.storage.get('pakar-appearance')).theme,'system');assert.equal(f.writes,0);
});
test('RayanLamp explicit override stays stable across system changes for every shared palette',()=>{
  for(const color_palette of ['studio','native'])for(const theme of ['dark','light']){
    const f=fixture({appearance:{theme,color_palette}});f.change(theme==='dark');
    assert.equal(f.root.dataset.resolvedTheme,theme);assert.equal(f.root.dataset.themePreference,theme);
    assert.equal(f.nodes.color.content,f.properties.get('--canvas'));assert.equal(f.writes,0);
  }
});
test('RayanLamp pre-paint bootstrap runs before CSS/body, uses one palette engine and makes no device calls',()=>{
  assert(web.indexOf('<script data-pakar-presentation>')<web.indexOf('<style>'));
  assert(web.indexOf('</script>')<web.indexOf('<body>'));
  assert.equal(web.split('const PEALAYER_PALETTES=').length,2);
  assert.doesNotMatch(bootstrap,/\b(?:fetch|WebSocket|XMLHttpRequest)\b|document\.title\s*=|\bimport\s/);
  const f=fixture({appearance:{theme:'light',color_palette:'native'},locale:'fa',direction:'ltr'});
  assert.equal(f.root.lang,'fa');assert.equal(f.root.dir,'ltr');assert.equal(f.root.dataset.resolvedLocale,'fa');assert.equal(f.properties.get('--canvas'),'#eeeeee');assert.equal(f.title(),'Peer name stays unchanged');
});
test('RayanLamp controller restart and idempotent teardown preserve the newest owner',()=>{
  const f=fixture();const oldStop=f.run('stopAppearanceController');const newStop=f.run('startBrowserAppearanceController()');
  assert.equal(f.listeners.size,1);assert.equal(f.events.get('storage').size,1);oldStop();oldStop();assert.equal(f.listeners.size,1);
  f.change(true);assert.equal(f.root.dataset.resolvedTheme,'light');newStop();newStop();assert.equal(f.listeners.size,0);assert.equal(f.events.get('storage').size,0);
  f.change(false);assert.equal(f.root.dataset.resolvedTheme,'light');assert.equal(f.writes,0);
});
test('Pre-paint survives blocked storage, missing OS API and malformed preferences',()=>{
  const blocked=fixture({blocked:true,languages:['fa-IR']});assert.equal(blocked.root.dataset.theme,'dark');assert.equal(blocked.root.lang,'fa');assert.equal(blocked.root.dir,'rtl');assert.equal(blocked.writes,0);
  const missing=fixture({missingMedia:true});assert.equal(missing.root.dataset.theme,'dark');
  const invalid=fixture({appearance:{theme:'fake',color_palette:'url(x)',custom_accent_color:'red;'},locale:'ar',direction:'sideways',languages:['ar']});
  assert.equal(invalid.root.lang,'en');assert.equal(invalid.root.dir,'ltr');assert.equal(invalid.root.dataset.palette,'studio');assert.equal(invalid.writes,0);
});
test('Cross-tab reset repaints metadata, malformed JSON preserves session and never echoes storage writes',()=>{
  const f=fixture();f.storageEvent({key:'pakar-appearance',newValue:JSON.stringify({theme:'light',color_palette:'native'})});assert.equal(f.nodes.color.content,'#eeeeee');
  f.storageEvent({key:'pakar-appearance',newValue:'{bad'});assert.equal(f.nodes.color.content,'#eeeeee');
  f.storage.delete('pakar-appearance');f.storageEvent({key:null});assert.equal(f.root.dataset.theme,'dark');assert.equal(f.nodes.color.content,'#080a0e');assert.equal(f.writes,0);
});
