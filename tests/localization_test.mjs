import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {gunzipSync} from 'node:zlib';
import vm from 'node:vm';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const font=readFileSync(new URL('../assets/fonts/Vazirmatn-Regular.ttf',import.meta.url));
const fontHeader=read('firmware/esp8266/src/FontAssets.h');
const gzip=Buffer.from(fontHeader.split('PROGMEM = {')[1].split('};')[0].match(/\d+/g).map(Number));
assert.deepEqual(gunzipSync(gzip),font,'ESP serves the real shared font, not an empty placeholder');
assert.deepEqual(readFileSync(new URL('../mobile/assets/fonts/Vazirmatn-Regular.ttf',import.meta.url)),font);
const web=read('firmware/esp8266/src/WebUi.h'),js=read('assets/web/localization.js');
assert.equal(web.split('// BEGIN SHARED LOCALIZATION')[1].split('// END SHARED LOCALIZATION')[0].trim(),js.trim());
assert(web.includes("src:url('/assets/fonts/Vazirmatn-Regular.ttf')"));
assert(read('firmware/esp8266/src/WebAccess.h').includes('path == F("/assets/fonts/Vazirmatn-Regular.ttf")'));
assert(read('firmware/esp8266/src/WebAccess.h').includes('path == F("/assets/fonts/Vazirmatn-OFL.txt")'));
assert(read('firmware/esp8266/esp8266.ino').includes('"text/plain; charset=utf-8",kVazirmatnLicense'));
const memory=new Map();let writes=0;
const elements={uiLanguage:{value:'fa'},uiDirection:{value:'auto'}};
const owned={nodeValue:' Connect ',isConnected:true,parentElement:{closest:()=>false}};
const peer={nodeValue:'Connect',isConnected:true,parentElement:{closest:()=>true}};
const attrs={placeholder:'Search settings…','aria-label':'Search device settings'};
const search={isConnected:true,closest:()=>false,getAttribute:k=>attrs[k],setAttribute(k,v){attrs[k]=v}};
const body={querySelectorAll:selector=>selector==='option:not([value])'?[]:[search]};
const ctx=vm.createContext({
  NodeFilter:{SHOW_TEXT:4},navigator:{languages:['fa-IR']},
  localStorage:{getItem:k=>memory.get(k),setItem(k,v){writes++;memory.set(k,v)}},
  document:{body,documentElement:{},getElementById:id=>elements[id],querySelectorAll:()=>[search],
    createTreeWalker(){let i=0;return {nextNode(){this.currentNode=[owned,peer][i++];return !!this.currentNode}}}
  }
});
vm.runInContext(read('assets/web/rayanlamp.js')+'\n'+js,ctx);
const run=code=>vm.runInContext(code,ctx);
assert.equal(run("resolveUiLanguage('system',['fa-IR'])"),'fa');
assert.equal(run("resolveUiLanguage('en',['fa-IR'])"),'en');
assert.equal(run("resolveUiDirection('auto','fa')"),'rtl');
assert.equal(run("resolveUiDirection('ltr','fa')"),'ltr');
assert.equal(run("uiTr('Connect')"),'اتصال');
assert.equal(run("uiTr('رلهٔ اصلی ۱۲۳ — ledboard.local')"),'رلهٔ اصلی ۱۲۳ — ledboard.local');
assert.equal(run("uiTr('constructor')"),'constructor','Unknown labels cannot resolve through Object.prototype');
run('initializeLocalization()');
assert.equal(owned.nodeValue,' اتصال ');
assert.equal(peer.nodeValue,'Connect','Peer text identical to a dictionary key is never translated');
assert.equal(attrs.placeholder,'جست‌وجوی تنظیمات…');
run('saveLocalization()');assert.equal(ctx.document.documentElement.lang,'fa');assert.equal(ctx.document.documentElement.dir,'rtl');
const first=writes;run('saveLocalization()');assert.equal(writes,first,'No identical storage writes');
elements.uiLanguage.value='en';run('saveLocalization()');assert.equal(run("uiTr('Connect')"),'Connect');
assert.equal(owned.nodeValue,' Connect ');assert.equal(attrs.placeholder,'Search settings…');
// Exercise template and dynamic ownership, not only a dictionary-presence test.
const runtime={isConnected:true,textContent:'',setAttribute(k,v){this[k]=v},replaceChildren(n){this.child=n;this.textContent=n.textContent}};
ctx.runtime=runtime;ctx.document.createElement=()=>({style:{},setAttribute(){}});
run("setUiText(runtime,'Disconnected')");
elements.uiLanguage.value='fa';run('saveLocalization()');assert.equal(runtime.textContent,'قطع‌شده');
run("setUiProtocolText(runtime,'ledboard.local')");
elements.uiLanguage.value='en';run('saveLocalization()');
assert.equal(runtime.textContent,'ledboard.local','A real address replaces the previous translated status owner');
const attributeNode={isConnected:true,setAttribute(k,v){this[k]=v}};ctx.attributeNode=attributeNode;
run("setUiAttribute(attributeNode,'aria-label','Logical cell {id}',{id:80})");
elements.uiLanguage.value='fa';run('saveLocalization()');assert.equal(attributeNode['aria-label'],'خانهٔ منطقی ۸۰');
const fresh={nodeValue:' Language & input ',isConnected:true,parentElement:{closest:()=>false}};
ctx.document.createTreeWalker=()=>{let i=0;return {nextNode(){this.currentNode=[fresh][i++];return !!this.currentNode}}};
const option={value:'rainbow',setAttribute(k,v){this[k]=v}};
ctx.template={querySelectorAll:s=>s==='option:not([value])'?[option]:[]};
run('registerUiTemplate(template);applyLocalization()');
assert.equal(fresh.nodeValue,' زبان و ورودی ');assert.equal(option.value,'rainbow','Translating an option must never change its command value');
elements.uiLanguage.value='en';run('saveLocalization()');assert.equal(fresh.nodeValue,' Language & input ');
const controllerCopy=JSON.parse(read('assets/web/controller-copy.json'));
for(const key of ['Language & input','Apply preferences','Wi-Fi & Ethernet','IPv4 address','Private topic','Cell selection','Clear selection']){
  assert(controllerCopy[key],key);ctx.key=key;run("uiLanguage='fa'");assert.notEqual(run('uiTr(key)'),key);
}
assert(web.includes('html[lang=fa] .eyebrow'),'Persian script must not inherit Latin letter spacing');
for(const id of ['effect','pattern'])assert(!web.match(new RegExp('<select id="'+id+'">([^<]*<option>(?:[^<]*)</option>)+')),'Effect options require stable explicit values');
// Execute the real status renderer through repeated telemetry, not a UI stub.
ctx.state={avr_online:true,tm1637_ack:true,websocket_clients:2,station_connected:false,ap_started:false,
  mqtt_enabled:false,cloud_websocket_enabled:false,brightness:255};ctx.contextReceivedAt=1;
for(const name of ['avr','tm','clients','station','ap','discovery','ntp','uart','mqttState','cloudWsState'])ctx[name]={textContent:''};
vm.runInContext(web.slice(web.indexOf('function renderLocalizedStatus(){'),web.indexOf('function render(s){')),ctx);
run('renderLocalizedStatus()');assert.equal(ctx.station.textContent,'قطع‌شده');assert.equal(ctx.mqttState.textContent,'غیرفعال');
elements.uiLanguage.value='en';run('saveLocalization();renderLocalizedStatus()');assert.equal(ctx.station.textContent,'Disconnected');
elements.uiLanguage.value='fa';run('saveLocalization();renderLocalizedStatus()');assert.equal(ctx.station.textContent,'قطع‌شده');
assert(read('desktop/src/ui/i18n.rs').includes('contextual_persian_in_labels_and_monospace_inputs'));
// Adapt Pealayer tests/i18n_surface_audit_test.rs's actual ownership audit:
// forbid raw application captions at known shell boundaries, not peer data.
const shell=read('desktop/src/ui/shell.rs');
for(const key of ['File','Workspace','Device','Help'])assert(shell.includes('menu::top_menu_button(ui, crate::ui::i18n::text("'+key+'")'));
assert(!shell.includes('ui.button("Connect")'));
assert(js.includes('ownedAttributes'));
assert(read('desktop/src/ui/i18n.rs').includes('changing_language_preserves_widget_fonts'));
assert(read('desktop/src/ui/dialog.rs').includes('input_alignment'));
assert(read('desktop/src/config.rs').includes('configure_ui_fonts(ctx, resolved_language(config).is_rtl())'));
assert(read('mobile/pubspec.yaml').includes('family: Vazirmatn'));
console.log('Shared font bytes, Web Persian/RTL/locale/persistence, and native wiring checks passed; native shaping test still requires Cargo execution.');
