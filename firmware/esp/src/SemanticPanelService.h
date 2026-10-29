#pragma once
// Included by the existing sketch after its settings/transport owners. All
// presentation and event fan-out remains on that loop, never a TCP callback.
void observePanelAction(uint8_t kind,uint16_t value,uint8_t detail) {
  if(mode==kModeSnake)return; // Per-frame game pixels are not human notices.
  char text[33]{};
  using namespace PakarPanel;
  switch(kind) {
    case Cell:
      selectedCell=value;
      snprintf(text,sizeof(text),"C%03u %s %s",value,
        (detail&3)==kCellRed?"RED":(detail&3)==kCellOff?"OFF":"WHITE",
        (detail>>2)==kEffectPulse?"PULSE":(detail>>2)==kEffectFade?"FADE":"SET");break;
    case Pixel:snprintf(text,sizeof(text),"P%03u %s",value,detail==6?"GROUP":detail?"PIXEL":"OFF");break;
    case PakarPanel::Effect:snprintf(text,sizeof(text),"%s",value==LedGrid::kPatternRainbow?"RAIN":
      value==LedGrid::kPatternFire?"FIRE":value==LedGrid::kPatternCellChase?"CHAS":"CELL");break;
    case Output:snprintf(text,sizeof(text),"LED %s",value?"ON":"OFF");break;
    case State:snprintf(text,sizeof(text),"ALL %s",value==kCellRed?"RED":value==kCellOff?"OFF":"WHITE");break;
    default:return;
  }
  semanticPanel.post(static_cast<Kind>(kind),text,value,millis(),kind==Pixel?55:45);
}
String panelEventJson(const PakarPanel::Event &event) {
  JsonDocument d;
  d["type"]="panel-event";d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;
  d["event_id"]=event.id;d["uptime_ms"]=event.at;d["kind"]=PakarPanel::name(event.kind);
  d["value"]=event.value;d["text"]=event.text;d["hardware_confirmed"]=false;
  char prefix[5]={' ',' ',' ',' ',0};
  for(unsigned i=0;i<4&&event.text[i];++i)prefix[i]=event.text[i];
  d["presentation"]["text"]=prefix;
  d["presentation"]["lifetime_ms"]=event.lifetime;
  d["presentation"]["blink"]=PakarPanel::name(event.blink);
  d["presentation"]["marquee"]=strlen(event.text)>4?"once":"none";
  d["presentation"]["priority"]=event.priority;
  String result;serializeJson(d,result);return result;
}
String panelStatusJson() {
  JsonDocument d;const auto now=millis();char text[5];semanticPanel.view(now,text);
  d["board_id"]=pcBoardId;d["boot_id"]=pcBootId;d["requested_text"]=text;
  d["event_id"]=semanticPanel.revision();d["dropped_events"]=semanticPanel.dropped();
  d["cloud_display_coalesced"]=displayCloudCoalesced;
  d["cloud_display_pending"]=pendingDisplayCloud.length()!=0;
  d["manual_owner"]=semanticPanel.manualActive(now)||avrCaptured;
  d["menu_active"]=avrMenuActive;d["hardware_confirmed"]=false;
  d["observation"]["fresh"]=tm1637DetectionKnown();
  d["observation"]["ack"]=tm1637Ack;
  d["observation"]["avr_session"]=avrSession;
  for(uint8_t segment:tm1637Segments)d["observation"]["segments"].add(segment);
  d["events"].to<JsonArray>();
  for(unsigned i=0;i<semanticPanel.historyCount();++i) {
    JsonDocument event;deserializeJson(event,panelEventJson(semanticPanel.history(i)));
    d["events"].add(event.as<JsonVariantConst>());
  }
  String result;serializeJson(d,result);return result;
}
void serviceSemanticPanel(uint32_t now) {
  using namespace PakarPanel;
  static bool initialized=false,lastAp=false,lastCl=false,lastAvr=false;
  static uint8_t lastMode=255,lastWinner=0,lastMenu=255;
  static bool lastMenuActive=false;
  static int lastScan=-2;
  static uint32_t lastUpdateAt=0;
  static int16_t lastProgress=-2;
  static PakarUpdate::Visual lastVisual=PakarUpdate::Visual::None;
  char text[5];
  if(!grid.outputEnabled())snprintf(text,sizeof(text),"OFF ");
  else if(grid.diagnosticActive())snprintf(text,sizeof(text),"P%03u",grid.diagnosticPixel());
  else if(mode==kModeFirstWins) {
    if(winner)snprintf(text,sizeof(text),"U%03u",winner);else snprintf(text,sizeof(text),"rEAd");
  } else if(grid.pattern()!=LedGrid::kPatternNone)snprintf(text,sizeof(text),"%s",
    grid.pattern()==LedGrid::kPatternRainbow?"rAIn":grid.pattern()==LedGrid::kPatternFire?"FIrE":"CHAS");
  else snprintf(text,sizeof(text),"C%03u",selectedCell);
#if PAKAR_FIRST_WINS_ENABLED
  if(!grid.diagnosticActive()&&grid.outputEnabled())miniPanel(text,now);
#endif
  semanticPanel.resting(text);
  semanticPanel.restingSegments(false,nullptr);
#if PAKAR_FIRST_WINS_ENABLED
  if(miniSettings.segmentAnimation&&miniReady()&&miniGame.phase()==BoardCore::KeyMiniGame::Ready&&
      grid.outputEnabled()&&!grid.diagnosticActive()){
    uint8_t segments[4];BoardCore::puzzlePanelSegments(now,segments);
    semanticPanel.restingSegments(true,segments);
  }
#endif
  const bool cl=networkConfig.stationConnected();
  if(initialized) {
    if(lastAp!=apStarted)semanticPanel.post(PakarPanel::Network,apStarted?"AP ON":"AP OFF",apStarted,now,30);
    if(lastCl!=cl)semanticPanel.post(PakarPanel::Network,cl?"CL UP":"CL DOWN",cl,now,35,cl?Steady:Slow);
    if(lastAvr!=avrOnline)semanticPanel.post(PakarPanel::Network,avrOnline?"AVR UP":"AVR DOWN",avrOnline,now,65,avrOnline?Steady:Slow);
    if(lastMode!=mode)semanticPanel.post(Mode,mode==kModeFirstWins?"FIRST WINS":mode==kModeSnake?"SNAKE":"CELL",mode,now,50);
    if(winner && winner!=lastWinner) {
      char notice[5];snprintf(notice,sizeof(notice),"U%03u",winner);semanticPanel.post(Complete,notice,winner,now,70,DoubleFast);
    }
    if(avrMenuActive!=lastMenuActive || (avrMenuActive&&lastMenu!=avrMenuPage))
      semanticPanel.post(Menu,avrMenuActive?"MENU":"HOME",avrMenuPage,now,25);
  } else semanticPanel.post(Rest,text,selectedCell,now,0);
  lastAp=apStarted;lastCl=cl;lastAvr=avrOnline;lastMode=mode;lastWinner=winner;
  lastMenu=avrMenuPage;lastMenuActive=avrMenuActive;initialized=true;
  const int scan=WiFi.scanComplete();
  if(scan!=lastScan) {
    if(scan==-1)semanticPanel.post(Scan,"SCAN",0,now,30,Slow);
    else if(lastScan==-1) {
      char notice[16];snprintf(notice,sizeof(notice),"%s %d",scan<0?"SCAN ERR":"SSIDS",scan<0?0:scan);
      semanticPanel.post(Scan,notice,scan<0?0:scan,now,30,scan<0?DoubleFast:Steady);
    }
    lastScan=scan;
  }
  const auto visual=upgradeLifecycle.visual(now);
  const bool updating=otaActive||otaRebootAt||remoteFirmware.busy()||urbootBusy.get();
  const int16_t progress=updating&&upgradeLifecycle.known()?upgradeLifecycle.percent():-1;
  if((updating && progress!=lastProgress && uint32_t(now-lastUpdateAt)>=250U)||visual!=lastVisual) {
    if(updating) {
      if(progress>=0)snprintf(text,sizeof(text),"P%03u",unsigned(progress));else snprintf(text,sizeof(text),"UPdt");
      semanticPanel.post(Progress,text,progress<0?65535:progress,now,90,progress<0?Slow:Steady);
      lastProgress=progress;lastUpdateAt=now;
    } else if(visual==PakarUpdate::Visual::Failure)semanticPanel.post(Fault,"Err ",0,now,95,DoubleFast,6000);
    else if(visual==PakarUpdate::Visual::Success)semanticPanel.post(Complete,"donE",100,now,80,DoubleFast);
    lastVisual=visual;
  }
  if(updating) {
    if(progress>=0)snprintf(text,sizeof(text),"P%03u",unsigned(progress));else snprintf(text,sizeof(text),"UPdt");
  }
  bool gameCountdown=false;
#if PAKAR_FIRST_WINS_ENABLED
  if(!updating&&miniReady()&&miniGame.view().timed&&miniGame.running())gameCountdown=miniPanel(text,now);
#endif
  semanticPanel.persistent(updating||gameCountdown,text);
  uint8_t payload[12];
  const bool inhibited=urbootRawOwned||programmingLatched||avrUploadOwner||avrCaptured||
    (avrMenuActive&&!updating)||semanticPanel.manualActive(now);
  if(semanticPanel.frame(now,tm1637DetectionKnown()&&tm1637Ack,inhibited,payload)) {
    if(updating&&avrMaintenancePanel)payload[4]|=0x04U;
    semanticPanelSending=true;
    if(sendToAvr(kDisplayText,sequence++,payload,sizeof(payload)))semanticPanel.admitted(now,payload);
    semanticPanelSending=false;
  }
  // ACKs/command replies take precedence. Do not wait for USB space or emit
  // unframed JSON to the binary serial port. One event always fits one frame.
  for(unsigned budget=0;budget<2;++budget) {
    const bool usb=hostDemand.wants(BoardCore::Protocol::UsbDemand::Panel,now);
    if(usb && hostSerial.availableForWrite()<kMaximumEncoded)break;
    Event event;if(!semanticPanel.pop(event))break;
    if(webSocket.count())webSocket.textAll(panelEventJson(event));
    if(usb){uint8_t wire[48];const auto length=encode(event,wire);hostLink.send(kPanelEvent,event.id,wire,length);}
  }
}
