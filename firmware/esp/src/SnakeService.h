#pragma once
// Included after the common presentation dispatcher. Optional game is an
// independent volatile layer, never a business mode or a saved cell scene.
#if PAKAR_SNAKE_ENABLED
#include <SnakeGame.h>
BoardCore::SnakeGame boardSnake;
bool boardSnakeAllowed=false;
uint32_t boardSnakeLease=0,boardSnakeRevision=0,boardSnakeLayout=0,boardSnakeSentAt=0;
const char *boardSnakeError=nullptr;
void beginBoardSnake(bool mounted){
  if(!mounted)return;auto f=LittleFS.open("/snake-enabled.txt","r");
  boardSnakeAllowed=f&&f.size()==1&&f.read()=='1';
}
bool boardSnakeOwns(){return boardSnakeLease&&hostPresentation.model.lease==boardSnakeLease&&
  hostPresentation.model.revision==boardSnakeRevision&&hostPresentation.model.temporary.enabled&&
  grid.groupLayout().revision()==boardSnakeLayout&&
  !strcmp(hostPresentation.model.temporary.owner,"board-snake");}
bool publishBoardSnake(bool clear=false){
  if(boardSnakeLease&&!boardSnakeOwns()){boardSnakeError="presentation_ownership_changed";boardSnake.stop();boardSnakeLease=0;return false;}
  JsonDocument packet;packet["revision"]=hostPresentation.model.revision;
  packet["group_layout_revision"]=grid.groupLayout().revision();packet["owner"]="board-snake";
  packet["lifetime"]="temporary";packet["ttl_ms"]=uint16_t(3000);packet["space"]="cells";
  packet["composite"]="replace";packet["replace"]=true;
  if(boardSnakeLease)packet["lease_id"]=boardSnakeLease;
  if(clear)packet["action"]="clear";
  else{
    auto entries=packet["entries"].to<JsonArray>();
    if(boardSnake.food<80){auto row=entries.add<JsonObject>();row["id"]=BoardCore::SnakeGame::logical(boardSnake.food);row["rgb"]=0x27e991U;}
    for(uint8_t i=0;i<boardSnake.length;++i){auto row=entries.add<JsonObject>();row["id"]=BoardCore::SnakeGame::logical(boardSnake.body[i]);row["rgb"]=i?0x125aedU:0x80c4ffU;}
  }
  boardSnakeError=applyHostPresentation(packet.as<JsonObjectConst>());
  if(boardSnakeError){boardSnake.stop();return false;}
  boardSnakeLease=clear?0:hostPresentation.model.lease;boardSnakeRevision=hostPresentation.model.revision;
  boardSnakeLayout=grid.groupLayout().revision();boardSnakeSentAt=millis();return true;
}
bool boardSnakeCommand(JsonObjectConst request,String &result){
  const char *action=request["action"]|"status";
  if(!strcmp(action,"configure")){
    if(!request["enabled"].is<bool>()||!maintenanceApiIdle()){result="invalid_snake_setting_or_busy";return false;}
    if(boardSnakeLease){result="stop_snake_before_configuring";return false;}
    const bool allowed=request["enabled"];
    if(allowed!=boardSnakeAllowed){
#if defined(ARDUINO_ARCH_ESP32)
      PakarS3::ScopedLedFlashPause pause;if(!pause){result="led_flash_guard_busy";return false;}
#endif
      auto f=LittleFS.open("/snake-enabled.tmp","w");
      if(!f||f.write(allowed?'1':'0')!=1){result="snake_save_failed";return false;}f.close();
      auto check=LittleFS.open("/snake-enabled.tmp","r");const bool valid=check&&check.size()==1&&check.read()==(allowed?'1':'0');check.close();
      if(!valid||!LittleFS.rename("/snake-enabled.tmp","/snake-enabled.txt")){result="snake_save_failed";return false;}
      boardSnakeAllowed=allowed;
    }
    result="snake_setting_saved";return true;
  }
  if(!strcmp(action,"status")){result="snake_status";return true;}
  if(!boardSnakeAllowed){result="snake_disabled";return false;}
  if(!strcmp(action,"start")||!strcmp(action,"restart")){
    if(hostPresentation.model.active()&&!boardSnakeOwns()){result="presentation_owned";return false;}
    if(!request["interval_ms"].isNull()){
      if(!request["interval_ms"].is<uint16_t>()||request["interval_ms"].as<uint16_t>()<80||request["interval_ms"].as<uint16_t>()>2000){result="invalid_interval";return false;}
      boardSnake.interval=request["interval_ms"];
    }
    if(!strcmp(action,"start")&&boardSnakeLease){result="snake_already_started";return false;}
    boardSnake.start(millis());if(!publishBoardSnake()){result=boardSnakeError;return false;}
  }else if(!strcmp(action,"stop")){
    boardSnake.stop();if(boardSnakeLease&&!publishBoardSnake(true)){result=boardSnakeError;return false;}
  }else if(!strcmp(action,"pause")){boardSnake.pause();}
  else if(!strcmp(action,"resume")){boardSnake.resume(millis());}
  else if(!strcmp(action,"direction")){
    if(!request["direction"].is<uint8_t>()||!boardSnake.turn(request["direction"].as<uint8_t>())){result="invalid_direction";return false;}
  }else{result="unknown_snake_action";return false;}
  snakeAlive=boardSnake.phase==BoardCore::SnakeGame::Running||boardSnake.phase==BoardCore::SnakeGame::Paused;
  snakeLength=boardSnake.length;keyObservationPending=true;result="snake_updated";return true;
}
void serviceSnake(uint32_t now){
  if(!boardSnakeLease)return;
  if(!boardSnakeOwns()){boardSnake.stop();boardSnakeLease=0;snakeAlive=false;keyObservationPending=true;return;}
  if(boardSnake.service(now)||uint32_t(now-boardSnakeSentAt)>=900){
    if(!publishBoardSnake()){snakeAlive=false;return;}
    snakeLength=boardSnake.length;snakeAlive=boardSnake.phase==BoardCore::SnakeGame::Running||boardSnake.phase==BoardCore::SnakeGame::Paused;
    if(!snakeAlive)publishBoardSnake(true);
    keyObservationPending=true;
  }
}
#else
void beginBoardSnake(bool){}
bool boardSnakeCommand(JsonObjectConst,String &result){result="snake_not_built";return false;}
void serviceSnake(uint32_t){}
#endif
String boardSnakeJson(){
  JsonDocument doc;doc["built"]=bool(PAKAR_SNAKE_ENABLED);
#if PAKAR_SNAKE_ENABLED
  doc["enabled"]=boardSnakeAllowed;doc["owner"]="board";
  static const char *const phases[]={"ready","running","paused","over","won","stopped"};
  doc["phase"]=phases[boardSnake.phase];doc["length"]=boardSnake.length;doc["score"]=boardSnake.score;
  doc["direction"]=boardSnake.direction;doc["interval_ms"]=boardSnake.interval;
  auto body=doc["body"].to<JsonArray>();for(uint8_t i=0;i<boardSnake.length;++i)body.add(BoardCore::SnakeGame::logical(boardSnake.body[i]));
  if(boardSnake.food<80)doc["food"]=BoardCore::SnakeGame::logical(boardSnake.food);
  if(boardSnakeError)doc["error"]=boardSnakeError;
#else
  doc["enabled"]=false;
#endif
  String text;serializeJson(doc,text);return text;
}
// Legacy mode/input paths cannot launch this optional presentation implicitly.
void startSnake(){}
bool setSnakeDirection(uint8_t direction){
#if PAKAR_SNAKE_ENABLED
  return boardSnakeAllowed&&boardSnakeLease&&boardSnake.turn(direction);
#else
  (void)direction;return false;
#endif
}
