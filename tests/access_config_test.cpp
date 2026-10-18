#include "AccessConfig.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
int main() {
  fs::FS filesystem; AccessConfig config;
  assert(config.begin(filesystem)); assert(config.defaultsActive());
  assert(!config.enabled());
  assert(config.configure("", false));
  AccessConfig disabled; assert(disabled.begin(filesystem) && !disabled.enabled());
  assert(!config.configure("", true)); // Enabling requires explicit credentials.
  assert(config.configure("", true, "admin", "pakarcontroller"));
  assert(!config.change("wrong", "David", "new-password"));
  assert(!config.change("pakarcontroller", "bad:user", "new-password"));
  filesystem.failWrite = true;
  assert(!config.change("pakarcontroller", "David", "new-password"));
  assert(config.verify("admin", "pakarcontroller"));
  filesystem.failWrite = false; filesystem.failRename = true;
  assert(!config.change("pakarcontroller", "David", "new-password"));
  assert(config.verify("admin", "pakarcontroller"));
  filesystem.failRename = false;
  assert(config.change("pakarcontroller", "David", "new-password"));
  assert(!config.defaultsActive() && config.verify("David", "new-password"));
  AccessConfig reboot; assert(reboot.begin(filesystem)); assert(reboot.verify("David", "new-password"));
  assert(!reboot.verify("admin", "pakarcontroller"));
  assert(!reboot.configure("wrong", false));
  filesystem.failWrite=true;assert(!reboot.configure("new-password", false));assert(reboot.enabled());
  filesystem.failWrite=false;assert(reboot.configure("new-password", false));assert(!reboot.enabled());
  AccessConfig offReboot;assert(offReboot.begin(filesystem) && !offReboot.enabled());
  filesystem.files["/access.bin"]->bytes[20] ^= 0x80;
  AccessConfig damaged; assert(!damaged.begin(filesystem)); assert(!damaged.ready());
  assert(!damaged.verify("admin", "pakarcontroller"));
  assert(damaged.provisionFromPhysicalUsb("recovered", "new-secret"));
  AccessConfig recovered; assert(recovered.begin(filesystem)); assert(recovered.verify("recovered", "new-secret"));
  AccessConfig unmounted; assert(!unmounted.begin(filesystem, false));
  assert(!unmounted.ready() && !unmounted.provisionFromPhysicalUsb("admin", "new-secret"));
  struct Legacy {uint32_t magic;uint8_t schema;char username[33],password[65];uint32_t checksum;} __attribute__((packed));
  for (bool custom : {false,true}) {
    fs::FS legacyFs;Legacy legacy{0x504b4155UL,1,"admin","pakarcontroller",0};
    if(custom){strcpy(legacy.username,"David");strcpy(legacy.password,"private-password");}
    uint32_t sum=2166136261UL;auto *bytes=reinterpret_cast<uint8_t *>(&legacy);
    for(size_t i=0;i<offsetof(Legacy,checksum);++i)sum=(sum^bytes[i])*16777619UL;
    legacy.checksum=sum;File file=legacyFs.open("/access.bin","w");file.write(bytes,sizeof(legacy));file.close();
    AccessConfig migrated;assert(migrated.begin(legacyFs));assert(migrated.enabled()==custom);
    AccessConfig reread;assert(reread.begin(legacyFs));assert(reread.enabled()==custom);
    assert(reread.verify(legacy.username,legacy.password));
  }
  puts("Production access persistence, wrong-password rejection, failed commit preservation, restart, corruption and USB recovery passed (filesystem test double).");
}
