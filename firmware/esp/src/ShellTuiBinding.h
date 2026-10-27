#pragma once
// One descriptor/validator owner for TUI, get/set and JSON management.
struct ShellTuiSnapshot {
  String values[16];PakarShell::ShellTui::Row rows[16];uint8_t cells[80]{};
  uint8_t groupCounts[LedGrid::kCells]{};uint16_t groupStarts[LedGrid::kCells]{};
  PakarShell::ShellTui::Snapshot view;
  explicit ShellTuiSnapshot(DeviceShellSession &session){
    using T=PakarShell::ShellTui;
    view.title=deviceIdentity.hostname();view.rows=rows;view.cellStates=cells;
    view.canLeds=(currentFeatures()&BoardCore::Context::Table)!=0;
    view.canPixels=(currentFeatures()&BoardCore::Context::Leds)!=0;view.pixelCount=view.canPixels?LedGrid::kPixels:0;
    if(view.canPixels){
      const auto &layout=grid.groupLayout();
      view.groupCount=LedGrid::kCells;view.usedPixels=layout.used();view.groupLayoutRevision=layout.revision();
      view.groupCounts=groupCounts;view.groupStarts=groupStarts;
      for(uint8_t i=0;i<view.groupCount;++i){groupCounts[i]=layout.count(i+1);groupStarts[i]=layout.start(i+1);}
    }
    const char *device[]={"hostname","label","version","boot_id","uptime","heap","avr.online","display.ack","firmware.phase","shell.enabled","shell.port","shell.color"};
    const char *network[]={"network.mode","network.dhcp","network.ssid","network.password","network.address","network.gateway","network.subnet","network.dns1","network.dns2","network.ip","network.ap_ip","mqtt.enabled","mqtt.host","mqtt.port","mqtt.tls","telemetry.seconds"};
    const char *led[]={"led.enabled","led.mode","led.brightness","led.effect","led.backend","led.rmt","led.dma"};
    const char *pixel[]={"led.enabled","led.mode","led.brightness","led.backend","led.rmt","led.dma"};
    const char **list=device;size_t count=sizeof(device)/sizeof(*device);
    if(session.tui.page()==T::Network){list=network;count=sizeof(network)/sizeof(*network);}
    else if(session.tui.page()==T::Led){list=led;count=sizeof(led)/sizeof(*led);}
    else if(session.tui.page()==T::Pixel){list=pixel;count=sizeof(pixel)/sizeof(*pixel);}
    view.rowCount=count;
    for(size_t i=0;i<count;++i){ShellProperty property;const bool known=shellProperty(list[i],property);
      values[i]=known?shellGet(property):String("unknown");rows[i]={list[i],list[i],values[i].c_str(),known&&property.writable&&!shellBusy(),known&&property.secret};}
    for(uint8_t i=0;i<80;++i)cells[i]=grid.state(i+1);
  }
};
void shellTuiDraw(DeviceShellSession &s,bool force){
  ShellTuiSnapshot state(s);s.tui.render(state.view,[&](const uint8_t *p,size_t n){s.write(p,n);},force);
}
void shellTuiInput(DeviceShellSession &s,uint8_t ch,bool flushEscape){
  using T=PakarShell::ShellTui;ShellTuiSnapshot state(s);T::Action action;
  const bool emitted=flushEscape?s.tui.flushEscape(action):s.tui.feed(ch,state.view,action);
  if(emitted){
    if(action.type==T::Action::Exit){s.print(F("\x1b[0m\x1b[H\x1b[2J"));shellPrompt(s);return;}
    String result;bool accepted=true;
    if(action.type==T::Action::Set)accepted=shellSet(action.key,action.value,result);
    else if(action.type==T::Action::Cell||action.type==T::Action::Pixel){
      if(shellBusy()){accepted=false;result=F("Maintenance active; nothing dispatched");}
      else{
        JsonDocument d;
        if(action.type==T::Action::Cell){d["command"]="cell";d["id"]=action.index;d["state"]=action.value;d["effect"]="immediate";}
        else{d["command"]="pixel-diagnostic";d["index"]=action.index;d["count"]=action.count;d["group_layout_revision"]=action.groupLayoutRevision;
          d["rgb"]=!strcmp(action.value,"red")?0xff0000:0xffffff;d["off"]=!strcmp(action.value,"off");}
        String json;serializeJson(d,json);accepted=handleCloudCommand(json,result);
      }
    }else result=F("Refreshed observations");
    s.tui.feedback((String(accepted?"OK ":"ERR ")+result).c_str());
  }
  // Render once after the transport's input batch, not for each arrow byte.
}
