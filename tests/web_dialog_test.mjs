// Execute the shared component, not a dialog substitute. Native focus trapping,
// Escape and viewport layout still require rendered browser acceptance.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
let document;
class Element {
  constructor(tag){this.tagName=tag;this.children=[];this.attrs={};this.dataset={};this.style={};this.listeners={};this.disabled=false;this.open=false;this.isConnected=false;this.classList={add:()=>{}};}
  append(...items){for(const item of items){item.parentElement=this;item.isConnected=this.isConnected;this.children.push(item);}}
  set textContent(value){this.text=String(value);}
  get textContent(){return this.text||this.children.map(c=>c.textContent).join('');}
  setAttribute(key,value){this.attrs[key]=String(value);}
  getAttribute(key){return this.attrs[key]??null;}
  removeAttribute(key){delete this.attrs[key];}
  addEventListener(name,listener){(this.listeners[name]??=[]).push(listener);}
  dispatch(name,event={}){for(const listener of this.listeners[name]||[])listener({target:this,...event});}
  contains(item){return item===this||this.children.some(c=>c.contains(item));}
  focus(){document.activeElement=this;}
  showModal(){this.open=true;this.connect(true);}
  connect(value){this.isConnected=value;for(const c of this.children)c.connect(value);}
  close(){if(!this.open)throw Error('Repeated native close');this.open=false;this.dispatch('close');}
  remove(){this.connect(false);if(this.parentElement)this.parentElement.children=this.parentElement.children.filter(c=>c!==this);}
  getBoundingClientRect(){return {left:10,right:100,top:10,bottom:100};}
}
document={body:new Element('body'),createElement:tag=>new Element(tag),createElementNS:(_ns,tag)=>new Element(tag)};
document.body.connect(true);
const prior=new Element('button');document.body.append(prior);prior.focus();
const context=vm.createContext({document,console,setTimeout:()=>0,clearTimeout:()=>{}});
vm.runInContext(readFileSync(new URL('../web/modules/components.js',import.meta.url),'utf8'),context);
const ui=context.WebUI,flush=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
const actions=d=>d.element.children.find(c=>c.className==='web-dialog-actions').children;
const headerClose=d=>d.element.children[0].children[1];

const prompt=ui.dialog({title:'Discard edits?',description:'Keeps the board settings.',closeLabel:'بستن',actions:[{label:'Cancel',autofocus:true},{label:'Reload'}]});
const title=prompt.element.children[0].children[0].children[0],detail=prompt.element.children[0].children[0].children[1];
assert.equal(prompt.element.getAttribute('aria-labelledby'),title.id);
assert.equal(prompt.element.getAttribute('aria-describedby'),detail.id);
assert.equal(headerClose(prompt).getAttribute('aria-label'),'بستن');
assert.equal(document.activeElement,actions(prompt)[0],'Destructive confirmation focuses Cancel');
prompt.close();prompt.close();assert.equal(document.activeElement,prior);assert(!prompt.element.isConnected);

const content=new Element('div'),search=new Element('input');content.append(search);
const palette=ui.dialog({title:'Go to panel',content,initialFocus:search});
assert.equal(document.activeElement,search,'Palette focus belongs to its actual search input');
assert.equal(palette.element.getAttribute('aria-describedby'),null,'No dangling description ID');
palette.close();
const outsider=new Element('input');
const invalidFocus=ui.dialog({title:'Safe',initialFocus:outsider});assert.notEqual(document.activeElement,outsider);invalidFocus.close();

let release,calls=0;
const saving=ui.dialog({title:'Save',actions:[{label:'Save',action:()=>{calls++;return new Promise(resolve=>{release=resolve;});}},{label:'Unavailable',disabled:true}]});
const save=actions(saving)[0];save.dispatch('click');save.dispatch('click');await flush();
assert.equal(calls,1,'Only one asynchronous action owns the dialog');
assert.equal(saving.element.getAttribute('aria-busy'),'true');assert(actions(saving).every(b=>b.disabled));
release(false);await flush();assert(saving.element.open,'A false result retains the dialog');assert(!save.disabled);assert(actions(saving)[1].disabled);assert.equal(saving.element.getAttribute('aria-busy'),null);
save.dispatch('click');await flush();assert.equal(calls,2);saving.close();release(true);await flush();
assert(!saving.element.isConnected,'Late completion cannot resurrect a dismissed dialog');
save.dispatch('click');await flush();assert.equal(calls,2,'Retired controls cannot run actions');

let shouldThrow=true;
const failure=ui.dialog({title:'Try again',actions:[{label:'Apply',action:()=>{if(shouldThrow)throw Error('Request failed');return true;}}]});
actions(failure)[0].dispatch('click');await flush();assert(failure.element.open);assert(!actions(failure)[0].disabled);assert.equal(failure.element.getAttribute('aria-busy'),null);
shouldThrow=false;actions(failure)[0].dispatch('click');await flush();assert(!failure.element.open,'Failed action releases ownership so a deliberate retry works');

const backdrop=ui.dialog({title:'Dismiss'});backdrop.element.dispatch('click',{clientX:50,clientY:50});assert(backdrop.element.open,'Interior padding is not a backdrop click');backdrop.element.dispatch('click',{clientX:1,clientY:1});assert(!backdrop.element.open);
const workspace=readFileSync(new URL('../assets/web/feature-workspaces.js',import.meta.url),'utf8');
assert(workspace.includes("closeLabel:translate('Close')"));assert(workspace.includes("label:translate('Cancel'),icon:'close',autofocus:true"));
const utilities=readFileSync(new URL('../assets/web/application-utilities.js',import.meta.url),'utf8');assert(utilities.includes("closeLabel:translate('Close'),content,initialFocus:search"));
console.log('Shared Web dialog: accessible description/localized close, deliberate focus, async single ownership, recovery and retirement passed. DOM evidence only.');
