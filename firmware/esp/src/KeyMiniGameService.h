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
  auto modes=d["available_modes"].to<JsonArray>();
  constexpr const char *labels[]={"Press to win","Illuminated target","Color pattern","Repeat sequence","Matching pairs",
    "Clear all","Color order","Switch pattern","Reaction chase","Toggle"};
  for(uint8_t i=0;i<G::ModeCount;++i)if(G::available(G::Mode(i))){auto mode=modes.add<JsonObject>();mode["id"]=G::name(G::Mode(i));mode["label"]=labels[i];}
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
void miniReset(uint32_t now){miniGame.reset(now);miniInputFence(now);winner=0;grid.resetWhite();gameAutoResetArmed=false;keyObservationPending=true;}
bool miniCommand(JsonObjectConst request,String &result){
  using G=BoardCore::KeyMiniGame;
  if(!miniReady()){result=F("minigame_avr_ownership_not_ready");return false;}
  const String command=request["command"]|"";const uint32_t now=millis();
  if(command=="minigame-configure"){
    KeyMiniGameSettings next=miniSettings;next.value.enabledMask=keySettings.enabledMask;
    if(!next.patch(request["settings"])){result=F("invalid_minigame_settings");return false;}
    if(!grid.presentationReady()||hostPresentation.model.active()){result=F("presentation_busy");return false;}
    if(!next.save(LittleFS)){result=F("minigame_settings_not_saved");return false;}
    miniSettings=next;miniGame.configure(next.value,now);miniInputFence(now);winner=0;grid.resetWhite();
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
    if(miniGame.running())miniGame.pause(now);
    for(uint8_t k=1;k<=9;++k){
      if(miniRemoteHeld&G::keyBit(k))miniInput(k,false,false,now);
      if(miniPhysicalHeld&G::keyBit(k))miniInput(k,false,true,now);
    }
    miniLayer.enabled=false;return; // Explicit resume after interrupted input ownership.
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
  static uint32_t revision=UINT32_MAX,lastSecond=UINT32_MAX;
  const uint32_t second=(v.timed?v.remainingMs:v.elapsedMs)/1000;
  if(revision!=v.revision||second!=lastSecond){revision=v.revision;lastSecond=second;
    keyObservationPending=true;grid.presentationChanged();}
  const uint16_t tone=miniFenceNote?miniFenceNote:miniGame.takeNote();miniFenceNote=0;
  if(tone&&miniGame.config().mode!=G::PressToWin){
    const uint8_t payload[]={uint8_t(tone),uint8_t(tone>>8),80,0};sendToAvr(kBuzzer,sequence++,payload,sizeof(payload));
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
