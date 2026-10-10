// First four tests port the actual Pealayer web_ui/scripts/appearance.test.mjs
// contracts. Browser tests execute the shipped embedded code, not a replacement.
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {accentForeground,mergeAppearance,paletteName,resolvedAccent,resolvedAppearanceTheme} from '../assets/themes/appearance.js';

test('Neutral helper default is retained; product supplies its explicit Studio default',()=>{
  assert.equal(paletteName(null),'native');assert.equal(paletteName({}),'native');
  assert.equal(paletteName({color_palette:'studio'}),'studio');
});
test('shared preview merges preferences, preserves unrelated settings and excludes resolution fields',()=>{
  const original={endpoint:'http://ledboard.local',color_palette:'native'};
  const appearance={theme:'dark',color_palette:'studio',accent_color:'pealayer_green',custom_accent_color:null,resolved_theme:'dark',resolved_accent:'#38d27a'};
  const next=mergeAppearance(original,appearance);
  assert.equal(next.endpoint,original.endpoint);assert.equal(next.color_palette,'studio');
  assert.equal(next.accent_color,'pealayer_green');assert.equal(next.resolved_accent,undefined);
  assert.equal(next.resolved_theme,undefined);assert.equal(original.color_palette,'native');
  assert.equal(resolvedAccent(null,next,appearance),'#38d27a');assert.equal(accentForeground('#38d27a'),'#141414');
});
test('System theme and accent preserve donor host resolution and offline fallbacks',()=>{
  const appearance={resolved_theme:'dark',resolved_accent:'#9e5cf4'};
  assert.equal(resolvedAppearanceTheme('system',true,appearance),'dark');
  assert.equal(resolvedAppearanceTheme('light',false,appearance),'light');
  assert.equal(resolvedAppearanceTheme('system',true),'light');
  assert.equal(resolvedAccent({accentColor:'#0078d4'},{accent_color:'system'},appearance),'#9e5cf4');
  assert.equal(resolvedAccent(null,{accent_color:'custom',custom_accent_color:'#123456'},appearance),'#123456');
});
test('live host theme events leave System preference unchanged and respect explicit light/dark',()=>{
  const config={theme:'system',color_palette:'native',accent_color:'system'};
  for(const scheme of ['light','dark','light']){
    const event={...config,resolved_theme:scheme,resolved_accent:'#6362c7'},synced=mergeAppearance(config,event);
    assert.equal(synced.theme,'system');assert.equal(resolvedAppearanceTheme(synced.theme,scheme!=='light',event),scheme);
    assert.equal(resolvedAppearanceTheme('light',false,event),'light');assert.equal(resolvedAppearanceTheme('dark',true,event),'dark');
  }
});

const web=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
const shipped=web.match(/<script>([\s\S]*?)<\/script>/)[1];
const code=shipped.slice(shipped.indexOf('// BEGIN SHARED APPEARANCE'),shipped.indexOf('// END BROWSER APPEARANCE'));
const catalog=JSON.parse(readFileSync(new URL('../assets/themes/palettes.json',import.meta.url),'utf8'));
function fixture({stored=null,light=false,storageFailure=false}={}){
  const ids=['appearanceTheme','appearancePalette','appearanceAccent','appearanceCustom','appearanceDialog','appearanceForm','appearanceOpen','appearanceClose','appearanceCancel','appearanceError'];
  const nodes=Object.fromEntries(ids.map(id=>[id,{value:'',disabled:false,open:false,textContent:'',listeners:{},addEventListener(k,v){this.listeners[k]=v},showModal(){this.open=true},close(){this.open=false}}]));
  const properties=new Map(),root={dataset:{},style:{setProperty:(key,value)=>properties.set(key,value)}};
  const media={matches:light,addEventListener(k,v){this.listener=v}},events={},storage=new Map(stored===null?[]:[['pakar-appearance',stored]]);
  let writes=0;
  const context=vm.createContext({...nodes,document:{documentElement:root},matchMedia:()=>media,addEventListener:(key,fn)=>events[key]=fn,
    localStorage:{getItem:key=>{if(storageFailure)throw Error('blocked');return storage.get(key)},setItem:(key,value)=>{if(storageFailure)throw Error('blocked');writes++;storage.set(key,value)}}});
  vm.runInContext(code,context);
  return{...nodes,root,properties,media,events,storage,get writes(){return writes},run:source=>vm.runInContext(source,context)};
}
test('embedded catalog exactly matches all four actual palettes and all fourteen roles',()=>{
  const f=fixture();assert.deepEqual(JSON.parse(f.run('JSON.stringify(PEALAYER_PALETTES)')),catalog);
  for(const name of ['studio','native'])for(const theme of ['light','dark']){
    f.run(`browserAppearance={...appearanceDefaults,color_palette:'${name}',theme:'${theme}'};applyBrowserAppearance()`);
    assert.equal(f.root.dataset.theme,theme);assert.equal(f.root.dataset.palette,name);
    for(const[role,value]of Object.entries(catalog[name][theme]))assert.equal(f.properties.get('--'+role),value);
  }
  assert.equal(f.writes,0,'Rendering theme must not write preferences or hardware');
});
test('dialog draft/cancel never changes applied appearance; apply uses contrasting accent text and skips identical writes',()=>{
  const f=fixture();f.appearanceOpen.listeners.click();assert.equal(f.appearanceDialog.open,true);
  f.appearanceTheme.value='light';f.appearancePalette.value='native';f.appearanceAccent.value='pealayer_green';
  f.appearanceCancel.listeners.click();assert.equal(f.root.dataset.theme,'dark');assert.equal(f.writes,0);
  f.appearanceOpen.listeners.click();f.appearanceTheme.value='light';f.appearancePalette.value='native';f.appearanceAccent.value='pealayer_green';
  f.appearanceForm.listeners.submit({preventDefault(){}});
  assert.equal(f.root.dataset.theme,'light');assert.equal(f.root.dataset.palette,'native');
  assert.equal(f.properties.get('--accent'),'#38d27a');assert.equal(f.properties.get('--accent-text'),'#141414');
  assert.equal(f.appearanceDialog.open,false);assert.equal(f.writes,1);
  f.appearanceForm.listeners.submit({preventDefault(){}});assert.equal(f.writes,1);
});
test('System browser events repaint without rewriting saved System preference; forced theme is stable',()=>{
  const f=fixture({stored:JSON.stringify({theme:'system',color_palette:'studio',accent_color:'windows_blue'})});
  assert.equal(f.root.dataset.theme,'dark');f.media.matches=true;f.media.listener();assert.equal(f.root.dataset.theme,'light');
  assert.equal(JSON.parse(f.storage.get('pakar-appearance')).theme,'system');assert.equal(f.writes,0);
  f.run("browserAppearance={...browserAppearance,theme:'dark'};applyBrowserAppearance()");
  f.media.listener();assert.equal(f.root.dataset.theme,'dark');assert.equal(f.writes,0);
});
test('invalid storage and CSS-like values cannot enter palette/accent roles; cross-tab update never echoes a write',()=>{
  const f=fixture({stored:'{broken'});assert.equal(f.root.dataset.palette,'studio');
  f.events.storage({key:'unrelated',newValue:'{}'});assert.equal(f.root.dataset.theme,'dark');
  f.events.storage({key:'pakar-appearance',newValue:JSON.stringify({theme:'light',color_palette:'native',accent_color:'custom',custom_accent_color:'#123456'})});
  assert.equal(f.properties.get('--accent'),'#123456');assert.equal(f.root.dataset.theme,'light');
  f.events.storage({key:'pakar-appearance',newValue:JSON.stringify({theme:'invalid',color_palette:'url(evil)',accent_color:'custom',custom_accent_color:'red;background:url(evil)',resolved_theme:'light'})});
  assert.equal(f.root.dataset.palette,'studio');assert.equal(f.root.dataset.theme,'dark');assert.equal(f.properties.get('--accent'),'#0078d4');
  assert.equal(f.writes,0);f.events.storage({key:'pakar-appearance',newValue:null});assert.equal(f.root.dataset.theme,'dark');
});
test('unavailable storage still applies for this session and visibly reports failure',()=>{
  const f=fixture({storageFailure:true});f.appearanceOpen.listeners.click();f.appearanceTheme.value='light';
  f.appearanceForm.listeners.submit({preventDefault(){}});assert.equal(f.root.dataset.theme,'light');
  assert.match(f.appearanceError.textContent,/session only/);assert.equal(f.appearanceDialog.open,true);
});
test('theme control labels and palette-safe cells retain fixed geometry, half-row offset and amber search priority',()=>{
  assert.match(web,/aria-labelledby="appearanceTitle"/);assert.match(web,/Saved only in this browser/);
  assert.match(web,/transform:translateY\(50%\);--cell-surface:var\(--surface-0\)/);
  assert.match(web,/\.cell\{[^}]*border:2px solid var\(--line-strong\)/);
  assert.match(web,/\.cell\.matched\.selected,\.cell\.matched:hover/);
  assert.doesNotMatch(web,/background:#(?:3a222b|214239|332a15)/);
});
