#include "LedScene.h"
#include "AvrSceneSync.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <vector>

struct MemoryFiles {
  using Bytes = std::vector<uint8_t>;
  std::map<std::string, std::shared_ptr<Bytes>> files;
  bool failOpen = false, failWrite = false, failClose = false, failRename = false, failRead = false;
  unsigned opensForWrite = 0;
  struct File {
    MemoryFiles *owner; std::shared_ptr<Bytes> bytes; bool writing;
    explicit operator bool() const { return bool(bytes); }
    size_t size() const { return bytes ? bytes->size() : 0; }
    size_t read(uint8_t *out, size_t count) {
      if (!bytes || owner->failRead || count > bytes->size()) return 0;
      memcpy(out, bytes->data(), count); return count;
    }
    size_t write(const uint8_t *in, size_t count) {
      if (!bytes) return 0;
      size_t n = owner->failWrite ? count/2 : count;
      bytes->assign(in, in+n); return n;
    }
    void close() {
      if (writing && owner->failClose && bytes && !bytes->empty()) bytes->back() ^= 1;
      bytes.reset();
    }
  };
  bool exists(const char *path) const { return files.count(path); }
  File open(const char *path, const char *mode) {
    bool writing = mode[0] == 'w';
    if (failOpen) return {this, nullptr, writing};
    if (writing) { ++opensForWrite; files[path] = std::make_shared<Bytes>(); }
    return {this, exists(path) ? files[path] : nullptr, writing};
  }
  bool rename(const char *from, const char *to) {
    if (failRename || !exists(from)) return false;
    files[to] = files[from]; files.erase(from); return true;
  }
};
using Store = SceneStore<MemoryFiles>;
int main() {
  AvrSceneSync sync;
  auto baseline = sync.observe(1, 2);
  assert(baseline.baseline && baseline.sendMode);
  baseline = sync.observe(1, 2); assert(baseline.baseline && !baseline.sendMode);
  baseline = sync.observe(2, 2); assert(baseline.baseline && !baseline.sendMode);
  baseline = sync.observe(2, 2); assert(!baseline.baseline && !baseline.sendMode);
  sync.hello(); baseline = sync.observe(2, 2);
  assert(baseline.baseline && !baseline.sendMode);
  baseline = sync.observe(1, 2); assert(!baseline.baseline); // Later real menu action.
  SceneState defaults, desired;
  desired.cells[0] = 1; desired.cells[79] = 2;
  desired.mode = 2; desired.brightness = 144; desired.flags = 2;
  desired.selectedCell = 80; desired.selectedState = 2; desired.winner = 4;
  MemoryFiles fs; Store store;
  assert(!store.begin(fs, true, defaults) && !store.pending() && !store.saved());
  MemoryFiles factoryFs; Store factory; factory.begin(factoryFs, true, defaults);
  assert(factory.service(1, true, true) && factory.saved()); // Explicit save of the unchanged factory scene.
  store.observe(desired, 100); assert(store.pending());
  assert(!store.service(1599) && !fs.opensForWrite);
  assert(!store.service(1600, false) && !fs.opensForWrite); // Flash owner deferral.
  assert(store.service(1600) && store.saved() && fs.opensForWrite == 1);
  const auto original = *fs.files[Store::path()];
  Store reboot; assert(reboot.begin(fs, true, defaults) && reboot.restored() && reboot.saved());
  assert(!memcmp(&reboot.state(), &desired, sizeof(desired)));
  for (uint32_t now = 0; now < 60000; ++now) {
    reboot.observe(desired, now); assert(!reboot.service(now));
  }
  assert(fs.opensForWrite == 1); // No per-frame/telemetry/disconnect writes.
  auto next = desired; next.cells[3] = 1;
  reboot.observe(next, 100); reboot.observe(desired, 200); // Undo before debounce.
  assert(!reboot.pending() && !reboot.service(2000) && fs.opensForWrite == 1);
  for (unsigned fault = 0; fault < 5; ++fault) {
    MemoryFiles failed; failed.files[Store::path()] = std::make_shared<MemoryFiles::Bytes>(original);
    Store edits; assert(edits.begin(failed, true, defaults)); edits.observe(next, 100);
    failed.failOpen = fault == 0; failed.failWrite = fault == 1;
    failed.failClose = fault == 2; failed.failRead = fault == 3; failed.failRename = fault == 4;
    assert(!edits.service(1600) && edits.pending() && edits.error());
    assert(*failed.files[Store::path()] == original);
    const auto attempts = failed.opensForWrite;
    assert(!edits.service(1601, true, true) && failed.opensForWrite == attempts);
    failed.failOpen = failed.failWrite = failed.failClose = failed.failRead = failed.failRename = false;
    Store afterFailure; assert(afterFailure.begin(failed, true, defaults));
    assert(!memcmp(&afterFailure.state(), &desired, sizeof(desired))); // Pending never promoted.
    assert(edits.service(11600) && edits.saved());
    Store recovered; assert(recovered.begin(failed, true, defaults));
    assert(!memcmp(&recovered.state(), &next, sizeof(next)));
  }
  for (size_t i = 0; i < original.size(); ++i) {
    MemoryFiles damaged; auto bytes = original; bytes[i] ^= 1;
    damaged.files[Store::path()] = std::make_shared<MemoryFiles::Bytes>(bytes);
    Store corrupt; assert(!corrupt.begin(damaged, true, defaults) && corrupt.error());
    assert(!corrupt.restored() && !corrupt.saved() && !corrupt.pending());
    assert(!memcmp(&corrupt.state(), &defaults, sizeof(defaults)));
    assert(!damaged.opensForWrite); // Do not silently overwrite damage.
  }
  MemoryFiles continuous; Store coalesced; coalesced.begin(continuous, true, defaults);
  for (uint32_t at = 100; at <= 10100; at += 100) {
    auto changing = desired; changing.brightness = uint8_t(1 + at/100%250);
    coalesced.observe(changing, at); coalesced.service(at);
  }
  assert(continuous.opensForWrite == 1); // Continuous edits have bounded save delay.
  MemoryFiles wrapFs; Store wrap; wrap.begin(wrapFs, true, defaults);
  wrap.observe(desired, UINT32_MAX-500); assert(!wrap.service(998)); assert(wrap.service(999));
  Store unavailable; assert(!unavailable.begin(fs, false, defaults));
  unavailable.observe(next, 1); assert(!unavailable.service(1501) && unavailable.pending());
  SceneState bad = desired; bad.cells[2] = 3; reboot.observe(bad, 70000);
  assert(!reboot.pending()); // Invalid optional record cannot become saved authority.

  // Actual renderer: restore all logical states/mapping before first frame.
  LedGrid grid; const auto shows = FastLED.shows; grid.begin(false);
  assert(FastLED.shows == shows && grid.pixelFrameJson() == "null");
  uint8_t mapping[80];
  for (uint8_t id = 1; id <= 80; ++id) mapping[id-1] = BoardCore::TableCellMapping::physicalForLogical(id);
  grid.setPhysicalMapping(mapping, 80);
  desired.flags = 3; restoreLedScene(grid, desired);
  assert(FastLED.shows == shows); grid.service(++pixelTestNow);
  const auto frame = grid.pixelFrameJson(); const auto start = frame.find("\"rgb\":\"")+7;
  for (unsigned p = 0; p < 480; ++p) {
    const auto logical = BoardCore::TableCellMapping::logicalForPhysical(p/6+1);
    assert(frame.substr(start+p*6, 6) == (logical == 1 ? "ff0000" : logical == 80 ? "000000" : "ffffff"));
  }
  auto scene = captureLedScene(desired, grid, 2, 80, 2, 4);
  assert(!memcmp(&scene, &desired, sizeof(scene)));
  grid.setPattern(LedGrid::kPatternRainbow);
  scene = captureLedScene(scene, grid, 2, 80, 2, 4);
  MemoryFiles animations; Store animationStore; animationStore.begin(animations, true, defaults);
  animationStore.observe(scene, 0); assert(animationStore.service(1500));
  for (unsigned i = 0; i < 2000; ++i) {
    grid.service(++pixelTestNow);
    const auto tick = captureLedScene(scene, grid, 2, 80, 2, 4);
    animationStore.observe(tick, 2000+i); animationStore.service(2000+i);
  }
  assert(animations.opensForWrite == 1);
  grid.setPattern(LedGrid::kPatternCellChase);
  const auto chase = captureLedScene(scene, grid, 1, 80, 2, 0);
  for (unsigned i = 0; i < 80; ++i) {
    grid.service(pixelTestNow += 751);
    const auto tick = captureLedScene(chase, grid, 1, 80, 2, 0);
    assert(!memcmp(&tick, &chase, sizeof(tick))); // Chase frames don't churn saved cells.
  }
  auto snake = captureLedScene(scene, grid, 3, 80, 2, 0);
  grid.fill(BoardCore::Protocol::kCellOff);
  auto snakeTick = captureLedScene(snake, grid, 3, 80, 2, 0);
  assert(!memcmp(snake.cells, snakeTick.cells, 80)); // Game phase not stored.
  grid.setRedLock(false);grid.setCell(1,BoardCore::Protocol::kCellRed,BoardCore::Protocol::kEffectImmediate);
  grid.setRedLock(true);auto locked=captureLedScene(scene,grid,1,1,1,0);
  assert(locked.flags&4);MemoryFiles locks;Store lockStore;lockStore.begin(locks,true,defaults);
  lockStore.observe(locked,0);assert(lockStore.service(1500));Store lockReboot;assert(lockReboot.begin(locks,true,defaults));
  LedGrid restoredLock;restoredLock.begin(false);restoreLedScene(restoredLock,lockReboot.state());
  assert(restoredLock.redLock()&&restoredLock.state(1)==BoardCore::Protocol::kCellRed);
  assert(!restoredLock.setCell(1,BoardCore::Protocol::kCellWhite,BoardCore::Protocol::kEffectImmediate));
  assert(restoredLock.setCell(1,BoardCore::Protocol::kCellOff,BoardCore::Protocol::kEffectImmediate));
  restoredLock.setRedLock(false);assert(restoredLock.resetWhite());
  puts("Scene persistence: exact80-cell reboot/mode/output/red-lock restore, first-frame mapping, debounce/max-delay, no-op/frame wear, rollover and transactional failure/corruption recovery passed. Host fixtures, not power-loss/optical proof.");
}
