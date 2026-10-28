#pragma once
// One descriptor/validator owner for TUI, get/set and JSON management.
struct ShellTuiSnapshot {
  String values[16];PakarShell::ShellTui::Row rows[16];uint8_t cells[80]{};
  uint8_t groupCounts[LedGrid::kCells]{};uint16_t groupStarts[LedGrid::kCells]{};
  PakarShell::ShellTui::Snapshot view;
  explicit ShellTuiSnapshot(DeviceShellSession &session){
    using T=PakarShell::ShellTui;
    view.title=deviceIdentity.hostname();view.rows=rows;view.cellStates=cells;
    view.columns=session.parser.columns();view.lines=session.parser.rows();view.unicode=session.unicode;
    view.terminalSettings=true;
    view.cloudSettings=true;
#if defined(ARDUINO_ARCH_ESP32)
    view.notificationSettings=true;
#endif
#if PAKAR_FIRST_WINS_ENABLED
    view.canGame=true;view.gameReady=avrOnline&&avrRemotePress&&keyMaskApplied&&!shellBusy();
    view.enabledKeys=keySettings.enabledMask;view.winner=winner;
#endif
    view.canLeds=(currentFeatures()&BoardCore::Context::Table)!=0;
    view.canPixels=(currentFeatures()&BoardCore::Context::Leds)!=0;view.pixelCount=view.canPixels?LedGrid::kPixels:0;
    if(view.canPixels){
      const auto &layout=grid.groupLayout();
      view.groupCount=LedGrid::kCells;view.usedPixels=layout.used();view.groupLayoutRevision=layout.revision();
      view.groupCounts=groupCounts;view.groupStarts=groupStarts;
      for(uint8_t i=0;i<view.groupCount;++i){groupCounts[i]=layout.count(i+1);groupStarts[i]=layout.start(i+1);}
    }
    const char *device[]={"hostname","label","version","boot_id","uptime","heap","avr.online","display.ack","firmware.phase","shell.enabled","shell.port","shell.color","firmware.hash","build.date"};
    const char *network[]={"network.mode","network.station_enabled","network.ap_enabled","network.auto_switch","network.dhcp","network.ssid","network.password","network.address","network.gateway","network.subnet","network.dns1","network.dns2","network.ip","network.ap_ip","network.ap_ssid"};
    const char *cloud[]={"mqtt.enabled","mqtt.host","mqtt.port","mqtt.tls","mqtt.user","mqtt.password","mqtt.topic","websocket.enabled","websocket.host","websocket.port","websocket.tls","websocket.path","websocket.token","telemetry.seconds"};
    const char *notifications[]={"ntfy.enabled","ntfy.host","ntfy.topic","ntfy.token"};
    const char *led[]={"led.enabled","led.mode","led.brightness","led.effect","led.backend","led.rmt","led.dma"};
    const char *pixel[]={"led.enabled","led.mode","led.brightness","led.effect","led.backend","led.rmt","led.dma"};
    const char *game[]={"game.enabled_mask","game.effects","game.auto_reset_ms","game.chase_ms","game.neighbors","game.invert","game.idle_rgb","game.win_rgb","game.lose_rgb","game.winner_cue","game.reset_cue"};
    const char **list=device;size_t count=sizeof(device)/sizeof(*device);
    if(session.tui.page()==T::Network){list=network;count=sizeof(network)/sizeof(*network);}
    else if(session.tui.page()==T::Led){list=led;count=sizeof(led)/sizeof(*led);}
    else if(session.tui.page()==T::Pixel){list=pixel;count=sizeof(pixel)/sizeof(*pixel);}
    else if(session.tui.page()==T::Game){list=game;count=sizeof(game)/sizeof(*game);}
    else if(session.tui.page()==T::Cloud){list=cloud;count=sizeof(cloud)/sizeof(*cloud);}
    else if(session.tui.page()==T::Notifications){list=notifications;count=sizeof(notifications)/sizeof(*notifications);}
    else if(session.tui.page()==T::TerminalPage){
      values[0]=session.parser.terminal();values[1]=String(session.parser.columns());values[2]=String(session.parser.rows());
      values[3]=session.unicode?"true":"false";values[4]=session.color?"true":"false";
      const char *labels[]={"Terminal type","Columns","Rows","Unicode","ANSI color"};
      const char *keys[]={"terminal.type","terminal.columns","terminal.rows","terminal.unicode","terminal.color"};
      view.rowCount=5;for(size_t i=0;i<5;++i)rows[i]={labels[i],keys[i],values[i].c_str(),i!=0,false};return;
    }
    view.rowCount=count;
    for(size_t i=0;i<count;++i){ShellProperty property;const bool known=shellProperty(list[i],property);
      values[i]=known?shellGet(property):String("unknown");rows[i]={PakarShell::propertyLabel(list[i]),list[i],values[i].c_str(),known&&property.writable&&!shellBusy(),known&&property.secret};}
    for(uint8_t i=0;i<80;++i)cells[i]=grid.state(i+1);
  }
};
void shellTuiDraw(DeviceShellSession &s,bool force){
  ShellTuiSnapshot state(s);s.tui.setAnsi(s.color&&s.parser.ansi());s.tui.render(state.view,[&](const uint8_t *p,size_t n){s.write(p,n);},force);
}
void shellTuiInput(DeviceShellSession &s,uint8_t ch,bool flushEscape){
  using T=PakarShell::ShellTui;ShellTuiSnapshot state(s);T::Action action;
  const bool emitted=flushEscape?s.tui.flushEscape(action):s.tui.feed(ch,state.view,action);
  if(emitted){
    if(action.type==T::Action::Exit){if(s.alternateScreen)s.print(F("\x1b[0m\x1b[?1049l"));s.alternateScreen=false;shellPrompt(s);return;}
    String result;bool accepted=true;
    if(action.type==T::Action::Set){
      const String key(action.key),value(action.value);
      if(key.startsWith("terminal.")){
        if(key=="terminal.unicode"||key=="terminal.color"){
          accepted=value=="true"||value=="false";
          if(accepted){if(key=="terminal.unicode")s.unicode=value=="true";else s.color=value=="true";}
        }else{uint32_t number=0;accepted=PakarShell::unsignedNumber(value.c_str(),240,number)&&
          s.parser.size(key=="terminal.columns"?number:s.parser.columns(),key=="terminal.rows"?number:s.parser.rows());}
        result=accepted?F("Session terminal settings applied"):F("Invalid terminal setting");
      }else accepted=shellSet(action.key,action.value,result);
    }
    else if(action.type==T::Action::Cell||action.type==T::Action::Pixel||action.type==T::Action::Press||action.type==T::Action::Reset){
      if(shellBusy()){accepted=false;result=F("Maintenance active; nothing dispatched");}
      else{
        JsonDocument d;
        if(action.type==T::Action::Press){d["command"]="game-press";d["key"]=action.index;}
        else if(action.type==T::Action::Reset)d["command"]="reset";
        else if(action.type==T::Action::Cell){d["command"]="cell";d["id"]=action.index;d["state"]=action.value;d["effect"]="immediate";}
        else{d["command"]="pixel-diagnostic";d["index"]=action.index;d["count"]=action.count;d["group_layout_revision"]=action.groupLayoutRevision;
          d["rgb"]=!strcmp(action.value,"red")?0xff0000:0xffffff;d["off"]=!strcmp(action.value,"off");}
        String json;serializeJson(d,json);accepted=handleCloudCommand(json,result);
      }
    }else result=F("Refreshed observations");
    s.tui.feedback((String(accepted?"OK ":"ERR ")+result).c_str());
  }
  // Render once after the transport's input batch, not for each arrow byte.
}
