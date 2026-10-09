#pragma once

#include <Arduino.h>

const char kWebUi[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PakarController</title><style>
:root{color-scheme:dark;--bg:#070b16;--panel:#11182b;--line:#24314f;--text:#eef4ff;--muted:#91a2c5;--red:#ff405c;--cyan:#4ad9ff}
*{box-sizing:border-box}body{margin:0;font:15px system-ui;background:radial-gradient(circle at 20% 0,#182a50 0,transparent 38%),var(--bg);color:var(--text)}
main{max-width:1180px;margin:auto;padding:24px}.top{display:flex;justify-content:space-between;gap:20px;align-items:end}.eyebrow{color:var(--cyan);letter-spacing:.16em;text-transform:uppercase;font-size:12px}h1{margin:.25rem 0;font-size:clamp(30px,6vw,64px)}
.status{color:var(--muted)}.layout{display:grid;grid-template-columns:1fr 330px;gap:18px;margin-top:20px}.card{background:#11182bdd;border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 18px 50px #0005}.grid{display:grid;grid-template-columns:repeat(10,1fr);gap:7px;direction:rtl}.cell{aspect-ratio:1;border:1px solid #435474;border-radius:12px;background:#f6f7ff;color:#111827;font-weight:800;cursor:pointer;transition:.18s transform,.18s box-shadow,.18s background}.cell:hover{transform:translateY(-2px);box-shadow:0 7px 20px #4ad9ff33}.cell.red{background:var(--red);color:white}.cell.off{background:#151b29;color:#64748b}
.controls{display:grid;gap:12px}button,select,input{font:inherit;border-radius:10px;border:1px solid #344566;background:#18223a;color:var(--text);padding:10px 12px}button{cursor:pointer}button.primary{background:linear-gradient(135deg,#1e8fff,#6d5cff);border:0}.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1}.metric{display:flex;justify-content:space-between;border-bottom:1px solid var(--line);padding:9px 0}.metric span{color:var(--muted)}.warn{color:#ffcb6b;font-size:13px}.winner{font-size:58px;font-weight:900;color:var(--cyan);text-align:center;padding:12px}
@media(max-width:850px){.layout{grid-template-columns:1fr}.grid{gap:4px}.cell{border-radius:7px;padding:0}.top{align-items:start;flex-direction:column}}
</style></head><body><main><div class="top"><div><div class="eyebrow">AtomicDeploy control surface</div><h1>PakarController</h1><div class="status" id="summary">Connecting…</div></div><button onclick="refresh()">Refresh state</button></div>
<div class="layout"><section class="card"><div class="grid" id="grid"></div></section><aside class="controls"><section class="card"><div class="metric"><span>Mode</span><b id="mode">—</b></div><div class="metric"><span>AVR</span><b id="avr">—</b></div><div class="metric"><span>Winner</span><b id="winnerSmall">—</b></div><div class="winner" id="winner">—</div><div class="row"><button class="primary" onclick="setMode('cell')">Cell select</button><button class="primary" onclick="setMode('first-wins')">First wins</button></div><button onclick="resetGame()">Reset game</button></section>
<section class="card"><b>Selected cell action</b><div class="row"><input id="cellId" type="number" min="1" max="80" value="1"><select id="effect"><option>fade</option><option>immediate</option><option>pulse</option></select></div><div class="row"><button onclick="setCell('white')">White</button><button onclick="setCell('red')">Red</button><button onclick="setCell('off')">Off</button></div></section>
<section class="card"><b>Firmware vault</b><p class="warn">Programming automatically takes a local backup first and requires a complete 32 KiB merged AVR image.</p><input id="avrFile" type="file" accept=".bin"><button onclick="uploadAvr()">Store AVR image</button><div class="row"><button onclick="api('/api/v1/avr/backup',{method:'POST'})">Back up AVR</button><button onclick="programAvr()">Program + verify</button></div><a href="/update">ESP self-OTA</a></section></aside></div></main>
<script>
const grid=document.querySelector('#grid');let status={cells:[]};
function idFor(x,y){const fromRight=9-x;const o=fromRight%2===0?7-y:y;return fromRight*8+o+1}
for(let y=0;y<8;y++)for(let x=0;x<10;x++){const id=idFor(x,y),b=document.createElement('button');b.className='cell';b.textContent=id;b.onclick=()=>{cellId.value=id;setCell('red')};b.dataset.id=id;grid.appendChild(b)}
async function api(path,options={}){const r=await fetch(path,options);const t=await r.text();if(!r.ok)throw Error(t||r.statusText);try{return JSON.parse(t)}catch{return t}}
async function refresh(){try{status=await api('/api/v1/status');summary.textContent=`${status.hostname} · ${status.ip} · ${status.pixels} pixels`;mode.textContent=status.mode;avr.textContent=status.avr_online?'online':'waiting for UART';winnerSmall.textContent=status.winner||'—';winner.textContent=status.winner||'—';for(const b of grid.children){const s=status.cells[Number(b.dataset.id)-1]||'white';b.className='cell '+(s==='white'?'':s)}}catch(e){summary.textContent=e.message}}
async function setMode(v){await api('/api/v1/mode?value='+v,{method:'POST'});refresh()}
async function resetGame(){await api('/api/v1/game/reset',{method:'POST'});refresh()}
async function setCell(state){const id=cellId.value,e=effect.value;await api(`/api/v1/cell?id=${id}&state=${state}&effect=${e}`,{method:'POST'});refresh()}
async function uploadAvr(){const f=avrFile.files[0];if(!f)return alert('Choose a 32 KiB .bin file');const d=new FormData();d.append('image',f);alert(await api('/api/v1/avr/image',{method:'POST',body:d}))}
async function programAvr(){if(!confirm('Back up, erase, program and verify the AVR flash?'))return;alert(await api('/api/v1/avr/program',{method:'POST'}));refresh()}
refresh();setInterval(refresh,1000);
</script></body></html>
)HTML";
