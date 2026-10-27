// Canonical controls and localizer, with a tiny DOM contract fixture. Browser
// geometry is separately checked at 320/442/768/1280px; this is not visual proof.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
class Element {
  constructor(tag){this.tagName=tag.toUpperCase();this.nodeType=1;this.childNodes=[];this.attrs={};this.dataset={};this.listeners={};this.className='';this.isConnected=true;this.style={};this.classList={add:(...values)=>{this.className=[...new Set([...this.className.split(' ').filter(Boolean),...values])].join(' ');},contains:value=>this.className.split(' ').includes(value)};}
  get children(){return this.childNodes.filter(child=>child.nodeType===1);}
  get textContent(){return this.childNodes.map(child=>child.textContent).join('');}
  set textContent(text){this.childNodes=[];this.append({nodeType:3,textContent:String(text)});}
  append(...children){for(const child of children){if(child.parentElement)child.parentElement.childNodes=child.parentElement.childNodes.filter(item=>item!==child);child.parentElement=this;this.childNodes.push(child);}}
  prepend(child){this.append(child);this.childNodes.unshift(this.childNodes.pop());}
  appendChild(child){this.append(child);return child;}
  replaceChildren(...children){this.childNodes=[];this.append(...children);}
  setAttribute(key,value){this.attrs[key]=String(value);}
  getAttribute(key){return this.attrs[key]??null;}
  querySelector(query){if(query===':scope > svg')return this.children.find(child=>child.tagName==='SVG')??null;if([':scope > [data-control-label]','[data-control-label]'].includes(query))return this.children.find(child=>Object.hasOwn(child.attrs,'data-control-label'))??null;return null;}
  addEventListener(name,listener){this.listeners[name]=listener;}
  matches(){return this.classList.contains('cell');}
  closest(){return null;}
}
const context=vm.createContext({console,setTimeout,clearTimeout,Intl,localStorage:{getItem:()=>null},navigator:{language:'en'},validLocalePreferences:()=>true,
  document:{documentElement:{dataset:{}},createElement:tag=>new Element(tag),createElementNS:(namespace,tag)=>{const e=new Element(tag);e.namespaceURI=namespace;return e;}}});
vm.runInContext(read('web/modules/components.js'),context);
vm.runInContext(read('assets/web/localization.js'),context);
const ui=context.WebUI;
const legacy=new Element('button');legacy.textContent='Select all';legacy.setAttribute('onclick','selectAllCells()');
const originalText=legacy.childNodes[0],handler=()=>{};legacy.addEventListener('click',handler);
const root={querySelectorAll:query=>query==='button'?[legacy]:[]};
ui.enhanceControls(root);ui.enhanceControls(root);
assert.equal(legacy.children.length,2,'Hydration is idempotent');
assert.equal(legacy.children[1].childNodes[0],originalText,'Localizer-owned text nodes survive hydration');
assert.equal(legacy.listeners.click,handler,'Existing action handlers survive hydration');
const glyph=legacy.children[0];
assert.equal(glyph.getAttribute('viewBox'),'0 0 24 24');assert.equal(glyph.getAttribute('stroke'),'currentColor');
assert.equal(glyph.getAttribute('fill'),'none');assert.equal(glyph.getAttribute('aria-hidden'),'true');
assert.equal(glyph.getAttribute('width'),'18');assert.equal(glyph.getAttribute('height'),'18');
context.control=legacy;
vm.runInContext("setUiText(control,'Select all');uiLanguagePreference='fa';applyLocalization()",context);
assert.equal(legacy.children[0],glyph,'Language switching cannot erase decorative SVG');
assert.equal(legacy.children[1].textContent,'انتخاب همه');
vm.runInContext("uiLanguagePreference='en';applyLocalization()",context);
assert.equal(legacy.children[1].textContent,'Select all');
const modern=ui.button('Save',()=>{},{icon:'save'});context.control=modern;
vm.runInContext("setUiText(control,'Save');uiLanguagePreference='fa';applyLocalization()",context);
assert.equal(modern.getAttribute('aria-label'),'ذخیره','Accessible names track translated visible labels');
assert.equal(modern.children[0].tagName,'SVG');
const close=new Element('button');close.textContent='×';ui.decorateControl(close,'close',{iconOnly:true});
assert.equal(close.getAttribute('aria-label'),'Close','An unlabeled close glyph receives a meaningful name');
const primary=new Element('button');primary.className='primary';primary.textContent='Apply';ui.decorateControl(primary,'check');assert(primary.classList.contains('button--primary'),'Legacy primary actions retain their semantic emphasis');
const cell=new Element('button');cell.className='cell';cell.textContent='1';cell.setAttribute('onclick','setCell()');
ui.enhanceControls({querySelectorAll:query=>query==='button'?[cell]:[]});assert.equal(cell.children.length,0,'Pixel/box controls are not decorated as toolbar actions');
const peer=new Element('button');peer.textContent='Save';
ui.enhanceControls({querySelectorAll:query=>query==='button'?[peer]:[]});assert.equal(peer.children.length,0,'Peer-provided copy is not guessed as an application action');
const css=read('assets/web/studio.css'),surface=read('assets/themes/controller-surfaces.css');
assert(!surface.includes('flex:1 1 90px'),'No blanket row sizing stretches every action');
assert(css.includes('inline-size:var(--square-size);block-size:var(--square-size)'),'Chevron geometry has one shared square dimension');
assert(css.includes('.button-label{display:block;min-width:0;flex:0 1 auto;white-space:normal'),'Labels shrink/wrap rather than spill out');
assert(css.includes('.studio .top{flex-direction:row;flex-wrap:wrap;'),'Header owns its desktop direction instead of inheriting legacy column flow');
assert(css.includes('.studio .top{flex-direction:column;flex-wrap:nowrap;align-items:stretch}'),'Small headers cannot wrap into an offscreen second column');
assert(css.includes('.studio .top>.brand,.studio .top>.row{flex:0 0 auto;width:100%;min-width:0}'),'Column children never inherit a 100 percent height basis');
assert(css.includes('#studioNav button>span{max-width:100%;min-width:0;white-space:normal;'),'Long bottom-nav captions wrap inside their action');
assert(css.includes('prefers-reduced-motion:reduce'));
const appearance=read('assets/themes/browser-appearance.js');
assert(appearance.includes("accent_color:'pealayer_green'"));
assert(appearance.includes("includes(v.accent_color)?v.accent_color:appearanceDefaults.accent_color"),'Explicit saved accents remain authoritative');
const luminance=hex=>{const rgb=hex.match(/\w\w/g).map(n=>parseInt(n,16)/255).map(n=>n<=.04045?n/12.92:((n+.055)/1.055)**2.4);return rgb.reduce((n,c,i)=>n+c*[.2126,.7152,.0722][i],0);};
const contrast=(a,b)=>{const [bright,dark]=[luminance(a),luminance(b)].sort((a,b)=>b-a);return(bright+.05)/(dark+.05);};
assert(contrast('38d27a','141414')>=7,'Bright green solid actions retain strong dark text contrast');
assert(contrast('16753d','ffffff')>=4.5,'Light-theme semantic green remains legible');
const catalog=JSON.parse(read('assets/web/controller-copy.json'));
for(const key of ['Fit','Fit table to panel','Zoom in','Zoom out','Go to','Go to panel','Arrange panels','Done','Select {state}'])assert(catalog[key],key+' must have Persian copy');
assert(read('assets/web/workspace-studio.js').includes("setUiAttribute(b,'aria-label',label)"),'Collapsed rail actions retain localized accessible names');
const menu=new Element('div'),panel=new Element('section');panel.dataset={panelId:'table',panelLabel:'Table',capability:'table'};
context.workspacePanels=[panel];context.panelPreferences={};context.panelMenuSignature='';context.settingsNode=()=>menu;context.currentPeer=()=>({has:()=>true});context.panelToggle=()=>{};
vm.runInContext(read('assets/web/device-settings.js').split(/\r?\n/).find(line=>line.startsWith('function renderPanelMenu()')),context);
vm.runInContext('renderPanelMenu()',context);
assert.equal(menu.children[0].getAttribute('aria-checked'),'true');assert.equal(menu.children[0].dataset.controlIcon,'eyeOff');assert.equal(menu.children[1].dataset.controlIcon,'reset');
context.panelPreferences={table:{hidden:true}};vm.runInContext('renderPanelMenu()',context);
assert.equal(menu.children[0].getAttribute('aria-checked'),'false');assert.equal(menu.children[0].dataset.controlIcon,'eye');
const html=read('web/index.html');
vm.runInContext(html.split(/\r?\n/).find(line=>line.startsWith('function semanticCell(')),context);
const selectBody=html.match(/button\.onclick=\(\)=>\{(const cells=state\.cell_states\?\?state\.cells;[^\n]+?)\};tools\.appendChild\(button\)/)?.[1];assert(selectBody);
context.state={cell_states:Array.from({length:80},(_,i)=>i===0?'lost':i===1?'won':'idle'),state_presentation:{states:{lost:{rgb:0x00ff00},won:{rgb:0xff0000}}}};
context.name='lost';context.paintSelection=()=>{};vm.runInContext('(()=>{'+selectBody+'})()',context);
assert.deepEqual([...context.selectedCells],[1],'Selection follows lost semantic identity even when its configured color is green');
context.name='won';vm.runInContext('(()=>{'+selectBody+'})()',context);assert.deepEqual([...context.selectedCells],[2]);
const utilities=read('assets/web/application-utilities.js');vm.runInContext(utilities,context);
context.root=context;context.translate=value=>value;context.stateCaption=context.PakarApplicationUtilities.stateCaption;context.preferences={labels:'colors'};
vm.runInContext(utilities.slice(utilities.indexOf('    function setControlCaption('),utilities.indexOf('    function refreshLabels(')),context);
context.control=modern;vm.runInContext("setControlCaption(control,'lost');setControlCaption(control,'lost')",context);
assert.equal(modern.children.length,2,'Semantic caption updates reuse the stable label slot, never duplicate it');assert.equal(modern.children[1].textContent,'Red');assert.equal(modern.getAttribute('aria-label'),'Red');assert.equal(modern.dataset.cellTone,'lost');
context.preferences={labels:'states'};vm.runInContext("setControlCaption(control,'lost')",context);assert.equal(modern.children[1].textContent,'Lost');assert.equal(modern.children[0].tagName,'SVG');
assert(css.includes('button[data-cell-tone]:not(:disabled)'),'Semantic color styles never brighten disabled actions');
const mappingDot=css.match(/\.studio \.mapping-pixel>\.mapping-dot\{([^}]+)\}/)?.[1];assert(mappingDot);for(const rule of ['inline-size:32px','block-size:32px','aspect-ratio:1','border-radius:50%','flex:none','border:1px solid'])assert(mappingDot.includes(rule),'Pixel circles retain fixed square geometry: '+rule);
assert(css.includes('.mapping-dot[data-known=false]{border-style:dashed'),'Missing RGB is distinct from observed black');
assert(css.includes('.studio .correction-rule-map .mapping-dot{inline-size:28px;block-size:28px}'),'Rule diagrams use the Flutter compact diameter without changing diagnostic pixels');
assert(css.includes('.studio .correction-rule-map{display:flex;flex-wrap:wrap'),'Compact rule diagrams wrap rather than stretch their circles');
console.log('Web controls: stable SVG/label hydration, translation/accessibility, semantic isolation, square/flex rules and green contrast passed. Not browser layout acceptance.');
