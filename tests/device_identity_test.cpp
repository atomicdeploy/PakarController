#include "DeviceIdentity.h"
#include "ConfigurationChanges.h"
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
  PakarConfiguration::changes().reset();PakarConfiguration::Journal::Event change;
  filesystem.files["/network.bin"] = std::make_shared<File::Data>();
  filesystem.files["/network.bin"]->bytes = {1, 2, 3, 4};
  const auto wifiBefore = filesystem.files["/network.bin"]->bytes;
  filesystem.failWrite = true; assert(!config.rename("ledboard"));
  assert(strcmp(config.hostname(), PAKAR_DEFAULT_HOSTNAME) == 0);
  filesystem.failWrite = false; filesystem.failRename = true; assert(!config.rename("ledboard"));
  assert(!PakarConfiguration::changes().peek(change));
  DeviceIdentity previous; assert(previous.begin(filesystem)); assert(strcmp(previous.hostname(), PAKAR_DEFAULT_HOSTNAME) == 0);
  filesystem.failRename = false;
  assert(config.rename("LEDBOARD.local")); assert(strcmp(config.hostname(), "ledboard") == 0);
  assert(PakarConfiguration::changes().peek(change)&&change.scopes==PakarConfiguration::Identity);
  assert(PakarConfiguration::changes().retire(change));
  assert(filesystem.files["/network.bin"]->bytes == wifiBefore);
  DeviceIdentity reboot; assert(reboot.begin(filesystem)); assert(strcmp(reboot.hostname(), "ledboard") == 0);
  assert(config.configure("LEDBOARD.local", "David’s table"));
  DeviceIdentity labelled; assert(labelled.begin(filesystem)); assert(strcmp(labelled.displayName(), "David’s table") == 0);
  assert(labelled.rename("ledboard")); assert(strcmp(labelled.displayName(), "David’s table") == 0);
  filesystem.failRename = true; assert(!labelled.configure("different", "Other board"));
  assert(strcmp(labelled.hostname(), "ledboard") == 0 && strcmp(labelled.displayName(), "David’s table") == 0);
  filesystem.failRename = false;
  for (const char *bad : {"", " leading", "trailing ", "x\n", "\x7f", "\xc2\x80", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"})
    assert(!BoardCore::Identity::validDisplayName(bad));
  assert(BoardCore::Identity::validDisplayName("Table \"A\" \\ studio"));
  assert(!labelled.configure("ledboard", String("a\0b", 3)));
  assert(!BoardCore::Identity::validDisplayName(String(64, 'a').c_str()));
  reboot = labelled;
  auto persisted = filesystem.files["/identity.bin"];
  assert(reboot.rename("ledboard.local")); assert(filesystem.files["/identity.bin"] == persisted); // no wear for a no-op
  assert(!reboot.rename("bad/name")); assert(strcmp(reboot.hostname(), "ledboard") == 0);
  assert(!reboot.rename(String("led\0bad", 7)));
  filesystem.files["/identity.bin"]->bytes[12] ^= 0x80;
  DeviceIdentity corrupt; assert(!corrupt.begin(filesystem)); assert(!corrupt.ready());
  assert(corrupt.rename("recovered")); DeviceIdentity recovered; assert(recovered.begin(filesystem));
  assert(strcmp(recovered.hostname(), "recovered") == 0);
  DeviceIdentity unavailable; assert(!unavailable.begin(filesystem, false)); assert(!unavailable.rename("ledboard"));
  // Schema-1 migration preserves a previously deployed hostname atomically.
  struct Legacy { uint32_t magic; uint8_t schema; char hostname[64]; uint32_t checksum; } __attribute__((packed));
  Legacy legacy{}; legacy.magic=0x504b4944UL; legacy.schema=1; strcpy(legacy.hostname,"old-board");
  uint32_t hash=2166136261UL; const uint8_t *bytes=reinterpret_cast<const uint8_t *>(&legacy);
  for(size_t i=0;i<offsetof(Legacy,checksum);++i)hash=(hash^bytes[i])*16777619UL;
  legacy.checksum=hash;
  fs::FS migration; migration.files["/identity.bin"]=std::make_shared<File::Data>();
  migration.files["/identity.bin"]->bytes.assign(bytes,bytes+sizeof(legacy));
  migration.failRename=true; DeviceIdentity failedMigration; assert(!failedMigration.begin(migration));
  assert(strcmp(failedMigration.hostname(),"old-board")==0 && strcmp(failedMigration.displayName(),"old-board")==0);
  assert(migration.files["/identity.bin"]->bytes.size()==sizeof(legacy));
  migration.failRename=false; DeviceIdentity migrated; assert(migrated.begin(migration));
  assert(strcmp(migrated.hostname(),"old-board")==0 && strcmp(migrated.displayName(),"old-board")==0);
  DeviceIdentity migratedReboot; assert(migratedReboot.begin(migration));
  puts("Production identity: validation, persistent initial label, failed-commit preservation, unchanged Wi-Fi, reboot, no-op wear avoidance and damaged-storage recovery passed.");
}
