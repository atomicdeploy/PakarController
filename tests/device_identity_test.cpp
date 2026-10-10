#include "DeviceIdentity.h"
#include <assert.h>
#include <stdio.h>
int main() {
  char name[64];
  assert(BoardCore::Identity::normalizeName("LEDBOARD.Local", name)); assert(strcmp(name, "ledboard") == 0);
  for (const char *bad : {"", "-x", "x-", "a.b", "http://ledboard.local", "x.local.local", "x_y", "x local", "local", "localhost", "é"})
    assert(!BoardCore::Identity::normalizeName(bad, name));
  String longest(63, 'a'); assert(BoardCore::Identity::normalizeName(longest.c_str(), name));
  assert(!BoardCore::Identity::normalizeName(String(64, 'a').c_str(), name));
  fs::FS filesystem; DeviceIdentity config;
  assert(config.begin(filesystem)); assert(config.ready());
  assert(filesystem.exists("/identity.bin")); assert(strcmp(config.hostname(), PAKAR_DEFAULT_HOSTNAME) == 0);
  filesystem.files["/network.bin"] = std::make_shared<File::Data>();
  filesystem.files["/network.bin"]->bytes = {1, 2, 3, 4};
  const auto wifiBefore = filesystem.files["/network.bin"]->bytes;
  filesystem.failWrite = true; assert(!config.rename("ledboard"));
  assert(strcmp(config.hostname(), PAKAR_DEFAULT_HOSTNAME) == 0);
  filesystem.failWrite = false; filesystem.failRename = true; assert(!config.rename("ledboard"));
  DeviceIdentity previous; assert(previous.begin(filesystem)); assert(strcmp(previous.hostname(), PAKAR_DEFAULT_HOSTNAME) == 0);
  filesystem.failRename = false;
  assert(config.rename("LEDBOARD.local")); assert(strcmp(config.hostname(), "ledboard") == 0);
  assert(filesystem.files["/network.bin"]->bytes == wifiBefore);
  DeviceIdentity reboot; assert(reboot.begin(filesystem)); assert(strcmp(reboot.hostname(), "ledboard") == 0);
  auto persisted = filesystem.files["/identity.bin"];
  assert(reboot.rename("ledboard.local")); assert(filesystem.files["/identity.bin"] == persisted); // no wear for a no-op
  assert(!reboot.rename("bad/name")); assert(strcmp(reboot.hostname(), "ledboard") == 0);
  assert(!reboot.rename(String("led\0bad", 7)));
  filesystem.files["/identity.bin"]->bytes[12] ^= 0x80;
  DeviceIdentity corrupt; assert(!corrupt.begin(filesystem)); assert(!corrupt.ready());
  assert(corrupt.rename("recovered")); DeviceIdentity recovered; assert(recovered.begin(filesystem));
  assert(strcmp(recovered.hostname(), "recovered") == 0);
  DeviceIdentity unavailable; assert(!unavailable.begin(filesystem, false)); assert(!unavailable.rename("ledboard"));
  puts("Production identity: validation, persistent initial label, failed-commit preservation, unchanged Wi-Fi, reboot, no-op wear avoidance and damaged-storage recovery passed.");
}
