import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

// Test the embedded production controls/payload without a browser or fake peer.
const source=readFileSync(new URL('../firmware/esp8266/src/WebUi.h',import.meta.url),'utf8');
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

const audioCue={value:'welcome',options:[],replaceChildren(){this.options=[]},appendChild(option){this.options.push(option)}};
const audioPlay={disabled:false};
const audioContext=vm.createContext({audioCue,audioPlay,document:{createElement:()=>({})}});
vm.runInContext(source.match(/^function renderAudio\(s\).*$/m)[0],audioContext);
vm.runInContext(`renderAudio({avr_online:true,audio_cues:['welcome','welcome-40led','bad','welcome-40led',7]})`,audioContext);
assert.deepEqual(audioCue.options.map(o=>o.value),['welcome','welcome-40led']);
assert.equal(audioPlay.disabled,false);assert.equal(audioCue.value,'welcome');
assert.ok(!audioCue.options.some(o=>o.value==='attention'),'Older peer must not gain a new cue from the app catalog');
vm.runInContext(`renderAudio({avr_online:true,audio_cues:['attention']})`,audioContext);
assert.deepEqual(audioCue.options.map(o=>o.value),['attention']);
vm.runInContext('renderAudio({avr_online:false,audio_cues:[]})',audioContext);
assert.equal(audioCue.options.length,0);assert.equal(audioPlay.disabled,true);
vm.runInContext('renderAudio({avr_online:true})',audioContext);
assert.equal(audioPlay.disabled,true);
console.log('Web melodies use only known peer-advertised AVR cues; offline and absent metadata disable playback.');
