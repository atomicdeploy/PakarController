// Isolated browser fixture: no server, saved session, board or network requests.
// Resolve Playwright from the host's provided NODE_PATH; no package installation.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createRequire} from 'node:module';
const {chromium}=createRequire(import.meta.url)('playwright');
const source=readFileSync(new URL('../web/index.html',import.meta.url),'utf8');
const code=source.slice(source.indexOf('// BEGIN KEY GAME PRESS CONTROLS'),source.indexOf('function renderContext()'));
const browser=await chromium.launch({channel:process.env.PAKAR_TEST_BROWSER_CHANNEL||'msedge',headless:true});
try{
  const page=await browser.newPage({viewport:{width:390,height:844}});
  await page.route('**/*',route=>route.abort());
  await page.setContent('<style>'+[...source.matchAll(/<style[^>]*>([\s\S]*?)<\/style>/g)].map(m=>m[1]).join('\n')+'</style><main><div id="contextMessage"></div></main>');
  await page.addScriptTag({content:`
    var calls=[],state={board_id:'fixture',boot_id:'boot',winner:0,key_mask_applied:true,key_game:{enabled_mask:3,idle_rgb:16777215,win_rgb:65280,lose_rgb:16711680},minigame:{available_modes:[{id:'press-to-win',label:'First press wins'},{id:'toggle',label:'Toggle'},{id:'color-match',label:'Color match'}],mode:'press-to-win',phase:'ready',revision:1,ready:true,elapsed_ms:0,remaining_ms:60000,timed:true,participating_mask:3,held_mask:0,latched_mask:0,winner:0,round:0,progress:0,mistakes:0,settings:{mode:'press-to-win',limit_ms:60000,warning_ms:10000,urgent_ms:3000,penalty_ms:2000,window_ms:1200,show_ms:500,gap_ms:200,feedback_ms:240,rounds:5,sequence_length:4,palette_size:7,fixed_target:0,toggle_kind:'toggle',hard:false,outputs:Array.from({length:8},()=>({source:'disabled',key:0}))}}};
    var peer={known:true,role:'first-wins',has:()=>true},peerHttp={writable:()=>true};
    function currentPeer(){return peer}function report(error){throw error}
    async function post(path,data){calls.push({path,...data});if(data.settings){if(data.settings.mode)Object.assign(state.minigame.settings,{limit_ms:data.settings.mode==='toggle'?0:60000,window_ms:1200,show_ms:500,gap_ms:200,feedback_ms:240});Object.assign(state.minigame.settings,data.settings);state.minigame.mode=state.minigame.settings.mode;}renderKeyGame();return true}
    ${code}
    renderKeyGame();`});
  assert.equal(await page.evaluate(()=>calls.length),0);
  assert.equal(await page.locator('[data-mini-mode] option').count(),3,'Only the board catalog is rendered');
  assert.equal(await page.locator('[data-mini-action=start]').isDisabled(),true);
  await page.locator('[data-key-readonly]').uncheck();
  await page.locator('[data-mini-action=start]').click();
  assert.equal(await page.evaluate(()=>calls[0].action),'start');
  await page.locator('[data-key="1"]').click();
  assert.equal(await page.evaluate(()=>calls[1].action),'tap');
  await page.locator('[data-minigame] summary').click();
  await page.locator('[data-mini-form] [name=limit_ms]').fill('45000');
  await page.evaluate(()=>{state.minigame.elapsed_ms=123;renderKeyGame()});
  assert.equal(await page.locator('[data-mini-form] [name=limit_ms]').inputValue(),'45000','Status does not erase the local draft');
  assert.equal(await page.locator('[data-mini-mode]').isDisabled(),true,'Mode cannot silently replace dirty settings');
  await page.locator('[data-mini-form] [name=source0]').selectOption('held');
  await page.locator('[data-mini-form] [name=key0]').fill('2');
  await page.locator('[data-mini-form] [type=submit]').click();
  assert.deepEqual(await page.evaluate(()=>calls.at(-1).settings.outputs[0]),{source:'held',key:2});
  assert.equal(await page.evaluate(()=>calls.at(-1).settings.limit_ms),45000);
  await page.locator('[data-mini-mode]').selectOption('toggle');
  assert.equal(await page.locator('[data-mini-form] [name=limit_ms]').inputValue(),'0','Mode selection replaces previous countdown with board defaults');
  await page.locator('[data-mini-form] [name=toggle_kind]').selectOption('press');
  await page.locator('[data-mini-form] [type=submit]').click();
  assert.equal(await page.evaluate(()=>calls.at(-1).settings.limit_ms),0,'Later settings save cannot resend the previous mode countdown');
  const box=page.locator('[data-key="1"]');await box.scrollIntoViewIfNeeded();
  const bounds=await box.boundingBox();await page.mouse.move(bounds.x+bounds.width/2,bounds.y+bounds.height/2);await page.mouse.down();await page.mouse.up();
  await page.waitForFunction(()=>calls.at(-1)?.action==='up');
  assert.deepEqual(await page.evaluate(()=>calls.slice(-2).map(call=>call.action)),['down','up']);
  await page.locator('[data-mini-mode]').selectOption('color-match');
  assert.equal(await page.locator('[data-mini-form] [name=limit_ms]').inputValue(),'60000');
  await box.scrollIntoViewIfNeeded();const colorBounds=await box.boundingBox();
  await page.mouse.move(colorBounds.x+colorBounds.width/2,colorBounds.y+colorBounds.height/2);
  const beforeHold=await page.evaluate(()=>calls.length);await page.mouse.down();
  await page.waitForTimeout(3100);
  assert.deepEqual(await page.evaluate(start=>calls.slice(start).map(call=>call.action),beforeHold),['down'],'Color-match remains held beyond the board reverse threshold');
  await page.mouse.up();await page.waitForFunction(start=>calls.length===start+2,beforeHold);
  assert.deepEqual(await page.evaluate(start=>calls.slice(start).map(call=>call.action),beforeHold),['down','up'],'Release does not add a second tap');
  await box.focus();const beforeKeyboard=await page.evaluate(()=>calls.length);
  await page.keyboard.down('Space');await page.keyboard.down('Space');await page.keyboard.up('Space');
  await page.waitForFunction(start=>calls.length===start+2,beforeKeyboard);
  assert.deepEqual(await page.evaluate(start=>calls.slice(start).map(call=>call.action),beforeKeyboard),['down','up'],'Keyboard repeat and release cannot duplicate color-match input');
  await page.locator('[data-mini-form] [name=window_ms]').fill('900');
  await page.evaluate(()=>{state.minigame.mode='toggle';state.minigame.settings.mode='toggle';state.minigame.settings.window_ms=1200;state.minigame.settings.limit_ms=0;renderKeyGame()});
  assert.equal(await page.locator('[data-mini-form] [name=window_ms]').inputValue(),'1200','External mode change retires the previous mode draft even while its field is focused');
  await page.evaluate(()=>{state.minigame.ready=false;renderKeyGame()});
  assert.equal(await page.locator('[data-mini-mode]').isDisabled(),true);assert.equal(await box.isDisabled(),true);
  await page.evaluate(()=>{state.minigame.ready=true;state.boot_id='next';renderKeyGame()});
  assert.equal(await page.locator('[data-key-readonly]').isChecked(),true,'New peer disarms controls');
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=390),'Phone fixture has no horizontal overflow');
  await page.evaluate(()=>{delete state.minigame;renderKeyGame()});
  assert.equal(await page.locator('[data-minigame]').isHidden(),true,'Legacy board hides minigames');
  console.log('Web minigame isolated browser: catalog, drafts, save mapping, tap/hold release, peer rearm and phone layout passed');
}finally{await browser.close()}
