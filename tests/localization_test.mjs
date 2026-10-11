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
const ctx=vm.createContext({
  NodeFilter:{SHOW_TEXT:4},navigator:{languages:['fa-IR']},
  localStorage:{getItem:k=>memory.get(k),setItem(k,v){writes++;memory.set(k,v)}},
  document:{documentElement:{},getElementById:id=>elements[id],querySelectorAll:()=>[search],
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
run('initializeLocalization()');
assert.equal(owned.nodeValue,' اتصال ');
assert.equal(peer.nodeValue,'Connect','Peer text identical to a dictionary key is never translated');
assert.equal(attrs.placeholder,'جست‌وجوی تنظیمات…');
run('saveLocalization()');assert.equal(ctx.document.documentElement.lang,'fa');assert.equal(ctx.document.documentElement.dir,'rtl');
const first=writes;run('saveLocalization()');assert.equal(writes,first,'No identical storage writes');
elements.uiLanguage.value='en';run('saveLocalization()');assert.equal(run("uiTr('Connect')"),'Connect');
assert.equal(owned.nodeValue,' Connect ');assert.equal(attrs.placeholder,'Search settings…');
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
