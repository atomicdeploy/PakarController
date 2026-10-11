#include "AccessConfig.h"
#include <assert.h>
#include <stdio.h>
int main() {
  fs::FS filesystem; AccessConfig config;
  assert(config.begin(filesystem)); assert(config.defaultsActive());
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
  filesystem.files["/access.bin"]->bytes[20] ^= 0x80;
  AccessConfig damaged; assert(!damaged.begin(filesystem)); assert(!damaged.ready());
  assert(!damaged.verify("admin", "pakarcontroller"));
  assert(damaged.provisionFromPhysicalUsb("recovered", "new-secret"));
  AccessConfig recovered; assert(recovered.begin(filesystem)); assert(recovered.verify("recovered", "new-secret"));
  AccessConfig unmounted; assert(!unmounted.begin(filesystem, false));
  assert(!unmounted.ready() && !unmounted.provisionFromPhysicalUsb("admin", "new-secret"));
  puts("Production access persistence, wrong-password rejection, failed commit preservation, restart, corruption and USB recovery passed (filesystem test double).");
}
