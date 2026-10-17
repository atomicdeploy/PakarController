#include "StatusRgbConfig.h"
#include <cassert>
#include <cstdio>

using Store = PakarS3::StatusRgbConfig;
int main() {
  fs::FS filesystem; Store settings;
  assert(settings.begin(filesystem));
  assert(!settings.persisted() && !settings.configuration().enabled &&
      !settings.configuration().pinConfirmed && settings.configuration().pin == -1);
  assert(filesystem.files.empty()); // Boot defaults never wear flash.
  auto candidate = settings.configuration();
  candidate.pin = 48; candidate.enabled = true;
  assert(!settings.save(candidate)); // No guessed onboard pin.
  assert(filesystem.files.empty());
  candidate.pinConfirmed = true; candidate.pin = 4;
  assert(!settings.save(candidate)); // Matrix cannot become status RGB.
  candidate.pin = 17; assert(!settings.save(candidate)); // UART cannot either.
  candidate.pin = 48; candidate.brightness = 200;
  assert(settings.save(candidate) && settings.persisted());
  const auto original = filesystem.files.at("/status-rgb.bin");
  const auto bytes = original->bytes;
  assert(settings.save(candidate));
  assert(filesystem.files.at("/status-rgb.bin") == original); // No identical write.
  Store reboot; assert(reboot.begin(filesystem));
  assert(reboot.configuration().enabled && reboot.configuration().pin == 48 &&
      reboot.configuration().brightness == 200 && reboot.configuration().pinConfirmed);
  assert(reboot.save(candidate) && filesystem.files.at("/status-rgb.bin") == original);
  candidate.pin = 38; candidate.brightness = 32;
  filesystem.failWrite = true;
  assert(!settings.save(candidate));
  assert(settings.configuration().pin == 48 && settings.configuration().brightness == 200);
  assert(filesystem.files.at("/status-rgb.bin")->bytes == bytes);
  filesystem.failWrite = false; filesystem.failRename = true;
  assert(!settings.save(candidate));
  assert(settings.configuration().pin == 48 && filesystem.files.at("/status-rgb.bin") == original);
  Store afterFailedRename; assert(afterFailedRename.begin(filesystem));
  assert(afterFailedRename.configuration().pin == 48); // Never recover uncommitted .pending.
  filesystem.failRename = false;
  assert(settings.save(candidate) && settings.configuration().pin == 38);
  candidate.enabled = false;
  assert(settings.save(candidate));
  Store disabled; assert(disabled.begin(filesystem) && !disabled.configuration().enabled);
  filesystem.files.at("/status-rgb.bin")->bytes[6] ^= 0x01;
  Store corrupt; assert(!corrupt.begin(filesystem));
  assert(!corrupt.configuration().enabled && corrupt.configuration().pin == -1);
  // Exact record size matters, not just the checksum-covered prefix.
  filesystem.files.at("/status-rgb.bin")->bytes = bytes;
  filesystem.files.at("/status-rgb.bin")->bytes.push_back(0);
  Store oversized; assert(!oversized.begin(filesystem));
  assert(!oversized.configuration().enabled);
  // Explicit reviewed build defaults are permitted; arbitrary ones fail safe.
  fs::FS fresh; Store explicitDefault;
  candidate.pin = 38; candidate.enabled = true; candidate.pinConfirmed = true;
  assert(explicitDefault.begin(fresh,candidate) && explicitDefault.configuration().enabled);
  assert(fresh.files.empty());
  candidate.pin = 4; Store badDefault;
  assert(!badDefault.begin(fresh,candidate));
  assert(!badDefault.configuration().enabled && badDefault.configuration().pin == -1);
  Store unmounted; assert(!unmounted.save(Store::Configuration{}));
  puts("S3 RGB config: explicit pin whitelist, safe defaults, checksum/exact layout, commit/readback, no identical writes, write/rename rollback and reboot recovery pass; not physical LED proof.");
}
