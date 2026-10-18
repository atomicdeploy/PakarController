#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "CloudCommandGuard.h"
#include "CommandReceipts.h"

using namespace BoardCore::CloudCommand;
static constexpr const char *board = "esp32s3-907069eac800";
static constexpr const char *boot = "123456789abcdef0123456789abcdef0";
static constexpr uint32_t epoch = 1792250000U;
static Result call(Guard &g, const char *id, uint32_t now = epoch,
                   uint32_t expiry = epoch + 60, uint32_t ticks = 0,
                   const char *command = "cell") {
  return g.admit(command, board, boot, id, expiry, now, ticks);
}
int main() {
  {
    BoardCore::CommandReceipts cache;
    BoardCore::CommandReceipts::Entry *entry=nullptr;
    const char *id="0123456789abcdef0123456789abcdef";
    const std::string digest(64,'a'),other(64,'b');
    using Receipts=BoardCore::CommandReceipts;
    assert(cache.reserve(id,digest.c_str(),5000,100,entry)==Receipts::Fresh);
    entry->accepted=true;entry->count=80;
    assert(cache.reserve(id,digest.c_str(),5000,200,entry)==Receipts::Cached);
    assert(entry->accepted && entry->count==80);
    assert(cache.reserve(id,other.c_str(),5000,200,entry)==Receipts::Conflict);
    assert(cache.reserve(id,digest.c_str(),5000,5001,entry)==Receipts::Expired);
    assert(cache.reserve(id,digest.c_str(),0x100000064ULL,0xfffffff0ULL,entry)==Receipts::Fresh);
    assert(cache.reserve(id,digest.c_str(),5000,0x100000001ULL,entry)==Receipts::Expired);
    assert(cache.reserve("bad",digest.c_str(),5000,0,entry)==Receipts::Invalid);
    Receipts full;
    char key[33];
    for(size_t i=0;i<Receipts::kCapacity;++i) {
      snprintf(key,sizeof(key),"%032x",static_cast<unsigned>(i));
      assert(full.reserve(key,digest.c_str(),5000,100,entry)==Receipts::Fresh);
    }
    assert(full.reserve("ffffffffffffffffffffffffffffffff",digest.c_str(),5000,100,entry)==Receipts::Full);
    assert(full.reserve("00000000000000000000000000000000",digest.c_str(),5000,200,entry)==Receipts::Cached);
  }
  Guard g;
  for (const auto *name : {"status", "telemetry", "firmware-status"})
    assert(g.admit(name, nullptr, nullptr, nullptr, 0, 0, UINT32_MAX) == Result::Allowed);
  assert(call(g, "one") == Result::InvalidIdentity);
  assert(!g.setIdentity(board, "00000000000000000000000000000000"));
  assert(!g.setIdentity("bad\nname", boot));
  assert(g.setIdentity(board, boot));
  assert(g.admit("cell", nullptr, boot, "one", epoch + 5, epoch, 0) == Result::MissingEnvelope);
  assert(g.admit("cell", board, nullptr, "one", epoch + 5, epoch, 0) == Result::MissingEnvelope);
  assert(g.admit("cell", board, boot, nullptr, epoch + 5, epoch, 0) == Result::MissingEnvelope);
  assert(call(g, "one", epoch, 0) == Result::MissingEnvelope);
  assert(g.admit("cell", "other", boot, "one", epoch + 5, epoch, 0) == Result::WrongPeer);
  assert(g.admit("cell", board, "223456789abcdef0123456789abcdef0", "one", epoch + 5, epoch, 0) == Result::WrongPeer);
  assert(g.admit("cell", board, "123456789ABCDEF0123456789abcdef0", "one", epoch + 5, epoch, 0) == Result::WrongPeer);
  assert(call(g, "") == Result::InvalidRequestId);
  for (const auto *id : {"has space", "line\nbreak", "topic/#", "dot.id", "\x80"})
    assert(call(g, id) == Result::InvalidRequestId);
  assert(call(g, std::string(41, 'a').c_str()) == Result::InvalidRequestId);
  assert(call(g, "one", 0) == Result::ClockUnavailable);
  assert(call(g, "one", epoch, epoch) == Result::Expired);
  assert(call(g, "one", epoch, epoch - 1) == Result::Expired);
  assert(call(g, "one", epoch, epoch + 61) == Result::DeadlineTooFar);
  assert(call(g, "one") == Result::Allowed);
  // The same ID is consumed before handler success: unknown outcome, a changed
  // payload/command, MQTT->WS delivery, or a future deadline cannot replay it.
  assert(call(g, "one", epoch, epoch + 1, 1, "outputs") == Result::Duplicate);
  assert(g.setIdentity(board, boot));
  assert(call(g, "one") == Result::Duplicate);
  assert(!g.setIdentity(nullptr, nullptr));
  assert(call(g, "one") == Result::InvalidIdentity);
  assert(g.setIdentity(board, boot));
  assert(call(g, "one") == Result::Duplicate);
  // Unknown/empty/null commands never acquire the read-only exception.
  for (const auto *name : {"", "Status", "status ", "unknown"})
    assert(g.admit(name, nullptr, nullptr, nullptr, 0, epoch, 0) == Result::MissingEnvelope);
  assert(g.admit(nullptr, nullptr, nullptr, nullptr, 0, epoch, 0) == Result::MissingEnvelope);
  assert(call(g, std::string(40, 'a').c_str()) == Result::Allowed);

  Guard capacity;
  assert(capacity.setIdentity(board, boot));
  for (size_t i = 0; i < Guard::kCapacity; ++i) {
    char id[41]; snprintf(id, sizeof(id), "request-%u", static_cast<unsigned>(i));
    assert(call(capacity, id) == Result::Allowed);
  }
  assert(call(capacity, "extra") == Result::Capacity);
  assert(call(capacity, "request-0") == Result::Duplicate); // No eviction.
  assert(call(capacity, "extra", epoch + 60, epoch + 61) == Result::Allowed);
  // Reuse after old deadline expiry is safe: the old envelope itself is expired.
  assert(call(capacity, "request-0", epoch + 60, epoch + 60) == Result::Expired);
  assert(call(capacity, "request-0", epoch + 60, epoch + 61) == Result::Allowed);

  Guard clock;
  assert(clock.setIdentity(board, boot));
  assert(call(clock, "old", epoch, epoch + 5, UINT32_MAX - 1000) == Result::Allowed);
  assert(call(clock, "across-wrap", epoch + 2, epoch + 10, 999) == Result::Allowed);
  assert(call(clock, "old", epoch + 2, epoch + 10, 999) == Result::Duplicate);
  assert(call(clock, "new", epoch + 1, epoch + 4, 1100) == Result::ClockRegressed);
  assert(clock.admit("status", nullptr, nullptr, nullptr, 0, epoch + 20, 2000) == Result::Allowed);
  assert(call(clock, "new", epoch + 19, epoch + 21, 2100) == Result::ClockRegressed);
  assert(call(clock, "new", epoch + 20, epoch + 21, 2200) == Result::Allowed);
  assert(call(clock, "old", epoch + 20, epoch + 5, 2300) == Result::Expired);
  // Retained/queued delivery from the prior boot is rejected even with a fresh ID.
  assert(clock.setIdentity(board, "223456789abcdef0123456789abcdef0"));
  assert(call(clock, "retained", epoch + 20, epoch + 21) == Result::WrongPeer);
  Guard ceiling;
  assert(ceiling.setIdentity(board, boot));
  assert(call(ceiling, "ceiling", UINT32_MAX - 2, UINT32_MAX) == Result::Allowed);
  assert(call(ceiling, "wrapped-expiry", UINT32_MAX - 1, 1) == Result::Expired);
  assert(!strcmp(code(Result::Capacity), "capacity"));
  assert(!strcmp(code(Result::ClockRegressed), "clock_regressed"));
  printf("Cloud admission: %u reservations, %u bytes; replay/deadline/identity/clock/wrap checks passed.\n",
      static_cast<unsigned>(Guard::kCapacity), static_cast<unsigned>(sizeof(Guard)));
}
