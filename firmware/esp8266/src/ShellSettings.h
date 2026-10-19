#pragma once
#include <Arduino.h>
#include <FS.h>
#include <stddef.h>
#include <string.h>
#include "ConfigurationChanges.h"

// Adapt NetworkConfigStore/CloudConfigStore's exact-layout, checksum,
// identical-save suppression and pending/readback/rename transaction.
class ShellSettings {
 public:
  bool enabled()const{return record_.enabled;}
  bool color()const{return record_.color;}
  uint16_t port()const{return record_.port;}
  bool begin(fs::FS &fs){
    fs_=&fs;File file=fs.open("/shell.bin","r");if(!file)return true;
    Record next{};const bool ok=file.size()==sizeof(next)&&file.read(reinterpret_cast<uint8_t*>(&next),sizeof(next))==sizeof(next);
    file.close();if(!ok||!valid(next)){record_.enabled=0;return false;}record_=next;return true;
  }
  bool configure(bool enabled,uint16_t port,bool color){
    Record next=record_;next.enabled=enabled;next.port=port;next.color=color;next.check=sum(next);
    if(!fs_||!valid(next))return false;
    File current=fs_->open("/shell.bin","r");Record previous{};
    const bool identical=current&&current.size()==sizeof(previous)&&
      current.read(reinterpret_cast<uint8_t*>(&previous),sizeof(previous))==sizeof(previous)&&!memcmp(&previous,&next,sizeof(next));
    if(current){current.close();}
    if(identical){record_=next;return true;}
    File pending=fs_->open("/shell.pending","w");if(!pending)return false;
    const bool written=pending.write(reinterpret_cast<const uint8_t*>(&next),sizeof(next))==sizeof(next);pending.close();
    File verify=fs_->open("/shell.pending","r");Record observed{};
    const bool readback=verify&&verify.size()==sizeof(observed)&&
      verify.read(reinterpret_cast<uint8_t*>(&observed),sizeof(observed))==sizeof(observed)&&!memcmp(&next,&observed,sizeof(next));
    if(verify){verify.close();}
    if(!written||!readback||!fs_->rename("/shell.pending","/shell.bin"))return false;
    record_=next;PakarConfiguration::changes().saved(PakarConfiguration::Shell);return true;
  }
 private:
  struct Record {uint32_t magic;uint8_t layout,enabled;uint16_t port;uint8_t color;uint32_t check;} __attribute__((packed));
  static uint32_t sum(const Record &r){uint32_t hash=2166136261UL;const auto *p=reinterpret_cast<const uint8_t*>(&r);
    for(size_t i=0;i<offsetof(Record,check);++i){hash^=p[i];hash*=16777619UL;}return hash;}
  static bool valid(const Record &r){return r.magic==0x504b5348&&r.layout==1&&r.enabled<=1&&r.color<=1&&r.port&&
    r.port!=80&&r.port!=443&&r.port!=7777&&r.check==sum(r);}
  Record record_{0x504b5348,1,1,23,1,0};fs::FS *fs_=nullptr;
};
