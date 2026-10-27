import {GameAudio} from './sounds.mjs';
const audio=new GameAudio();
const $=id=>document.getElementById(id),canvas=$('canvas'),ctx=canvas.getContext('2d');
let state,token,map={},editing=false,dirty=false;
const initial=await fetch('/api/session').then(r=>r.json());token=initial.token;state=initial;map={...state.map};
let lastSound=initial.soundEvent?.id??0;
// Configure a target on open, never acquire a board lease before explicit Start.
$('board').checked=new URLSearchParams(location.search).get('board')==='1';
try{const prefs=JSON.parse(localStorage.getItem('drs-snake-controls')??'{}');$('showIds').checked=prefs.showIds!==false;}catch{}
function error(message){$('error').hidden=!message;$('error').textContent=message;}
async function send(action,extra={}){try{const response=await fetch('/api/snake',{method:'POST',headers:{'Content-Type':'application/json','X-Snake-Token':token},body:JSON.stringify({action,...extra})});const result=await response.json();if(!response.ok)throw Error(result.error);state=result;render();return true;}catch(e){error(e.message);return false;}}
function render(){
  if(state.soundEvent&&state.soundEvent.id>lastSound){lastSound=state.soundEvent.id;audio.play(state.soundEvent.name);}
  $('phase').textContent=state.phase[0].toUpperCase()+state.phase.slice(1);$('score').textContent=state.score;$('length').textContent=state.body.length;
  $('connection').textContent=state.board?`${state.board.hostname} · ${state.board.transport}`:'Host display';
  const active=['running','paused'].includes(state.phase);
  $('start').disabled=active||!state.enabled;$('restart').disabled=state.phase==='ready'||!state.enabled;$('stop').disabled=!active;
  $('pause').disabled=!active;$('pause').textContent=state.phase==='paused'?'▶ Resume':'Ⅱ Pause';$('board').disabled=active;
  $('boardSound').disabled=active;
  $('restore').textContent=state.restoration?.message??'';
  $('soundWarning').textContent=state.soundWarning?`Board sound: ${state.soundWarning}`:'';
  for(const input of $('configuration').elements)input.disabled=active;
  error(state.error??'');
  if(!dirty){
    for(const id of ['interval','growth','seeds'])$(id).value=state.options[id];
    for(const id of ['autoplay','immortal'])$(id).checked=state.options[id];
    if(active)$('board').checked=state.boardOutput;
  }
  paint();
}
function paint(){
  ctx.clearRect(0,0,1200,320);const colors=new Map(state.cells.map(v=>[v.id,v.rgb]));
  for(let id=1;id<=80;id++){
    const n=id-1,x=2*(n%10)+Math.floor(n/10)%2,y=Math.floor(n/20),cx=30+x*60,cy=32+y*66+(x%2)*33;
    const tile=editing?map[id]:state.map[id];ctx.beginPath();ctx.roundRect(cx-24,cy-24,48,48,12);
    ctx.fillStyle=colors.has(id)?'#'+colors.get(id).toString(16).padStart(6,'0'):'#22303e';ctx.fill();
    ctx.strokeStyle=tile?'#ffd46a':'#405064';ctx.lineWidth=tile?2:1;ctx.stroke();
    if($('showIds').checked||tile){ctx.fillStyle=colors.has(id)?'#ffffff':'#9caebf';ctx.font='500 13px system-ui';ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText(tile?({block:'×',detour1:'↱',detour2:'↰',reverse:'↶',teleport:'◎',switch:'⇄',potion:'◆'}[tile]):String(id),cx,cy);}
  }
}
$('start').onclick=async()=>{error('');await audio.unlock().catch(()=>{});send('start',{board:$('board').checked,boardSound:$('boardSound').checked});};
$('restart').onclick=async()=>{await audio.unlock().catch(()=>{});send('restart',{board:$('board').checked,boardSound:$('boardSound').checked});};$('stop').onclick=()=>send('stop',{runId:state.runId});
$('appSound').onchange=()=>{audio.enabled=$('appSound').checked;if(audio.enabled)audio.unlock().catch(()=>{});};
$('pause').onclick=()=>send(state.phase==='paused'?'resume':'pause');
document.querySelectorAll('[data-direction]').forEach(b=>b.onclick=()=>send('direction',{value:b.dataset.direction}));
$('settingsToggle').onclick=()=>{const open=$('settings').hidden;$('settings').hidden=!open;$('settingsToggle').setAttribute('aria-expanded',String(open));};
$('showIds').onchange=()=>{localStorage.setItem('drs-snake-controls',JSON.stringify({showIds:$('showIds').checked}));paint();};
$('edit').onclick=()=>{editing=!editing;$('edit').classList.toggle('editing',editing);$('edit').textContent=editing?'Finish editing':'Edit map';paint();};
$('configuration').oninput=()=>{dirty=true;$('draft').textContent='Unapplied settings';};
$('configuration').onsubmit=async e=>{e.preventDefault();if(await send('configure',{map,options:{interval:+$('interval').value,growth:+$('growth').value,seeds:+$('seeds').value,autoplay:$('autoplay').checked,immortal:$('immortal').checked}})){dirty=false;$('draft').textContent='Applied to the next round';}};
canvas.onclick=e=>{
  if(!editing||['running','paused'].includes(state.phase))return;
  const r=canvas.getBoundingClientRect(),px=(e.clientX-r.left)*1200/r.width,py=(e.clientY-r.top)*320/r.height;
  const x=Math.round((px-30)/60),y=Math.round((py-32-(x%2)*33)/66);
  if(x<0||x>=20||y<0||y>=4)return;
  const id=1+Math.floor(x/2)+(x%2)*10+y*20;
  if($('tile').value)map[id]=$('tile').value;else delete map[id];dirty=true;$('draft').textContent='Unapplied map';paint();
};
let touch;
canvas.onpointerdown=e=>{touch={x:e.clientX,y:e.clientY};};
canvas.onpointerup=e=>{if(!touch||editing)return;const dx=e.clientX-touch.x,dy=e.clientY-touch.y;touch=null;if(Math.max(Math.abs(dx),Math.abs(dy))<20)return;send('direction',{value:Math.abs(dx)>Math.abs(dy)?dx>0?'right':'left':dy>0?'down':'up'});};
canvas.onpointercancel=()=>{touch=null;};
addEventListener('keydown',e=>{if(/INPUT|SELECT|TEXTAREA/.test(e.target.tagName)||e.repeat)return;const d={ArrowUp:'up',w:'up',ArrowDown:'down',s:'down',ArrowLeft:'left',a:'left',ArrowRight:'right',d:'right'}[e.key];if(d){e.preventDefault();send('direction',{value:d});}else if(e.code==='Space'){e.preventDefault();$('pause').click();}});
$('export').onclick=()=>{const blob=new Blob([JSON.stringify({map,options:state.options},null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='drs-snake-map.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('import').onchange=async()=>{try{const file=$('import').files[0];if(!file)return;if(file.size>8192)throw Error('Map exceeds 8 KiB');const value=JSON.parse(await file.text());if(await send('configure',value)){map={...state.map};for(const id of ['interval','growth','seeds'])$(id).value=state.options[id];for(const id of ['autoplay','immortal'])$(id).checked=state.options[id];dirty=false;paint();}}catch(e){error(e.message);}finally{$('import').value='';}};
addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
const events=new EventSource('/api/events');events.onmessage=e=>{state=JSON.parse(e.data);if(!dirty)map={...state.map};render();};events.onerror=()=>error('Host connection lost. Board presentation expires automatically.');
addEventListener('pagehide',()=>{
  events.close();
  if(['running','paused'].includes(state.phase))fetch('/api/snake',{method:'POST',keepalive:true,
    headers:{'Content-Type':'application/json','X-Snake-Token':token},body:JSON.stringify({action:'stop',runId:state.runId})}).catch(()=>{});
});
// BFCache must create a fresh observer; it must never resume a stopped game.
addEventListener('pageshow',e=>{if(e.persisted)location.reload();});
render();
