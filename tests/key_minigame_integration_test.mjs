import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {resolve} from 'node:path';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const app=read('firmware/esp/application.ino');
const service=read('firmware/esp/src/KeyMiniGameService.h');
test('actual AVR key callback compiles for both roles and preserves their input owners',()=>{
  const avr=read('firmware/avr/avr.ino');
  const start=avr.indexOf('void boardKeyGesture('),end=avr.indexOf('void serviceInputs(',start);
  assert(start>=0&&end>start,'Extract the actual shared callback, not a copied implementation');
  const fixture=`#include <cassert>
#include <pccontroller/Keys.h>
#define _BV(bit) (1U << (bit))
uint32_t micros(){return 0;}
uint8_t stableInputs=0;bool stableInput9=false;unsigned dirty=0,actions=0;
void markStateDirty(){++dirty;}
void onInputChanged(uint8_t,bool pressed){assert(pressed);++actions;}
struct Controller {
  unsigned keys=0,edges=0;bool menu=false,espMenu=false;
  void key(uint8_t,KeyEvent,uint8_t,uint32_t,uint32_t){++keys;}
  bool menuActive()const{return menu;}bool espMenuActive()const{return espMenu;}
#if PAKAR_FIRST_WINS_ENABLED
  void gameInput(uint8_t,KeyEvent){++edges;}
#endif
}pcController;
struct Panel {bool held=false;bool captured()const{return held;}}frontPanel;
#if PAKAR_FIRST_WINS_ENABLED
bool espGameOwned=false;
#endif
${avr.slice(start,end)}
int main(){
  boardKeyGesture(0,KeyEvent::Down,nullptr);assert(stableInputs==1&&dirty==1&&pcController.keys==1&&actions==1);
  boardKeyGesture(0,KeyEvent::Up,nullptr);assert(stableInputs==0&&dirty==2&&pcController.keys==2&&actions==1);
  boardKeyGesture(8,KeyEvent::Down,nullptr);assert(stableInput9&&actions==2);
  boardKeyGesture(8,KeyEvent::Up,nullptr);assert(!stableInput9&&actions==2);
  pcController.menu=true;boardKeyGesture(1,KeyEvent::Down,nullptr);assert(actions==2);
  pcController.menu=false;pcController.espMenu=true;boardKeyGesture(1,KeyEvent::Down,nullptr);assert(actions==2);
  pcController.espMenu=false;frontPanel.held=true;boardKeyGesture(1,KeyEvent::Down,nullptr);assert(actions==2);
  frontPanel.held=false;boardKeyGesture(1,KeyEvent::Click,nullptr);assert(actions==2);
#if PAKAR_FIRST_WINS_ENABLED
  const auto priorKeys=pcController.keys;espGameOwned=true;
  boardKeyGesture(2,KeyEvent::Down,nullptr);assert((stableInputs&4)&&pcController.edges==1);
  boardKeyGesture(2,KeyEvent::Up,nullptr);assert(!(stableInputs&4)&&pcController.edges==2);
  boardKeyGesture(2,KeyEvent::Click,nullptr);assert(pcController.edges==2&&pcController.keys==priorKeys&&actions==2);
#else
  assert(pcController.edges==0&&pcController.keys==8);
#endif
}
`;
  const directory=resolve('build/native');mkdirSync(directory,{recursive:true});
  const cpp=resolve(directory,'avr_key_gesture_fixture.cpp'),exe=resolve(directory,'avr_key_gesture_test.exe');
  let previous;try{previous=readFileSync(cpp,'utf8')}catch(error){if(error.code!=='ENOENT')throw error;}
  if(previous!==fixture)writeFileSync(cpp,fixture);
  for(const enabled of [0,1]){
    const result=spawnSync(process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
      '-DPAKAR_FIRST_WINS_ENABLED='+enabled,'-I',resolve('tests/access_stubs'),'-I',resolve('libraries/BoardCore/src'),cpp,'-o',exe],{encoding:'utf8',timeout:60000});
    assert.equal(result.status,0,result.error?.message||result.stderr);
    const run=spawnSync(exe,[],{encoding:'utf8',timeout:10000});
    assert.equal(run.status,0,run.error?.message||run.stderr);
  }
});
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
test('boot restoration is once-only and timer cues cannot preempt AVR audio',()=>{
  const settings=read('firmware/esp/src/KeyMiniGameSettings.h'),tone=read('firmware/avr/TonePlayer.cpp');
  assert.match(app,/miniSettings\.value=BoardCore::bootGameConfig\(miniSettings\.value,miniSettings\.bootLast\)/);
  assert.match(service,/if\(miniBootPending\)\{miniBootPending=false/);
  assert(service.indexOf('if(!miniReady())')<service.indexOf('if(miniBootPending)'));
  assert.match(service,/miniTimerFeedback\.update\(miniGame\.running\(\)&&v\.timed/);
  assert.match(service,/!tone&&!successCue&&avrIdleAudio/);
  assert.match(tone,/if\(action==8&&\(isBusy\(\)\|\|streamState_==1\)\)return 5/);
  for(const field of ['boot_mode','segment_animation','timer_beeps','timer_alternate_tones']){
    assert(settings.includes(`out["${field}"]`));
    for(const file of ['mobile/lib/minigame.dart','mobile/lib/minigame_panel.dart','desktop/src/minigame.rs','firmware/esp/src/WebUi.h'])assert(read(file).includes(field),file+': '+field);
  }
});
test('actual cue owners fence deferred feedback and preserve first-win melody overrides',()=>{
  const ownerStart=service.indexOf('  static G::Phase audioPhase=',service.indexOf('void serviceMiniGame('));
  const ownerEnd=service.indexOf('  if(miniResumeAfterFence',ownerStart);
  const audioStart=service.indexOf('  const uint16_t tone=',ownerEnd);
  const audioEnd=service.indexOf('  // Set a level, never replay a press.',audioStart);
  const linkStart=app.indexOf('#if PAKAR_FIRST_WINS_ENABLED',app.indexOf('void serviceGameLink('));
  const linkEnd=app.indexOf('#endif',linkStart);
  assert(ownerStart>=0&&ownerEnd>ownerStart&&audioStart>ownerEnd&&audioEnd>audioStart&&linkEnd>linkStart,
    'Compile the actual owner and dispatch blocks, not a copied implementation');
  const ownershipLoss=service.slice(service.indexOf('  if(!miniReady()){',ownerEnd),service.indexOf('    miniTimerFeedback.update(false',ownerEnd));
  const fixture=`#include <cassert>
#include <vector>
#include <AudioCues.h>
#include <KeyMiniGame.h>
#include <KeyPresentation.h>
#include <GamePanelFeedback.h>
using G=BoardCore::KeyMiniGame;
using namespace BoardCore::Protocol;
struct Mini {
  G::Config cfg;G::View state;uint16_t note=0;
  const G::Config &config()const{return cfg;}
  G::View view()const{return state;}
  G::Phase phase()const{return state.phase;}
  uint16_t takeNote(){const auto out=note;note=0;return out;}
  bool running()const{return state.phase==G::Input||state.phase==G::Show;}
  bool feedbackCorrect()const{return false;}
}miniGame;
struct Settings {uint8_t winnerCue=kCueWinner;BoardCore::KeyPresentation presentation;}keySettings;
struct MiniSettings {bool timerBeeps=true,timerAlternateTones=true;}miniSettings;
BoardCore::GameTimerFeedback miniTimerFeedback;
struct Sent {uint8_t opcode;std::vector<uint8_t> payload;};
std::vector<Sent> sent;bool accept=true;
uint8_t sequence=1,winner=2,gameAudioSequence=0,gameAudioError=0;
uint16_t miniEpoch=1,miniFenceNote=0;
bool ready=true,avrMiniGames=true,avrOnline=true,keyAudioApplied=true,programmingLatched=false;
bool miniReady(){return ready;}
bool avrAtomicAudio=true,avrIdleAudio=true,avrFlashAudio=true;
bool gameAudioStarted=false,gameAudioPending=false,gameAudioAck=false;
uint32_t gameWinnerAt=0,gameAudioSentAt=0;
bool sendToAvr(uint8_t opcode,uint8_t,const uint8_t *data,uint8_t size){
  sent.push_back({opcode,std::vector<uint8_t>(data,data+size)});return accept;
}
void feedback(uint32_t now){
${service.slice(ownerStart,ownerEnd)}
${ownershipLoss}    return;
  }
  const auto v=miniGame.view();
${service.slice(audioStart,audioEnd)}
}
void winnerAudio(uint32_t now){
${app.slice(linkStart,linkEnd)}
#endif
}
void clear(){sent.clear();accept=true;gameAudioStarted=false;gameAudioPending=false;gameAudioAck=false;gameAudioError=0;}
int main(){
  // A generic success was queued, then another mode wins before the next tick.
  miniGame.cfg.mode=G::Illuminated;miniGame.state.phase=G::Success;feedback(100);assert(sent.empty());
  miniGame.cfg.mode=G::PressToWin;gameWinnerAt=150;winnerAudio(150);
  assert(sent.size()==1&&sent[0].opcode==kAudioStream&&sent[0].payload[0]==6);
  const auto timeline=sent[0].payload;
  assert(timeline.size()==12&&timeline[5]==120&&timeline[7]==100&&timeline[9]==4);
  assert((timeline[10]|uint16_t(timeline[11])<<8)==400);
  feedback(150);feedback(400);winnerAudio(400);assert(sent.size()==1);
  // Named user overrides remain authoritative and are never replaced/replayed.
  for(const auto cue:{kCueFinish,kCueLost,kCueAttention}){
    clear();keySettings.winnerCue=cue;gameWinnerAt=500;winnerAudio(500);
    assert(sent.size()==1&&sent[0].opcode==kAudioCue&&sent[0].payload[0]==cue);
    feedback(500);feedback(800);winnerAudio(800);assert(sent.size()==1);
  }
  clear();keySettings.winnerCue=BoardCore::AudioCues::kSilent;winnerAudio(900);feedback(900);assert(sent.empty());
  // A busy/uncertain write is a consumed intent; no generic fallback or retry.
  clear();keySettings.winnerCue=kCueWinner;gameWinnerAt=1000;accept=false;winnerAudio(1000);
  assert(sent.size()==1&&gameAudioError==kBusy&&gameAudioStarted&&!gameAudioPending);
  accept=true;winnerAudio(1001);feedback(1300);assert(sent.size()==1);
  // No winner from another game's stale global observation may own this path.
  clear();miniGame.cfg.mode=G::Simon;winnerAudio(1400);assert(sent.empty());
  // Other modes still earn their configured full cue after the final note.
  keySettings.winnerCue=kCueFinish;feedback(1500);feedback(1739);assert(sent.empty());
  feedback(1740);assert(sent.size()==1&&sent[0].opcode==kAudioCue&&sent[0].payload[0]==kCueFinish);
  // Even the same mode/phase in a new input epoch cannot inherit an old cue.
  clear();++miniEpoch;miniGame.state.phase=G::Success;feedback(2000);
  ++miniEpoch;miniGame.state.phase=G::Input;feedback(2100);miniGame.state.phase=G::Ready;
  feedback(2400);assert(sent.size()==1&&sent[0].opcode==kBuzzer); // Explicit ready stops old audio only.
  clear();miniGame.state.phase=G::Success;feedback(3000);ready=false;feedback(3100);
  ready=true;miniGame.state.phase=G::Ready;feedback(3400);
  assert(sent.size()==1&&sent[0].opcode==kBuzzer);
  clear();miniGame.state.phase=G::Success;feedback(3500);ready=false;feedback(3600);
  ready=true;feedback(3900);feedback(4200);assert(sent.empty());
  // Cancellation before the first post-switch tick is also peer/epoch-fenced.
  clear();miniGame.state.phase=G::Success;feedback(4000);miniGame.cfg.mode=G::PressToWin;
  ready=false;feedback(4100);ready=true;miniGame.state.phase=G::Success;
  feedback(4500);assert(sent.empty());
}
`;
  const directory=resolve('build/native');mkdirSync(directory,{recursive:true});
  const cpp=resolve(directory,'key_game_audio_owner_fixture.cpp'),exe=resolve(directory,'key_game_audio_owner_test.exe');
  let previous;try{previous=readFileSync(cpp,'utf8')}catch(error){if(error.code!=='ENOENT')throw error;}
  if(previous!==fixture)writeFileSync(cpp,fixture);
  const result=spawnSync(process.env.CXX||'g++',['-std=c++17','-O2','-g0','-Wall','-Wextra','-Werror',
    '-DPAKAR_FIRST_WINS_ENABLED=1','-I',resolve('libraries/BoardCore/src'),cpp,'-o',exe],{encoding:'utf8',timeout:60000});
  assert.equal(result.status,0,result.error?.message||result.stderr);
  const run=spawnSync(exe,[],{encoding:'utf8',timeout:10000});
  assert.equal(run.status,0,run.error?.message||run.stderr);
});
