import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const html=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
// Keep operator-facing success concise; verification details belong in logs,
// protocol fields and acceptance evidence, not repeated control-panel narration.
for(const path of ['web/index.html','mobile/lib/main.dart','desktop/src/ui/shell.rs'])
  assert.doesNotMatch(readFileSync(new URL('../'+path,import.meta.url),'utf8'),/physical output not verified/i);
const code=html.slice(html.indexOf('function renderStateAppearance(){'),html.indexOf('function renderAssignmentTools(){'));
// Delayed capability arrival after Studio has flattened its old .stack columns.
// The old renderer dereferenced null.appendChild here on first live identity.
const ids=new Map();
class Node {
  constructor(tag){this.tagName=tag;this.dataset={};this.children=[];this.style={};this.type='';}
  set id(v){this._id=v;ids.set(v,this);}get id(){return this._id;}
  set innerHTML(value){for(const match of value.matchAll(/<(\w+)[^>]*\bid="([^"]+)"[^>]*>/g)){const n=new Node(match[1]);n.id=match[2];this.appendChild(n);}}
  appendChild(n){assert(n instanceof Node);n.parentElement=this;this.children.push(n);return n;}
  append(...items){for(const n of items)this.appendChild(n);}
  prepend(n){n.parentElement=this;this.children.unshift(n);}
  add(n){this.appendChild(n);}
  querySelector(s){return ids.get(s.slice(1))??null;}
  querySelectorAll(){return [...ids.values()].filter(n=>['input','select','button'].includes(n.tagName));}
  setAttribute(){}
}
const layout=new Node('main');let present=true;
const context=vm.createContext({document:{createElement:t=>new Node(t),createTextNode:t=>Object.assign(new Node('text'),{textContent:t}),
  getElementById:id=>ids.get(id),querySelector:s=>s==='.layout'&&present?layout:null,activeElement:null},
  currentPeer:()=>({has:()=>true,role:'first-wins'}),state:{state_presentation:{enabled:true,states:{won:{rgb:0x00ff00,motion:2}}}},
  presentationBusy:false,authenticated:true,peerHttp:{writable:()=>true},saveStateAppearance(){},temporaryStateColor(){},
  uiTr:text=>text,setUiText:(node,text)=>{node.textContent=text;},
  bulkState:{querySelector:()=>true},Option:function(text,value){return Object.assign(new Node('option'),{textContent:text,value});}});
vm.runInContext(code,context);
present=false;vm.runInContext('renderStateAppearance()',context);assert.equal(layout.children.length,0);
present=true;vm.runInContext('renderStateAppearance()',context);
assert.equal(layout.children.length,1);assert.equal(ids.get('stateAppearanceCard').dataset.capability,'state-presentation');
const motion=ids.get('stateMotion-won');assert.equal(motion.value,'2');context.document.activeElement=motion;
context.state.state_presentation.states.won.motion=1;vm.runInContext('renderStateAppearance()',context);
assert.equal(motion.value,'1','Focused dropdown must reflect confirmed external changes');
assert.equal(layout.children.length,1,'Reconnect does not duplicate a panel');
assert(ids.get('fireDirectionField').hidden,'Keycontroller does not expose a matrix rotation');
context.currentPeer=()=>({has:()=>true,role:'table-selection'});
context.state.state_presentation.fire_rotation=0;
vm.runInContext('renderStateAppearance()',context);
assert.equal(ids.get('fireDirectionField').hidden,false);
assert.equal(ids.get('fireDirection').value,'0');
const sent=[];context.saveStateAppearance=patch=>sent.push(patch);
ids.get('fireDirection').value='180';ids.get('fireDirection').onchange();
assert.equal(sent.length,1);assert.equal(sent[0].fire_rotation,180);
assert.equal(ids.get('fireDirection').value,'0','Keep actual direction until the board acknowledges');
context.state.state_presentation.fire_rotation=180;
vm.runInContext('renderStateAppearance()',context);assert.equal(ids.get('fireDirection').value,'180');
assert.equal(sent.length,1,'Readback never replays the effect');
delete context.state.state_presentation.fire_rotation;
vm.runInContext('renderStateAppearance()',context);assert(ids.get('fireDirectionField').hidden,'Older firmware stays compatible');
console.log('Delayed appearance mount after Studio, reconnect and focused live dropdown passed');
