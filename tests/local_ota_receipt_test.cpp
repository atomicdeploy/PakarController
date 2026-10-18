#include "../firmware/esp8266/src/LocalOtaReceipt.h"
#include "FS.h"
#include <cassert>
#include <cstdio>

uint32_t testNow = 0;

// Reuse the actual remote updater filesystem and ArduinoJson fixtures; this
// thin adapter adds short read/open/close faults without changing shared stubs.
struct ReceiptFiles {
  fs::FS backing;
  bool failOpen = false, failRead = false, failClose = false;
  unsigned opensForWrite = 0;
  struct Handle {
    ReceiptFiles *owner; File file; String path; bool writing;
    explicit operator bool() const { return bool(file); }
    size_t size() const { return file.size(); }
    size_t read(uint8_t *out, size_t n) { return owner->failRead ? 0 : file.read(out, n); }
    size_t write(const uint8_t *bytes, size_t n) { return file.write(bytes, n); }
    void close() {
      file.close();
      if (writing && owner->failClose) {
        auto &bytes = owner->backing.files.at(path)->bytes;
        if (!bytes.empty()) bytes.back() ^= 1;
      }
    }
  };
  bool exists(const char *path) const { return backing.files.count(path) != 0; }
  Handle open(const char *path, const char *mode) {
    const bool writing = mode[0] == 'w';
    if (failOpen) return {this, File{}, path, writing};
    if (writing) ++opensForWrite;
    return {this, backing.open(path, mode), path, writing};
  }
  bool rename(const char *from, const char *to) { return backing.rename(from, to); }
};
using Receipt = LocalOtaReceipt<ReceiptFiles>;
constexpr const char *digest = "0123456789abcdef0123456789abcdef";
constexpr const char *otherDigest = "abcdef0123456789abcdef0123456789";

int main() {
  ReceiptFiles fs;
  Receipt upload;
  assert(!upload.begin(fs, true, "source-boot", 8192, otherDigest, true));
  assert(!upload.known() && upload.uploadId() == 0 && !fs.opensForWrite);
  assert(upload.awaitingReboot("source-boot", 41, 4096, digest));
  assert(upload.pending() && !upload.installed() && !upload.completionVerified());
  assert(upload.uploadId() == 41 && upload.sourceBoot() == "source-boot");
  assert(upload.takeNotice() == Receipt::Notice::None && fs.opensForWrite == 1);
  const auto awaiting = fs.backing.files.at(Receipt::path())->bytes;
  assert(upload.awaitingReboot("source-boot", 41, 4096, digest));
  assert(fs.opensForWrite == 1); // Duplicate callback does not rewrite/rearm.
  assert(!upload.awaitingReboot("other-boot", 42, 4096, digest));

  Receipt sameBoot;
  assert(sameBoot.begin(fs, true, "source-boot", 4096, digest, true));
  assert(sameBoot.pending() && !sameBoot.installed() && !sameBoot.completionVerified());
  assert(fs.opensForWrite == 1); // A matching image BEFORE reboot is not completion.
  Receipt after;
  assert(after.begin(fs, true, "new-boot", 4096, digest, true));
  assert(!after.pending() && after.installed() && after.completionVerified());
  assert(after.installedBoot() == "new-boot" && fs.opensForWrite == 2);
  const auto completed = fs.backing.files.at(Receipt::path())->bytes;
  assert(after.takeNotice() == Receipt::Notice::Complete && after.noticeConsumed());
  assert(after.takeNotice() == Receipt::Notice::None && fs.opensForWrite == 3);
  for (unsigned i = 0; i < 100; ++i) {
    Receipt reboot;
    assert(reboot.begin(fs, true, "later-boot", 4096, digest, true));
    assert(reboot.completionVerified() && reboot.takeNotice() == Receipt::Notice::None);
    JsonDocument observed; assert(!deserializeJson(observed, reboot.json()));
    assert(observed["completion_verified"] == true && observed["automatic_retry"] == false);
  }
  assert(fs.opensForWrite == 3); // No per-frame/boot/telemetry writes after terminal consumption.

  // A later different firmware does not turn this historical receipt into a
  // fresh success, authorize a flash, or repeat its already consumed cue.
  Receipt changedImage;
  assert(changedImage.begin(fs, true, "different-update", 4096, otherDigest, true));
  assert(!changedImage.installed() && !changedImage.completionVerified());
  assert(changedImage.takeNotice() == Receipt::Notice::None && fs.opensForWrite == 3);

  // Failures are durable terminal observations, not automatic recovery writes.
  for (unsigned failure = 0; failure < 5; ++failure) {
    ReceiptFiles failed;
    failed.backing.files[Receipt::path()] = std::make_shared<File::Data>();
    failed.backing.files[Receipt::path()]->bytes = awaiting;
    Receipt bad;
    assert(bad.begin(failed, true, "new-boot", failure == 1 ? 4097 : 4096,
        failure == 2 ? "" : failure == 3 ? otherDigest : digest, failure != 0));
    // Case4 explicitly tests valid postboot, handled separately below.
    if (failure == 4) { assert(bad.completionVerified()); continue; }
    assert(!strcmp(bad.phase(), "failed") && !bad.completionVerified());
    assert(bad.takeNotice() == Receipt::Notice::Failed);
    Receipt again; assert(again.begin(failed, true, "third-boot", 4096, digest, true));
    assert(!strcmp(again.phase(), "failed") && again.takeNotice() == Receipt::Notice::None);
    assert(failed.opensForWrite == 2);
  }
  ReceiptFiles noDigestFs; Receipt noDigest;
  assert(!noDigest.begin(noDigestFs, true, "a", 4096, digest, true));
  assert(noDigest.awaitingReboot("a", 1, 4096));
  Receipt noDigestBoot; assert(noDigestBoot.begin(noDigestFs, true, "b", 4096, digest, true));
  assert(!strcmp(noDigestBoot.phase(), "failed") &&
      !strcmp(noDigestBoot.error(), "expected_digest_unavailable"));

  // Atomic pending/readback/rename failures preserve committed ownership.
  for (unsigned fault = 0; fault < 5; ++fault) {
    ReceiptFiles failed;
    failed.backing.files[Receipt::path()] = std::make_shared<File::Data>();
    failed.backing.files[Receipt::path()]->bytes = awaiting;
    failed.failOpen = fault == 0;
    failed.backing.failWrite = fault == 1;
    failed.failClose = fault == 2;
    failed.failRead = fault == 3;
    failed.backing.failRename = fault == 4;
    Receipt terminal;
    assert(!terminal.begin(failed, true, "new-boot", 4096, digest, true));
    assert(!terminal.completionVerified() && terminal.takeNotice() == Receipt::Notice::None);
    assert(failed.backing.files.at(Receipt::path())->bytes == awaiting);
    failed.failOpen = failed.failRead = failed.failClose = false;
    failed.backing.failWrite = failed.backing.failRename = false;
    Receipt recovered;
    assert(recovered.begin(failed, true, "another-boot", 4096, digest, true));
    assert(recovered.completionVerified()); // Only the committed pending record is used.
  }
  for (unsigned fault = 0; fault < 3; ++fault) {
    ReceiptFiles failed;
    failed.backing.files[Receipt::path()] = std::make_shared<File::Data>();
    failed.backing.files[Receipt::path()]->bytes = completed;
    Receipt notice; assert(notice.begin(failed, true, "new-boot", 4096, digest, true));
    failed.backing.failWrite = fault == 0; failed.failClose = fault == 1;
    failed.backing.failRename = fault == 2;
    assert(notice.takeNotice() == Receipt::Notice::None && !notice.noticeConsumed());
    assert(failed.backing.files.at(Receipt::path())->bytes == completed);
  }

  // Single-byte corruption, truncated/oversized, noncanonical/trailing or an
  // orphan pending file never becomes installed authority or plays a cue.
  for (size_t at = 0; at < awaiting.size(); ++at) {
    ReceiptFiles corrupt;
    corrupt.backing.files[Receipt::path()] = std::make_shared<File::Data>();
    corrupt.backing.files[Receipt::path()]->bytes = awaiting;
    corrupt.backing.files[Receipt::path()]->bytes[at] ^= 1;
    Receipt damaged; assert(!damaged.begin(corrupt, true, "new-boot", 4096, digest, true));
    assert(!damaged.known() && damaged.takeNotice() == Receipt::Notice::None && !corrupt.opensForWrite);
  }
  for (unsigned malformed = 0; malformed < 4; ++malformed) {
    ReceiptFiles corrupt;
    corrupt.backing.files[Receipt::path()] = std::make_shared<File::Data>();
    auto &bytes = corrupt.backing.files[Receipt::path()]->bytes;
    bytes = awaiting;
    if (malformed == 0) bytes.resize(Receipt::maximumRecordBytes + 1, ' ');
    if (malformed == 1) bytes.resize(bytes.size() - 1);
    if (malformed == 2) bytes.push_back('x');
    if (malformed == 3) bytes.insert(bytes.begin(), ' ');
    Receipt damaged; assert(!damaged.begin(corrupt, true, "new-boot", 4096, digest, true));
    assert(!damaged.completionVerified() && !corrupt.opensForWrite);
  }
  ReceiptFiles orphan;
  orphan.backing.files[Receipt::pendingPath()] = std::make_shared<File::Data>();
  orphan.backing.files[Receipt::pendingPath()]->bytes = awaiting;
  Receipt abandoned; assert(!abandoned.begin(orphan, true, "new-boot", 4096, digest, true));
  assert(!abandoned.known() && !orphan.opensForWrite);
  Receipt unavailable; assert(!unavailable.begin(fs, false, "new-boot", 4096, digest, true));
  assert(!unavailable.awaitingReboot("new-boot", 99, 4096, digest));
  puts("Local OTA receipt: bound postboot digest/size/integrity verification, atomic fault/corruption recovery, at-most-once write-ahead notices and no frame/reboot write churn passed. Host fixtures, not physical flash/power-loss or sound proof.");
}
