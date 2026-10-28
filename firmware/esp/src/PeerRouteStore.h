#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <PeerRoutePolicy.h>

// Route metadata is not command authority. Only an identity-verified relay
// observation may seed a record. Persistence is an explicit loop-owned action,
// never a discovery callback or periodic worker write. Boot IDs aren't saved.
class PeerRouteStore {
 public:
  static constexpr unsigned capacity=4;
  struct Route {String hostname,board,address;};
  void begin(fs::FS &fs,bool mounted) {
    fs_=mounted?&fs:nullptr;if(!fs_)return;
    auto file=fs_->open("/peer-routes.json","r");
    if(!file||file.size()>2048)return;
    JsonDocument d;if(deserializeJson(d,file)||!d.is<JsonObject>()||!d["routes"].is<JsonArray>()||d["routes"].size()>capacity)return;
    Route next[capacity];unsigned n=0;
    for(auto row:d["routes"].as<JsonArrayConst>()) {
      Route r{row["hostname"]|"",row["board_id"]|"",row["address"]|""};
      if(!valid(r))return;
      for(unsigned i=0;i<n;++i)if(next[i].board==r.board||next[i].hostname==r.hostname)return;
      next[n++]=r;
    }
    for(unsigned i=0;i<capacity;++i)rows_[i]=next[i];
  }
  const Route &at(unsigned i)const{return rows_[i];}
  bool saved(const String &board,const String &host,const String &address)const {
    for(const auto &r:rows_)if(r.board==board&&r.hostname==host&&r.address==address)return true;
    return false;
  }
  const char *update(const Route &route,bool remove) {
    if(!fs_)return "volume_unavailable";
    if(!valid(route))return "invalid_peer_route";
    Route next[capacity];for(unsigned i=0;i<capacity;++i)next[i]=rows_[i];
    unsigned slot=capacity;
    for(unsigned i=0;i<capacity;++i){
      if(next[i].board==route.board)slot=i;
      else if(next[i].hostname==route.hostname)return "peer_hostname_conflict";
    }
    if(remove){if(slot==capacity)return nullptr;next[slot]={};}
    else {
      if(slot==capacity)for(unsigned i=0;i<capacity;++i)if(next[i].board.isEmpty()){slot=i;break;}
      if(slot==capacity)return "peer_routes_full";
      next[slot]=route;
    }
    JsonDocument d;auto list=d["routes"].to<JsonArray>();
    for(const auto &r:next)if(r.board.length()){auto o=list.add<JsonObject>();o["hostname"]=r.hostname;o["board_id"]=r.board;o["address"]=r.address;}
    String raw;serializeJson(d,raw);if(!raw.length())return "route_memory_unavailable";
    auto old=fs_->open("/peer-routes.json","r");const bool same=old&&old.readString()==raw;old.close();
    if(!same) {
      auto file=fs_->open("/peer-routes.pending","w");if(!file)return "route_write_failed";
      const bool wrote=file.print(raw)==raw.length();file.close();if(!wrote)return "route_write_failed";
      file=fs_->open("/peer-routes.pending","r");const bool exact=file&&file.readString()==raw;file.close();
      if(!exact||!fs_->rename("/peer-routes.pending","/peer-routes.json"))return "route_publish_failed";
    }
    for(unsigned i=0;i<capacity;++i)rows_[i]=next[i];
    return nullptr;
  }
 private:
  fs::FS *fs_=nullptr;Route rows_[capacity];
  static bool valid(const Route &r){return BoardCore::PeerRoutePolicy::boardId(r.board.c_str())&&
      BoardCore::PeerRoutePolicy::hostname(r.hostname.c_str())&&BoardCore::PeerRoutePolicy::privateIpv4(r.address.c_str());}
};
