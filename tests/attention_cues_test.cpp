#include <AttentionCues.h>
#include <cassert>
#include <cstdio>
int main() {
  using namespace BoardCore::Attention;
  Upload upload;
  assert(upload.take()==Cue::None);upload.finish(false);assert(upload.take()==Cue::None);
  upload.begin();assert(upload.take()==Cue::Start);assert(upload.take()==Cue::None);
  upload.finish(true);assert(upload.take()==Cue::Success);
  upload.finish(false);assert(upload.take()==Cue::None); // Late disconnect cannot replace completion.
  upload.begin();upload.finish(false);assert(upload.take()==Cue::Failure); // Coalesce unplayed start.
  upload.finish(false);assert(upload.take()==Cue::None);
  upload.begin();upload.finish(true);assert(upload.take()==Cue::Success);
  Remote remote;
  assert(remote.next(0,"idle")==Cue::None);
  assert(remote.next(1,"complete")==Cue::None); // Persisted old completion is not a new sound.
  assert(remote.next(2,"failed")==Cue::None);
  assert(remote.next(3,"manifest")==Cue::None);
  assert(remote.next(3,"ready")==Cue::None); // Download alone is not applying firmware.
  assert(remote.next(3,"prepare")==Cue::Start);
  assert(remote.next(3,"prepare")==Cue::None);
  assert(remote.next(3,"program_esp")==Cue::None);
  assert(remote.next(3,"complete")==Cue::Success);
  assert(remote.next(3,"complete")==Cue::None);
  assert(remote.next(4,"manifest")==Cue::None);
  assert(remote.next(4,"failed")==Cue::Failure);
  assert(remote.next(4,"failed")==Cue::None);
  Remote afterReboot;
  assert(afterReboot.next(5,"verify_pair")==Cue::None); // No repeated start after reboot.
  assert(afterReboot.next(5,"complete")==Cue::Success);
  puts("Finite update notices: one-shot/coalesced/terminal priority, old completion suppression, remote apply/reboot/error and no replay passed.");
}
