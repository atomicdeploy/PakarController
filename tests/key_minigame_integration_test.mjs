import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const app=read('firmware/esp/application.ino');
const service=read('firmware/esp/src/KeyMiniGameService.h');
test('every minigame command is admitted and handled by the existing correlated command path',()=>{
  const admission=app.slice(app.indexOf('const bool supported=command=='),app.indexOf('const bool supported=command==')+1600);
  for(const name of ['minigame-configure','minigame-control','minigame-input']){
    assert(admission.includes(`command=="${name}"`),name+' missing from admission');
    assert(app.includes(`command=="${name}"`));
    assert(service.includes(`command=="${name}"`));
  }
  assert.match(app,/commandReceipts\.reserve/);
  assert.match(app,/else status=buildStatusJson\(\)/);
});
test('AVR ownership is explicit and raw edges use the established event queue',()=>{
  const avr=read('firmware/avr/avr.ino'),runtime=read('firmware/avr/PcControllerRuntime.cpp');
  assert.match(avr,/requestedOwner=frame\.payloadLength==7&&frame\.payload\[4\]/);
  assert.match(avr,/if\(espGameOwned\)\{\s*if\(event==KeyEvent::Down\|\|event==KeyEvent::Up\)pcController\.gameInput/);
  assert.match(runtime,/if\(gameOwned_&&eventCount_\)\{drainOne\(\);return;\}/);
  assert.match(app,/frame\.payloadLength==8&&frame\.payload\[0\]==ControllerProtocol::Event/);
  assert.match(app,/frame\.payload\[4\]==0&&\(frame\.payload\[6\]\|uint16_t\(frame\.payload\[7\]\)<<8\)==miniEpoch/);
  assert.match(service,/if\(\+\+miniEpoch==0\)\+\+miniEpoch/);
  assert.match(service,/avrMiniEpoch==miniEpoch/);
});
test('remote holds expire and game settings never save from the frame loop',()=>{
  assert.match(service,/uint32_t\(now-miniRemoteAt\[k-1\]\)>=5000/);
  const loop=service.slice(service.indexOf('void serviceMiniGame'),service.indexOf('bool miniPanel'));
  assert.doesNotMatch(loop,/\.save\(|LittleFS|delay\(/);
  assert.match(service,/if\(!next\.save\(LittleFS\)\)/);
  const configure=service.slice(service.indexOf('if(command=="minigame-configure")'),service.indexOf('}else if(command=="minigame-control")'));
  assert.match(configure,/#if defined\(ARDUINO_ARCH_ESP32\)[\s\S]*ScopedLedFlashPause pause;\s+if\(!pause\)\{result=F\("led_flash_guard_busy"\);return false;\}\s+#endif\s+if\(!next\.save\(LittleFS\)\)/,
    'Persistent configuration shares the LED flash fence; refusal cannot mutate the game');
  assert(configure.indexOf('if(!pause)')<configure.indexOf('miniSettings=next;'));
  assert.match(service,/miniPhysicalHeld\|miniRemoteHeld/);
});
test('countdown presentation keeps update progress priority and shared effects',()=>{
  const panel=read('firmware/esp/src/SemanticPanelService.h'),led=read('firmware/esp/src/LedGrid.cpp');
  assert.match(panel,/!updating&&miniReady\(\)&&miniGame\.view\(\)\.timed&&miniGame\.running\(\)/);
  assert.match(panel,/semanticPanel\.persistent\(updating\|\|gameCountdown,text\)/);
  assert.match(led,/Presentation::composeLayer\(\*gameLayer_/);
  assert.match(led,/millis\(\)-gamePresentationAt_/);
  assert.match(service,/grid\.setGamePresentationEpoch\(miniGame\.presentationAt\(\)\)/);
  assert.match(service,/mode!=G::PressToWin&&\(v\.phase==G::Ready\|\|v\.phase==G::Stopped\)/);
  assert.match(service,/resultCue=kCueIncorrect/);
  assert.match(service,/resultCue=keySettings\.winnerCue/);
});
