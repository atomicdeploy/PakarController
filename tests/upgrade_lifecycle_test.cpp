#include "../firmware/esp8266/src/UpgradeLifecycle.h"
#include "../firmware/esp8266/src/NtfyNotice.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using PakarUpdate::Lifecycle;
using PakarUpdate::Source;
using PakarUpdate::Visual;

static uint32_t after(uint32_t start, uint32_t elapsed) { return start + elapsed; }

int main() {
  {
    Lifecycle l;PakarUpdate::NtfyNotice notices;PakarUpdate::NtfyNotice::Notice n;
    assert(!notices.observe(l,1) && !notices.take(n,1));
    assert(l.observe(Source::Local,1,"receiving",0,100,true,false,10));
    assert(notices.observe(l,10) && !notices.observe(l,11));
    assert(l.observe(Source::Local,1,"receiving",80,100,true,false,20));
    assert(!notices.observe(l,20)); // No notification per chunk/percentage.
    assert(notices.observe(l,30010));
    assert(l.observe(Source::Local,1,"ready-to-reboot",100,100,true,false,30011));
    assert(notices.observe(l,30011) && notices.take(n,30011));
    assert(!n.verified && !strcmp(n.phase,"ready-to-reboot"));
    assert(!notices.take(n,30012)); // Latest only, not a replay spool.
    assert(l.observe(Source::Local,1,"complete",0,0,false,false,30013));
    assert(!notices.observe(l,30013)); // An unverified complete is never success.
    assert(l.observe(Source::Local,1,"complete",0,0,false,true,30014));
    assert(notices.observe(l,30014) && notices.take(n,30014) && n.verified);
    assert(!notices.observe(l,30015));
    assert(l.observe(Source::Remote,2,"download_esp",0,100,true,false,40000));
    assert(notices.observe(l,40000));
    assert(l.observe(Source::Remote,2,"failed",0,0,false,false,40001));
    assert(notices.observe(l,40001) && notices.take(n,40001) && !strcmp(n.phase,"failed"));
    notices.clear();assert(!notices.pending());
    assert(notices.observe(l,UINT32_MAX-10));
    assert(!notices.take(n,300000U)); // TTL and uptime rollover.
  }
  // A normal boot fades the restored scene, but is not an update operation or
  // a synthesized completion/event. Startup expires without another observe.
  {
    Lifecycle life;
    life.boot(1000);
    assert(life.visual(1000) == Visual::Startup && life.sceneAlpha(1000) == 0);
    assert(life.visual(1899) == Visual::Startup && life.sceneAlpha(1899) == 254);
    assert(life.visual(1900) == Visual::None);
    bool phase = false;
    assert(!life.takeEvent(2000, phase) && !life.verified());
    assert(!life.takeShutdownNotice(2000) && life.takeTerminalNotice() == 0);
  }

  // Actual measured stages only. A finished transfer is still active and can
  // never become a verified update merely because its byte count reaches100%.
  {
    Lifecycle life;
    assert(!life.observe(Source::Local, 0, "receiving", 0, 100, true, false, 1000));
    assert(!life.observe(Source::Local, 1, nullptr, 0, 100, true, false, 1000));
    assert(!life.observe(Source::Local, 1, "abcdefghijklmnopqrstuvwxyz123456", 0, 100, true, false, 1000));
    assert(life.observe(Source::Local, 1, "receiving", 25, 100, true, false, 1000));
    assert(life.known() && life.percent() == 25 && life.done() == 25 && life.total() == 100);
    assert(life.visual(1000) == Visual::Progress && life.startedAt() == 1000);
    assert(life.observe(Source::Local, 1, "receiving", 100, 100, true, true, 1100));
    assert(life.percent() == 100 && !life.verified() && life.takeTerminalNotice() == 0);
    assert(life.visual(1100) == Visual::Progress);
    for (const char *stage : {"verifying", "verify_pair", "prepare", "program_isp", "awaiting_application"}) {
      assert(life.observe(Source::Local, 1, stage, 100, 100, true, false, 1200));
      assert(!life.known() && life.percent() == 0);
    }
    for (const char *stage : {"download_esp", "download_avr", "backup_flash", "backup_eeprom",
                             "verify_flash", "verify_eeprom", "verify_esp", "program_avr", "program_esp"}) {
      assert(life.observe(Source::Local, 1, stage, 50, 100, true, false, 1300));
      assert(life.known() && life.percent() == 50);
      assert(life.observe(Source::Local, 1, stage, 50, 0, true, false, 1301));
      assert(!life.known());
      assert(life.observe(Source::Local, 1, stage, 101, 100, true, false, 1302));
      assert(!life.known());
    }
    assert(life.observe(Source::Local, 1, "receiving", UINT32_MAX, UINT32_MAX, true, false, 1400));
    assert(life.percent() == 100); // Denominator arithmetic must not overflow32bits.
  }

  // Immediate phase changes, coalesced250ms byte progress, and no repeated
  // event for unchanged observations. Revision counts actual publications.
  {
    Lifecycle life;
    bool phase = false;
    assert(life.observe(Source::Remote, 7, "download_esp", 0, 1000, true, false, 1000));
    assert(life.takeEvent(1000, phase) && phase && life.revision() == 1);
    for (uint32_t now = 1001; now < 1100; ++now) {
      assert(!life.observe(Source::Remote, 7, "download_esp", 0, 1000, true, false, now));
      assert(!life.takeEvent(now, phase));
    }
    assert(life.observe(Source::Remote, 7, "download_esp", 10, 1000, true, false, 1100));
    assert(!life.takeEvent(1249, phase));
    assert(life.observe(Source::Remote, 7, "download_esp", 20, 1000, true, false, 1249));
    assert(life.takeEvent(1250, phase) && !phase && life.revision() == 2 && life.done() == 20);
    assert(life.updatedAt() == 1249 && life.startedAt() == 1000);
    assert(!life.takeEvent(1251, phase));
    assert(life.observe(Source::Remote, 7, "verify_pair", 0, 0, true, false, 1251));
    assert(life.takeEvent(1251, phase) && phase && life.revision() == 3);
    assert(!life.takeEvent(1501, phase));
  }

  // Historical terminal jobs are a baseline, not a fresh success/error cue.
  // Only an explicit current postboot receipt may admit a terminal observation.
  {
    Lifecycle baseline;
    for (const char *stage : {"complete", "failed", "aborted", "cancelled"}) {
      assert(!baseline.observe(Source::Remote, 8, stage, 100, 100, false, true, 2000));
      assert(baseline.takeTerminalNotice() == 0 && !baseline.verified());
    }
    bool phase = false;
    assert(!baseline.takeEvent(2000, phase));
    assert(baseline.observe(Source::Local, 8, "complete", 0, 0, false, true, 2000, true));
    assert(baseline.verified() && baseline.visual(2000) == Visual::Success);
    assert(baseline.takeTerminalNotice() == 1 && baseline.takeTerminalNotice() == 0);
    assert(!baseline.observe(Source::Local, 8, "complete", 0, 0, false, true, 2001, true));
    assert(baseline.takeTerminalNotice() == 0);
  }

  // Unverified completion never emits a success notice. Actual verification
  // admits exactly one notice, even if its terminal status is repeated.
  {
    Lifecycle life;
    assert(life.observe(Source::Avr, 2, "program_avr", 32768, 32768, true, false, 1000));
    assert(life.observe(Source::Avr, 2, "complete", 32768, 32768, false, false, 1100));
    assert(!life.verified() && life.takeTerminalNotice() == 0 && life.visual(1100) != Visual::Success);
    assert(life.observe(Source::Avr, 2, "complete", 32768, 32768, false, true, 1200));
    assert(life.verified() && life.takeTerminalNotice() == 1 && life.takeTerminalNotice() == 0);
    assert(life.visual(2699) == Visual::Success && life.visual(2700) == Visual::None);
    assert(life.observe(Source::Avr, 3, "prepare", 0, 0, true, false, 3000));
    assert(life.takeTerminalNotice() == 0); // New operation resets the old result.
    assert(life.observe(Source::Avr, 3, "failed", 0, 0, false, false, 3100));
    assert(!life.verified() && life.visual(3100) == Visual::Failure);
    assert(life.takeTerminalNotice() == -1 && life.takeTerminalNotice() == 0);
    assert(life.observe(Source::Avr, 4, "prepare", 0, 0, true, false, 3200));
    assert(life.observe(Source::Avr, 4, "aborted", 0, 0, false, false, 3300));
    assert(life.takeTerminalNotice() == -1 && life.takeTerminalNotice() == 0);
    assert(life.observe(Source::Avr, 5, "prepare", 0, 0, true, false, 3400));
    assert(life.observe(Source::Avr, 5, "cancelled", 0, 0, false, false, 3500));
    assert(!life.verified() && life.takeTerminalNotice() == 0);
  }

  // Prepare must hold its final black state until the actual owner transitions
  // phase. At700ms a frame can now carry exact zero, not jump back to blue.
  {
    Lifecycle life;
    assert(life.observe(Source::Remote, 10, "prepare", 0, 0, true, false, 1000));
    assert(life.visual(1000) == Visual::Shutdown && life.sceneAlpha(1000) == 255);
    assert(life.takeShutdownNotice(1000) && !life.takeShutdownNotice(1001));
    assert(life.sceneAlpha(1699) == 1);
    assert(life.visual(1700) == Visual::Shutdown && life.sceneAlpha(1700) == 0);
    assert(life.visual(6000) == Visual::Shutdown && life.sceneAlpha(6000) == 0);
    assert(!life.takeShutdownNotice(6000));
    assert(life.observe(Source::Remote, 10, "program_avr", 0, 32768, true, false, 6001));
    assert(life.visual(6001) == Visual::Progress);
    assert(life.observe(Source::Remote, 10, "awaiting_reboot", 0, 0, true, false, 7000));
    assert(!life.takeShutdownNotice(7000)); // One finite shutdown cue per operation.
    assert(life.observe(Source::Remote, 11, "prepare", 0, 0, true, false, 9000));
    assert(life.takeShutdownNotice(9000));
    assert(life.observe(Source::Avr, 11, "prepare", 0, 0, true, false, 9100));
    assert(life.takeShutdownNotice(9100)); // Source is part of operation identity.
  }

  // Reboot waits for a complete fade AND actual wire presentation; a failed
  // output worker cannot prevent a committed firmware from rebooting forever.
  for (const char *stage : {"awaiting_reboot", "ready-to-reboot"}) {
    Lifecycle life;
    assert(life.observe(Source::Local, 12, stage, 0, 0, true, false, 1000));
    assert(!life.readyToReboot(1699, true) && !life.readyToReboot(1699, false));
    assert(life.sceneAlpha(1700) == 0 && life.readyToReboot(1700, true));
    assert(!life.readyToReboot(1700, false) && !life.readyToReboot(2999, false));
    assert(life.readyToReboot(3000, false));
    assert(!life.verified() && life.takeTerminalNotice() == 0);
  }

  // Every finite timing comparison, including event coalescing and the hard
  // reboot deadline, uses elapsed unsigned subtraction across uptime rollover.
  {
    constexpr uint32_t start = UINT32_MAX - 400;
    Lifecycle startup;
    startup.boot(start);
    assert(startup.visual(after(start, 899)) == Visual::Startup);
    assert(startup.sceneAlpha(after(start, 899)) == 254);
    assert(startup.visual(after(start, 900)) == Visual::None);
    Lifecycle life;
    assert(life.observe(Source::Local, 13, "ready-to-reboot", 0, 0, true, false, start));
    assert(life.sceneAlpha(after(start, 699)) == 1);
    assert(!life.readyToReboot(after(start, 699), true));
    assert(life.sceneAlpha(after(start, 700)) == 0 && life.readyToReboot(after(start, 700), true));
    assert(!life.readyToReboot(after(start, 1999), false));
    assert(life.readyToReboot(after(start, 2000), false));
    Lifecycle events;
    bool phase = false;
    assert(events.observe(Source::Remote, 14, "download_esp", 0, 100, true, false, start));
    assert(events.takeEvent(start, phase) && phase);
    assert(events.observe(Source::Remote, 14, "download_esp", 1, 100, true, false, after(start, 100)));
    assert(!events.takeEvent(after(start, 249), phase));
    assert(events.takeEvent(after(start, 250), phase) && !phase && events.revision() == 2);
    assert(events.observe(Source::Remote, 14, "complete", 0, 0, false, true, after(start, 300)));
    assert(events.visual(after(start, 1899)) == Visual::Success);
    assert(events.visual(after(start, 1900)) == Visual::None);
    assert(events.takeTerminalNotice() == 1 && events.takeTerminalNotice() == 0);
  }
  puts("Upgrade lifecycle: measured/unknown stage truth, coalesced distinct events, verified-only once notices, exact shutdown black/prepare hold, bounded reboot gate and uptime rollover passed. Pure host logic, not hardware/sound/presentation proof.");
}
