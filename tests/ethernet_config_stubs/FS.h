#pragma once
// Reuse existing transactional FS fault model; only classify Ethernet's pending
// path as a verification read. Production header still uses the real FS API.
#define fs CloudFixtureFs
#include "../cloud_stubs/FS.h"
#undef fs
namespace fs {
class FS : public CloudFixtureFs::FS {
 public:
  File open(const char* path,const char* mode) {
    if(std::string(path)!="/ethernet.pending")return CloudFixtureFs::FS::open(path,mode);
    File result=CloudFixtureFs::FS::open(path,mode);
    if(!result)return result;
    return File(files.at(path),faults,true);
  }
};
}
