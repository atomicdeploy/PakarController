import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

// Test the canonical production controls/payload without a browser or fake peer.
const source=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
assert.match(source, /<option value="once">Once<\/option>/);
assert.match(source, /release:true/);
const fields={displayText:{value:'HELLO WORLD'},displaySpeed:{value:'260'},displayHold:{value:'1500'},
  displayRepeat:{value:'once'},displayScroll:{checked:false},displayPause:{value:'5'},
  displayPauseLabel:{hidden:false},displayHoldLabel:{hidden:false}};
let sent;
const context=vm.createContext({...fields,post:(path,data)=>{sent={path,data}}});
for (const name of ['showMarquee','displayControls']) {
  const body=source.match(new RegExp(`function ${name}\\(\\)\\{([^}]+)\\}`));
  assert.ok(body,`${name} exists`);
  // showMarquee contains the nested object literal; include its closing brace.
  const expression=name==='showMarquee'
    ? source.match(/function showMarquee\(\)\{post\([^\n]+?\)\}/)[0]
    : body[0];
  vm.runInContext(expression,context);
}
vm.runInContext('showMarquee();displayControls()',context);
assert.equal(sent.path,'/api/display');
assert.equal(sent.data.repeat,'once'); assert.equal(sent.data.force_scroll,false);
assert.equal(fields.displayPauseLabel.hidden,true); assert.equal(fields.displayHoldLabel.hidden,true);
fields.displayText.value='HI';vm.runInContext('displayControls()',context);
assert.equal(fields.displayHoldLabel.hidden,false);
fields.displayRepeat.value='interval'; fields.displayPause.value='30'; fields.displayScroll.checked=true;
vm.runInContext('showMarquee();displayControls()',context);
assert.equal(sent.data.repeat,'interval'); assert.equal(sent.data.interval_seconds,'30');
assert.equal(sent.data.force_scroll,true); assert.equal(fields.displayPauseLabel.hidden,false);
fields.displayRepeat.value='loop';vm.runInContext('displayControls()',context);
assert.equal(fields.displayPauseLabel.hidden,true);assert.equal(fields.displayHoldLabel.hidden,true);
console.log('Embedded Web display defaults, scheduling payload and contextual controls passed.');

let replacements=0,insertions=0;
const nodes=new Map();
const audioCue={value:'welcome',options:[],dataset:{},disabled:false,parentElement:{insertBefore(node){insertions++;nodes.set(node.id,node)}},
  replaceChildren(){replacements++;this.options=[];this.value=''},appendChild(option){this.options.push(option);if(this.options.length===1)this.value=option.value}};
const audioPlay={disabled:false};
const audioContext=vm.createContext({audioCue,audioPlay,document:{createElement:()=>({value:'',setAttribute(name,value){this[name]=value}}),getElementById:id=>nodes.get(id)},setUiAttribute:(node,name,value)=>node.setAttribute(name,value)});
vm.runInContext(readFileSync(new URL('../assets/web/feature-workspaces.js',import.meta.url),'utf8'),audioContext);
vm.runInContext(source.slice(source.indexOf('function renderAudio(s){'),source.indexOf('// Cell interaction is initialized below')),audioContext);
const render=state=>{audioContext.state=state;vm.runInContext('renderAudio(state)',audioContext)};
const observed={avr_online:true,audio_cues:['welcome','welcome-40led','new-board-melody','<script>','bad/name','x'.repeat(41),'welcome-40led',7]};
render(observed);
assert.deepEqual(audioCue.options.map(o=>o.value),['welcome','welcome-40led','new-board-melody'],'Accept future valid advertised cues, deduplicate and filter invalid values');
assert.equal(audioPlay.disabled,false);assert.equal(audioCue.value,'welcome');
assert.ok(!audioCue.options.some(o=>o.value==='attention'),'Older peer must not gain a new cue from the app catalog');
audioCue.value='new-board-melody';const stable=audioCue.options,paintCount=replacements;
for(let i=0;i<5;i++)render({...observed,uptime:i});
assert.equal(audioCue.options,stable);assert.equal(replacements,paintCount,'Unchanged live status does not rebuild the select');assert.equal(audioCue.value,'new-board-melody');assert.equal(insertions,1,'Search field is created once');
const search=nodes.get('audioCueSearch');search.value='New board';search.oninput();
assert.deepEqual(audioCue.options.map(o=>o.value),['new-board-melody']);assert.equal(audioPlay.disabled,false);
search.value='not found';search.oninput();assert.equal(audioCue.options.length,0);assert.equal(audioPlay.disabled,true);assert.equal(audioCue.disabled,true);assert.equal(search.disabled,false,'No search result must not disable searching');
search.value='';search.oninput();assert.equal(audioCue.options.length,3);assert.equal(audioPlay.disabled,false);
render({avr_online:true,audio_cues:['attention']});
assert.deepEqual(audioCue.options.map(o=>o.value),['attention']);
render({avr_online:false,audio_cues:['attention']});
assert.equal(audioCue.options.length,0);assert.equal(audioPlay.disabled,true);
assert.equal(search.disabled,true);assert.equal(audioCue.disabled,true);
render({avr_online:true});assert.equal(audioPlay.disabled,true);
render({avr_online:true,audio_cues:['<bad>',null]});assert.equal(audioPlay.disabled,true);
console.log('Canonical Web melody catalogue: valid future advertised cues, invalid filtering, stable options/selection, single searchable control, empty search and offline guards passed.');
