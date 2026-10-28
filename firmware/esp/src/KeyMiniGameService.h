#pragma once
// Included after the sketch's transport/settings owners. No second writer,
// polling game input, delayed command replay, or filesystem writes per tick.
#if PAKAR_FIRST_WINS_ENABLED
bool miniReady(){return miniGame.config().valid()&&avrOnline&&avrMiniGames&&avrMiniOwned&&avrMiniEpoch==miniEpoch&&keyMaskApplied&&!programmingLatched&&
  !urbootBusy.get()&&!urbootQueued&&!urbootRawOwned&&!otaActive&&!otaRebootAt&&!avrUploadOwner;}
String miniJson(){
  using G=BoardCore::KeyMiniGame;JsonDocument d;const auto v=miniGame.view();
  d["ready"]=miniReady();d["mode"]=G::name(miniGame.config().mode);d["phase"]=G::phaseName(v.phase);
  d["revision"]=v.revision;d["elapsed_ms"]=v.elapsedMs;d["remaining_ms"]=v.remainingMs;d["timed"]=v.timed;
  d["participating_mask"]=v.participatingMask;d["held_mask"]=v.heldMask;d["latched_mask"]=v.latchedMask;
  d["winner"]=v.winner;d["target"]=v.target;d["round"]=v.round;d["progress"]=v.progress;
  d["mistakes"]=v.mistakes;d["warning"]=v.warning;d["last_reaction_ms"]=v.lastReactionMs;d["best_reaction_ms"]=v.bestReactionMs;
  d["remote_hold_lease_ms"]=5000;d["outputs_owned"]=avrMiniOwned;miniSettings.json(d["settings"].to<JsonObject>());
  if(miniGame.config().mode==G::ColorMatch)d["matching_mask"]=miniGame.matchingMask();
  d["arpeggio_supported"]=avrOnline&&avrAtomicAudio;
  d["timer_beeps_supported"]=avrOnline&&avrIdleAudio;
  if(v.phase==G::Feedback)d["feedback_correct"]=miniGame.feedbackCorrect();
  auto modes=d["available_modes"].to<JsonArray>();
  constexpr const char *labels[]={"Press to win","Illuminated target","Color pattern","Repeat sequence","Matching pairs",
    "Clear all","Color order","Switch pattern","Reaction chase","Toggle"};
  for(uint8_t i=0;i<G::ModeCount;++i)if(G::available(G::Mode(i))){auto mode=modes.add<JsonObject>();mode["id"]=G::name(G::Mode(i));mode["label"]=labels[i];mode["description"]=G::description(G::Mode(i));}
  String result;serializeJson(d,result);return result;
}
void miniInput(uint8_t key,bool down,bool physical,uint32_t now){
  using G=BoardCore::KeyMiniGame;const uint16_t mask=G::keyBit(key);if(!mask)return;
  const uint16_t before=miniPhysicalHeld|miniRemoteHeld;
  auto &owner=physical?miniPhysicalHeld:miniRemoteHeld;
  if(down)owner|=mask;else owner&=~mask;
  if(!physical&&down)miniRemoteAt[key-1]=now;
  const uint16_t after=miniPhysicalHeld|miniRemoteHeld;
  if((before&mask)!=(after&mask))miniGame.input(key,down,now);
}
void miniInputFence(uint32_t now){
  // A reset/start is a new input epoch, not permission to consume queued presses.
  if(++miniEpoch==0)++miniEpoch;
  miniPhysicalHeld=miniRemoteHeld=0;miniResumeAfterFence=miniGame.running();
  miniFenceNote=miniResumeAfterFence?miniGame.takeNote():0;
  if(miniResumeAfterFence)miniGame.pause(now);
  miniFenceAt=now;keyMaskApplied=false;keyMaskPending=true;keyMaskSentAt=0;
}
void miniReset(uint32_t now){miniBootPending=false;miniGame.reset(now);miniInputFence(now);winner=0;grid.resetWhite();gameAutoResetArmed=false;keyObservationPending=true;}
bool miniCommand(JsonObjectConst request,String &result){
  using G=BoardCore::KeyMiniGame;
  if(!miniReady()){result=F("minigame_avr_ownership_not_ready");return false;}
  const String command=request["command"]|"";const uint32_t now=millis();
  if(command=="minigame-configure"){
    KeyMiniGameSettings next=miniSettings;next.value.enabledMask=keySettings.enabledMask;
    if(!next.patch(request["settings"])){result=F("invalid_minigame_settings");return false;}
    if(!grid.presentationReady()||hostPresentation.model.active()){result=F("presentation_busy");return false;}
#if defined(ARDUINO_ARCH_ESP32)
    // The same flash/LED owner as other persistent settings. Filesystem writes
    // must not race a frame or driver switch; failure keeps the live game intact.
    PakarS3::ScopedLedFlashPause pause;
    if(!pause){result=F("led_flash_guard_busy");return false;}
#endif
    if(!next.save(LittleFS)){result=F("minigame_settings_not_saved");return false;}
    miniBootPending=false;miniSettings=next;miniGame.configure(next.value,now);miniInputFence(now);winner=0;grid.resetWhite();
  }else if(command=="minigame-control"){
    const String action=request["action"]|"";
    if(action=="start"){
      if(!grid.presentationReady()||hostPresentation.model.active()){result=F("presentation_busy");return false;}
      if(!miniGame.start(now,uint32_t(micros())^now)){result=F("game_already_started_or_unconfigured");return false;}
      miniInputFence(now);
      grid.setOutputEnabled(true);
    }else if(action=="reset")miniReset(now);
    else if(action=="stop"){miniRemoteHeld=0;miniGame.stop(now);}
    else if(action=="pause"){if(!miniGame.pause(now)){result=F("game_not_running");return false;}}
    else if(action=="resume"){if(!miniGame.resume(now)){result=F("game_not_paused");return false;}}
    else{result=F("unknown_game_action");return false;}
    miniBootPending=false;
  }else if(command=="minigame-input"){
    if(!request["key"].is<uint8_t>()){result=F("invalid_key");return false;}
    const uint8_t key=request["key"];const String action=request["action"]|"tap";
    if(!(miniGame.view().participatingMask&G::keyBit(key))){result=F("inactive_key");return false;}
    const auto phase=miniGame.phase();
    const bool mayPress=phase==G::Input||(phase==G::Ready&&(miniGame.config().mode==G::PressToWin||miniGame.config().mode==G::Toggle));
    if(action!="up"&&!mayPress){result=F("input_not_accepted_in_current_phase");return false;}
    if(action=="tap"){
      if((miniPhysicalHeld|miniRemoteHeld)&G::keyBit(key)){result=F("key_already_held");return false;}
      miniInput(key,true,false,now);miniInput(key,false,false,now);
    }else if(action=="down"||action=="up")miniInput(key,action=="down",false,now);
    else{result=F("invalid_input_action");return false;}
    // Owner handoff and idempotent release are successful even when the merged
    // level/game revision stays unchanged. Never reject after mutating ownership.
  }else{result=F("unknown_minigame_command");return false;}
  keyObservationPending=true;result=miniJson();return true;
}
void serviceMiniGame(uint32_t now){
  using G=BoardCore::KeyMiniGame;
  if(miniResumeAfterFence&&uint32_t(now-miniFenceAt)>2000){miniResumeAfterFence=false;miniFenceNote=0;}
  if(!miniReady()){
    miniTimerFeedback.update(false,0,0,false,false);
    if(miniGame.running())miniGame.pause(now);
    for(uint8_t k=1;k<=9;++k){
      if(miniRemoteHeld&G::keyBit(k))miniInput(k,false,false,now);
      if(miniPhysicalHeld&G::keyBit(k))miniInput(k,false,true,now);
    }
    miniLayer.enabled=false;return; // Explicit resume after interrupted input ownership.
  }
  // One boot-only admission, after AVR input ownership is acknowledged. A
  // later disconnect never restarts a round. Press-to-win stays Ready/white
  // until the first edge, so its optional timer does not run before a player.
  if(miniBootPending){miniBootPending=false;
    if(miniGame.phase()==G::Ready&&miniGame.config().mode!=G::PressToWin&&miniGame.config().mode!=G::Toggle){
      miniGame.start(now,uint32_t(micros())^now);miniInputFence(now);return;
    }
  }
  if(miniResumeAfterFence){miniResumeAfterFence=false;miniGame.resume(now);}
  for(uint8_t k=1;k<=9;++k)if((miniRemoteHeld&G::keyBit(k))&&uint32_t(now-miniRemoteAt[k-1])>=5000)
    miniInput(k,false,false,now);
  miniGame.tick(now);const auto v=miniGame.view();
  const uint8_t gameWinner=miniGame.config().mode==G::PressToWin?v.winner:0;
  if(winner!=gameWinner){winner=gameWinner;if(winner){grid.showWinner(winner);gameWinnerAt=now;gameAudioPulse=255;gameAudioStarted=false;
      gameAutoResetArmed=keySettings.autoResetMs!=0;gameAutoResetState=gameAutoResetArmed?"armed":"off";}
    else grid.resetWhite();}
  // First-wins retains the established winner flash/ring/pulse presentation.
  miniLayer.enabled=(miniGame.config().mode!=G::PressToWin||v.warning)&&v.phase!=G::Stopped;
  miniLayer.overlay=false;miniGame.styles(miniLayer.entries);
  grid.setGamePresentationEpoch(miniGame.presentationAt());
  static uint32_t revision=UINT32_MAX,lastSecond=UINT32_MAX;
  const uint32_t second=(v.timed?v.remainingMs:v.elapsedMs)/1000;
  if(revision!=v.revision||second!=lastSecond){revision=v.revision;lastSecond=second;
    keyObservationPending=true;grid.presentationChanged();}
  const uint16_t tone=miniFenceNote?miniFenceNote:miniGame.takeNote();miniFenceNote=0;
  const auto timerCue=miniTimerFeedback.update(miniGame.running()&&v.timed,
    v.elapsedMs,v.remainingMs,miniSettings.timerBeeps,miniSettings.timerAlternateTones);
  static G::Phase audioPhase=G::Ready;static uint32_t successAt=0;static bool successCue=false;static uint8_t resultCue=BoardCore::AudioCues::kSilent;
  if(audioPhase!=v.phase){audioPhase=v.phase;
    if(v.phase==G::Success&&miniGame.config().mode!=G::PressToWin){successAt=now;successCue=true;resultCue=keySettings.winnerCue;}
    else if(v.phase==G::Feedback&&miniGame.config().mode==G::MemoryPairs&&!miniGame.feedbackCorrect()){
      successAt=now;successCue=true;resultCue=kCueIncorrect;
    }
    else if(v.phase!=G::Success){successCue=false;
      // First-wins owns its reset/welcome cue in serviceGameLink. Do not cut
      // that cue when its AVR input fence subsequently returns to Ready.
      if(miniGame.config().mode!=G::PressToWin&&(v.phase==G::Ready||v.phase==G::Stopped)){
        const uint8_t silence[]={0,0,1,0};sendToAvr(kBuzzer,sequence++,silence,sizeof(silence));}}
  }
  if(tone&&miniGame.config().mode!=G::PressToWin){
    if(avrAtomicAudio){
      static uint16_t audioId=0x4000;if(++audioId==0)++audioId;
      uint8_t payload[17]={7,uint8_t(audioId),uint8_t(audioId>>8),0,0};uint8_t length=5;
      auto step=[&](uint16_t hz,uint16_t ms){payload[length++]=hz;payload[length++]=hz>>8;payload[length++]=ms;payload[length++]=ms>>8;};
      if(miniGame.config().colorArpeggio){step(tone,55);step(uint32_t(tone)*5/4,55);step(uint32_t(tone)*3/2,100);}
      else step(tone,100);
      sendToAvr(kAudioStream,sequence++,payload,length);
    }else{const uint8_t payload[]={uint8_t(tone),uint8_t(tone>>8),80,0};sendToAvr(kBuzzer,sequence++,payload,sizeof(payload));}
  }
  // Let the completing color finish, then play the configured full winner cue
  // once alongside the board-local success rainbow. No per-note network timing.
  if(successCue&&uint32_t(now-successAt)>=240){successCue=false;if(resultCue!=BoardCore::AudioCues::kSilent)sendToAvr(kAudioCue,sequence++,&resultCue,1);}
  // Idle-only atomic admission in the AVR is authoritative: a tick cannot
  // interrupt any melody, streamed note, pending prefill or winner timeline.
  // Rejection consumes this tick; no retries or legacy stop-and-beep fallback.
  if(timerCue!=BoardCore::GameTimerFeedback::None&&!tone&&!successCue&&avrIdleAudio){
    static uint16_t tickId=0x8000;if(++tickId==0)++tickId;
    const uint16_t hz=timerCue==BoardCore::GameTimerFeedback::Minute?659:
      timerCue==BoardCore::GameTimerFeedback::SecondOdd?1047:880;
    uint8_t payload[17]={8,uint8_t(tickId),uint8_t(tickId>>8),0,0};uint8_t length=5;
    auto step=[&](uint16_t frequency,uint16_t duration){payload[length++]=frequency;payload[length++]=frequency>>8;payload[length++]=duration;payload[length++]=duration>>8;};
    step(hz,timerCue==BoardCore::GameTimerFeedback::Minute?80:35);
    if(timerCue==BoardCore::GameTimerFeedback::Minute){step(0,60);step(hz,80);}
    sendToAvr(kAudioStream,sequence++,payload,length);
  }
  // Set a level, never replay a press. Only differing confirmed outputs cause
  // bounded retries; ACK/readback remains the existing AVR output observation.
  const uint8_t outputs=miniGame.outputMask();
  static uint32_t outputAt=0;static uint8_t desired=0;
  if(outputs!=activeOutputs&&(outputs!=desired||uint32_t(now-outputAt)>=100)){
    if(sendToAvr(kOutputMask,sequence++,&outputs,1,true)){outputAt=now;desired=outputs;}}
}
bool miniPanel(char text[5],uint32_t now){
  using G=BoardCore::KeyMiniGame;if(!miniReady())return false;
  const auto v=miniGame.view();
  if(v.phase==G::Ready)snprintf(text,5,"rEAd");
  else if(v.phase==G::Paused)snprintf(text,5,"PAUS");
  else if(v.phase==G::Stopped)snprintf(text,5,"StOP");
  else if(v.phase==G::Success)snprintf(text,5,"donE");
  else if(v.phase==G::Timeout)snprintf(text,5,"tOut");
  else if(v.phase==G::Failed)snprintf(text,5,"FAIL");
  else if(v.timed){const uint32_t seconds=(v.remainingMs+999)/1000;const uint32_t shown=seconds>5999?5999:seconds;
    snprintf(text,5,"%02u%02u",unsigned(shown/60),unsigned(shown%60));}
  else if(v.target)snprintf(text,5,"b%03u",v.target);
  else if(v.winner)snprintf(text,5,"U%03u",v.winner);
  else snprintf(text,5,"%04u",unsigned((v.elapsedMs/1000)%10000));
  if(v.warning&&(now%(v.warning==2?200:600))>=(v.warning==2?100:300))memcpy(text,"    ",5);
  return true;
}
#endif
