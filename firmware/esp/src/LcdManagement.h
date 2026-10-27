#pragma once
// Included in the loop-owner namespace. No I2C operation in async callbacks.
String lcdStatusJson() {
  JsonDocument d;
#if defined(ARDUINO_ARCH_ESP32)
  const auto &lcd=lcdPort.display();
  d["configured"]=lcdPort.configured();d["started"]=lcdPort.started();
  d["detection_known"]=lcd.detectionKnown();d["backpack_ack"]=lcd.detected();
  d["initialized"]=lcd.initialized();d["available"]=lcd.available();
  if(lcd.detected())d["address"]=lcd.address();else d["address"]=nullptr;
  d["columns"]=16;d["rows"]=2;d["host_active"]=lcdPort.hostActive();
  d["pending"]=lcd.pending();d["requested_revision"]=lcd.revision();
  d["presented_revision"]=lcd.presentedRevision();d["backlight"]=lcd.backlight();
  if(lcd.available()) {
    for(uint8_t row=0;row<2;++row){char line[17];memcpy(line,lcd.line(row),16);line[16]=0;d["lines"][row]=line;}
  }else d["lines"]=nullptr;
  if(lcd.error())d["last_error"]=lcd.error();else d["last_error"]=nullptr;
#else
  d["configured"]=false;
#endif
  return ConfigurationManagement::json(d);
}
void registerLcdApi() {
  server.on("/api/display/lcd",HTTP_GET,[](){if(authenticate())server.send(200,"application/json",lcdStatusJson());});
  server.on("/api/display/lcd",HTTP_POST,[](){
    if(!authenticate())return;
#if defined(ARDUINO_ARCH_ESP32)
    if(!lcdPort.configured()||!lcdPort.started()){server.send(503,"text/plain","LCD port is unavailable");return;}
    if(ConfigurationManagement::busy()){server.send(409,"text/plain","Maintenance owns this device");return;}
    const String action=server.arg("action");
    if(action=="rescan")lcdPort.rescan(millis());
    else if(action=="release")lcdPort.release();
    else {
      const String one=server.arg("line1"),two=server.arg("line2");
      const auto printable=[](const String &s){if(s.length()>16)return false;for(size_t i=0;i<s.length();++i)if(s[i]<0x20||s[i]>0x7e)return false;return true;};
      const String duration=server.hasArg("hold_ms")?server.arg("hold_ms"):String("5000");
      if(action!="present"||!printable(one)||!printable(two)||duration.isEmpty()||duration.length()>5){server.send(400,"text/plain","Use present, two ASCII rows of at most16 characters and hold_ms1..60000");return;}
      uint32_t hold=0;for(size_t i=0;i<duration.length();++i){if(duration[i]<'0'||duration[i]>'9'){server.send(400,"text/plain","Invalid hold_ms");return;}hold=hold*10U+duration[i]-'0';}
      if(!hold||hold>60000U){server.send(400,"text/plain","Invalid hold_ms");return;}
      if(!lcdPort.present(one.c_str(),two.c_str(),hold,millis())){server.send(503,"text/plain","LCD is not initialized and acknowledged");return;}
    }
    // Receipt is admission, not display confirmation. Poll presented_revision.
    server.send(202,"application/json",lcdStatusJson());
#else
    server.send(503,"text/plain","LCD port is not compiled for this target");
#endif
  });
}
