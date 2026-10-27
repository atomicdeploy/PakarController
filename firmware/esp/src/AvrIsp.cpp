#include "AvrIsp.h"

#include <memory>
#include <new>

namespace {
constexpr uint32_t kSpiClock = 125000;
constexpr uint16_t kPageBytes = 128;
}

bool AvrIsp::fail(const __FlashStringHelper *message) {
  error_ = message;
  leave();
  return false;
}

uint8_t AvrIsp::transfer4(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                          uint8_t *third) {
  spi_.transfer(a);
  spi_.transfer(b);
  const uint8_t thirdValue = spi_.transfer(c);
  if (third != nullptr) *third = thirdValue;
  return spi_.transfer(d);
}

bool AvrIsp::enter() {
  error_ = String();
#if defined(ARDUINO_ARCH_ESP32)
  if (!PAKAR_S3_ISP_ELECTRICAL_VERIFIED) {
    error_ = F("ISP disabled: confirm voltage-safe circuit and reset polarity before enabling build flag");
    return false;
  }
  digitalWrite(resetPin_, !PAKAR_S3_ISP_RESET_ASSERT_LEVEL);
  pinMode(resetPin_, OUTPUT);
  if (!spi_.begin(PAKAR_S3_ISP_SCK, PAKAR_S3_ISP_MISO, PAKAR_S3_ISP_MOSI, -1)) {
    pinMode(resetPin_, INPUT);
    error_ = F("Cannot start dedicated AVR ISP SPI bus");
    return false;
  }
  busStarted_ = true;
  spi_.beginTransaction(SPISettings(kSpiClock, MSBFIRST, SPI_MODE0));
  digitalWrite(PAKAR_S3_ISP_SCK, LOW);
  delay(5);
  digitalWrite(resetPin_, PAKAR_S3_ISP_RESET_ASSERT_LEVEL);
#else
  pinMode(resetPin_, OUTPUT_OPEN_DRAIN);
  digitalWrite(resetPin_, HIGH);
  spi_.begin();
  busStarted_ = true;
  spi_.beginTransaction(SPISettings(kSpiClock, MSBFIRST, SPI_MODE0));
  digitalWrite(SCK, LOW);
  delay(5);
  digitalWrite(resetPin_, LOW);
#endif
  active_ = true;
  delay(30);
  uint8_t echo = 0;
  transfer4(0xAC, 0x53, 0x00, 0x00, &echo);
  if (echo != 0x53) {
    leave();
    error_ = F("AVR did not enter serial programming mode");
    return false;
  }
  active_ = true;
  return true;
}

void AvrIsp::leave() {
  if (!busStarted_) return;
  if (active_) spi_.endTransaction();
  spi_.end();
#if defined(ARDUINO_ARCH_ESP32)
  pinMode(PAKAR_S3_ISP_MOSI, INPUT);
  pinMode(PAKAR_S3_ISP_SCK, INPUT);
  digitalWrite(resetPin_, !PAKAR_S3_ISP_RESET_ASSERT_LEVEL);
#else
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  digitalWrite(resetPin_, HIGH);
#endif
  pinMode(resetPin_, INPUT);
  active_ = false;
  busStarted_ = false;
  delay(10);
}

bool AvrIsp::readSignature(uint8_t signature[3]) {
  for (uint8_t i = 0; i < 3; ++i) {
    signature[i] = transfer4(0x30, 0x00, i, 0x00);
  }
  return signature[0] == 0x1E && signature[1] == 0x95 && signature[2] == 0x0F;
}

bool AvrIsp::validSignature() {
  uint8_t signature[3];
  if (!readSignature(signature)) return fail(F("Target signature is not ATmega328P"));
  return true;
}

bool AvrIsp::probe(uint8_t signature[3]) {
  if (!enter()) return false;
  const bool valid = readSignature(signature);
  if (!valid) error_ = F("Target signature is not ATmega328P");
  leave();
  return valid;
}

uint8_t AvrIsp::readFlashByte(uint32_t byteAddress) {
  const uint16_t word = static_cast<uint16_t>(byteAddress >> 1);
  return transfer4((byteAddress & 1U) == 0 ? 0x20 : 0x28,
                   static_cast<uint8_t>(word >> 8),
                   static_cast<uint8_t>(word), 0x00);
}

uint8_t AvrIsp::readEepromByte(uint16_t address) {
  return transfer4(0xA0, static_cast<uint8_t>(address >> 8),
                   static_cast<uint8_t>(address), 0x00);
}

uint8_t AvrIsp::readFuse(uint8_t a, uint8_t b) {
  return transfer4(a, b, 0x00, 0x00);
}

bool AvrIsp::backup(fs::FS &filesystem, const char *directory) {
  return captureBackup(filesystem,directory,false);
}

bool AvrIsp::captureBackup(fs::FS &filesystem, const char *directory, bool holdReset) {
  if (!enter()) return false;
  if (!validSignature()) return false;
  if ((readFuse(0x58, 0x00) & 0x3F) != 0x3F)
    return fail(F("Locked AVR cannot provide a verifiable full recovery backup"));
  filesystem.mkdir(directory);

  const String flashPath = String(directory) + F("/flash.pending");
  File flash = filesystem.open(flashPath, "w");
  if (!flash) return fail(F("Cannot create AVR flash backup"));
  uint8_t block[128];
  for (uint32_t base = 0; base < kFlashBytes; base += sizeof(block)) {
    for (uint16_t i = 0; i < sizeof(block); ++i) block[i] = readFlashByte(base + i);
    if (flash.write(block, sizeof(block)) != sizeof(block)) {
      flash.close();
      return fail(F("AVR flash backup write failed"));
    }
    report("backup_flash",base+sizeof(block),kFlashBytes);
  }
  flash.flush(); flash.close();
  flash = filesystem.open(flashPath, "r");
  if (!flash || flash.size()!=kFlashBytes) return fail(F("Flash backup could not be reopened"));
  for (uint32_t i=0;i<kFlashBytes;++i) {
    if (flash.read()!=readFlashByte(i)) {flash.close();return fail(F("Stored flash backup mismatch"));}
    if ((i & 127U)==127U) report("verify_flash",i+1,kFlashBytes);
  }
  flash.close();

  const String eepromPath = String(directory) + F("/eeprom.pending");
  File eeprom = filesystem.open(eepromPath, "w");
  if (!eeprom) return fail(F("Cannot create AVR EEPROM backup"));
  for (uint16_t base = 0; base < kEepromBytes; base += sizeof(block)) {
    const uint16_t count = min<uint16_t>(sizeof(block), kEepromBytes - base);
    for (uint16_t i = 0; i < count; ++i) block[i] = readEepromByte(base + i);
    if (eeprom.write(block, count) != count) {
      eeprom.close();
      return fail(F("AVR EEPROM backup write failed"));
    }
    report("backup_eeprom",base+count,kEepromBytes);
  }
  eeprom.flush(); eeprom.close();
  eeprom = filesystem.open(eepromPath, "r");
  if (!eeprom || eeprom.size()!=kEepromBytes) return fail(F("EEPROM backup could not be reopened"));
  for (uint16_t i=0;i<kEepromBytes;++i) {
    if (eeprom.read()!=readEepromByte(i)) {eeprom.close();return fail(F("Stored EEPROM backup mismatch"));}
    if ((i & 127U)==127U) report("verify_eeprom",i+1,kEepromBytes);
  }
  eeprom.close();

  const uint8_t lfuse = readFuse(0x50, 0x00);
  const uint8_t hfuse = readFuse(0x58, 0x08);
  const uint8_t efuse = readFuse(0x50, 0x08);
  const uint8_t lock = readFuse(0x58, 0x00);
  const String metadataPath=String(directory)+F("/metadata.pending");
  File metadata = filesystem.open(metadataPath, "w");
  if (!metadata) return fail(F("Cannot create AVR backup metadata"));
  char expectedMetadata[96];
  const int expectedSize=snprintf(expectedMetadata,sizeof(expectedMetadata),
      "signature=1E950F\nlfuse=%02X\nhfuse=%02X\nefuse=%02X\nlock=%02X\n",lfuse,hfuse,efuse,lock);
  if(expectedSize<=0 || static_cast<size_t>(expectedSize)>=sizeof(expectedMetadata))
    return fail(F("Recovery metadata formatting failed"));
  const size_t written=metadata.write(reinterpret_cast<const uint8_t *>(expectedMetadata),expectedSize);
  metadata.flush();metadata.close();
  metadata=filesystem.open(metadataPath,"r");
  if(written!=static_cast<size_t>(expectedSize) || !metadata || metadata.size()!=written)
    return fail(F("Recovery metadata write failed"));
  for(size_t i=0;i<written;++i)
    if(metadata.read()!=static_cast<uint8_t>(expectedMetadata[i]))
      return fail(F("Recovery fuse metadata readback mismatch; no erase"));
  metadata.close();
  if(!filesystem.rename(flashPath,String(directory)+F("/flash.bin")) ||
     !filesystem.rename(eepromPath,String(directory)+F("/eeprom.bin")) ||
     !filesystem.rename(metadataPath,String(directory)+F("/metadata.txt")))
    return fail(F("Recovery backup commit failed; no erase performed"));
  if(!holdReset)leave();
  return true;
}

bool AvrIsp::recoverFlash(fs::FS &filesystem, const char *freshDirectory,
                          File &image, File &pinnedFlash) {
  if(!image || image.size()!=kFlashBytes || !pinnedFlash || pinnedFlash.size()!=kFlashBytes) {
    error_=F("Complete candidate and pinned original flash required; no reset or erase");
    return false;
  }
  // Do not turn a failed/interrupted capture into an overwrite on retry.
  if(!freshDirectory || !*freshDirectory || filesystem.exists(freshDirectory)) {
    error_=F("Fresh recovery directory already exists or is invalid; no erase");
    return false;
  }
  if(!captureBackup(filesystem,freshDirectory,true))return false;
  // captureBackup flushed, reopened and compared all flash/EEPROM bytes and
  // committed fuse metadata. RESET stays asserted across file open and every
  // preflight. programFlash retains the exact same comparison/erase guards.
  File eeprom=filesystem.open(String(freshDirectory)+F("/eeprom.bin"),"r");
  File armed=filesystem.open(String(freshDirectory)+F("/eeprom-armed.bin"),"w+");
  const bool ok=programFlash(image,eeprom,&pinnedFlash,&armed);
  eeprom.close();armed.close();
  leave(); // Also releases on early image/file validation failures.
  return ok;
}

bool AvrIsp::programFlash(File &image, File &recoveryEeprom,
                          File *expectedFlash, File *armedEeprom) {
  if ((expectedFlash == nullptr) != (armedEeprom == nullptr)) {
    error_ = F("ISP recovery requires both original flash and armed EEPROM files");
    return false;
  }
  if (armedEeprom == &recoveryEeprom || armedEeprom == &image ||
      (armedEeprom != nullptr && armedEeprom == expectedFlash)) {
    error_ = F("Armed EEPROM must be a distinct recovery file");
    return false;
  }
  if (!image || image.size() != kFlashBytes) {
    error_ = F("AVR image must be exactly 32768 raw bytes");
    return false;
  }
  // recoverFlash is the sole held-session owner; never pulse RESET between its
  // durable snapshot and validation. Ordinary callers still enter normally.
  if (!active_ && !enter()) return false;
  if (!validSignature()) return false;
  if ((readFuse(0x58, 0x00) & 0x3F) != 0x3F)
    return fail(F("Locked target: refusing erase without readable recovery state"));
  // AVR programming runs from the HTTP/maintenance owner, whose call chain is
  // considerably deeper than the read-only backup route. Keeping the complete
  // EEPROM recovery image on that task's stack can overflow it before the
  // guarded erase begins. The buffer is still transaction-local and bounded,
  // but lives in checked heap storage instead.
  std::unique_ptr<uint8_t[]> savedEeprom(new (std::nothrow) uint8_t[kEepromBytes]);
  if (!savedEeprom) return fail(F("Insufficient memory for EEPROM recovery image"));
  if (!recoveryEeprom || recoveryEeprom.size()!=kEepromBytes || !recoveryEeprom.seek(0,SeekSet) ||
      recoveryEeprom.read(savedEeprom.get(),kEepromBytes)!=kEepromBytes)
    return fail(F("A complete durable EEPROM recovery snapshot is required"));
  for (uint16_t i=0;i<kEepromBytes;++i) {
    if (readEepromByte(i)!=savedEeprom[i]) return fail(F("EEPROM changed after recovery snapshot; erase refused"));
    if ((i & 127U)==127U) report("verify_eeprom",i+1,kEepromBytes);
  }
  // Prove the entire source remains readable before the irreversible erase.
  if (!image.seek(0,SeekSet)) return fail(F("Cannot seek flash image"));
  uint8_t preflight[kPageBytes];
  for(uint32_t i=0;i<kFlashBytes;i+=sizeof(preflight))
    if(image.read(preflight,sizeof(preflight))!=sizeof(preflight))
      return fail(F("Flash image unreadable before erase"));

  if (expectedFlash != nullptr) {
    if (!*expectedFlash || expectedFlash->size()!=kFlashBytes ||
        !expectedFlash->seek(0,SeekSet))
      return fail(F("ISP recovery requires the complete original flash snapshot"));
    for (uint32_t base=0;base<kFlashBytes;base+=sizeof(preflight)) {
      if (expectedFlash->read(preflight,sizeof(preflight))!=sizeof(preflight))
        return fail(F("Original flash snapshot unreadable; erase refused"));
      for (uint16_t i=0;i<sizeof(preflight);++i)
        if (readFlashByte(base+i)!=preflight[i])
          return fail(F("Flash changed after recovery snapshot; erase refused"));
      cooperate();
    }
    // MiniCore 328P uses full-swing external crystal LFUSE F7; FF selects
    // the low-power external crystal driver. Neither enables CKDIV8. The
    // existing 16 MHz board also requires the reset
    // vector in the application (BOOTRST=1) and software-controlled WDTON=1.
    // A different fuse/boot contract requires explicit recovery, never a write.
    const uint8_t lfuse=readFuse(0x50,0x00);
    if ((lfuse!=0xF7 && lfuse!=0xFF) || (readFuse(0x58,0x08)&0x11U)!=0x11U)
      return fail(F("ISP recovery fuse/clock contract mismatch; erase refused"));
    // Exact existing firmware/avr/ProgrammingLatch.cpp record at1008..1012:
    // PK, private EEPROM layout1, active flag1, XOR checksum. Do not mutate the
    // original recoveryEeprom file; preserve every other byte in the armed copy.
    constexpr uint16_t kProgAddress=1008;
    const uint8_t prog[]={0x50,0x4B,1,1,static_cast<uint8_t>(0x50^0x4B^1^1^0xA5)};
    for(uint8_t i=0;i<sizeof(prog);++i)savedEeprom[kProgAddress+i]=prog[i];
    if (!*armedEeprom || !armedEeprom->seek(0,SeekSet) ||
        armedEeprom->write(savedEeprom.get(),kEepromBytes)!=kEepromBytes)
      return fail(F("Cannot store armed EEPROM recovery snapshot; erase refused"));
    armedEeprom->flush();
    if(armedEeprom->size()!=kEepromBytes || !armedEeprom->seek(0,SeekSet))
      return fail(F("Cannot verify armed EEPROM recovery snapshot; erase refused"));
    for(uint16_t base=0;base<kEepromBytes;base+=sizeof(preflight)) {
      if(armedEeprom->read(preflight,sizeof(preflight))!=sizeof(preflight))
        return fail(F("Armed EEPROM recovery snapshot unreadable; erase refused"));
      for(uint16_t i=0;i<sizeof(preflight);++i)
        if(preflight[i]!=savedEeprom[base+i])
          return fail(F("Armed EEPROM recovery snapshot mismatch; erase refused"));
      cooperate();
    }
  }

  transfer4(0xAC, 0x80, 0x00, 0x00);
  delay(15);
  // EESAVE is not assumed. Restore first, while reset remains asserted, so
  // a later flash verification failure cannot silently erase settings/Prog.
  // ATmega328P datasheet 27.8.3: C0 byte write, F0 RDY/BSY polling.
  for (uint16_t i=0;i<kEepromBytes;++i) {
    if(readEepromByte(i)!=savedEeprom[i]) {
      transfer4(0xC0,static_cast<uint8_t>(i>>8),static_cast<uint8_t>(i),savedEeprom[i]);
      uint8_t attempts=0;
      while((transfer4(0xF0,0,0,0)&1U) && attempts++<20) delay(1);
      if(readEepromByte(i)!=savedEeprom[i]) return fail(F("EEPROM restore mismatch; retained recovery file required"));
    }
    if((i&127U)==127U)report("verify_eeprom",i+1,kEepromBytes);else cooperate();
  }
  image.seek(0, SeekSet);
  uint8_t page[kPageBytes];
  for (uint32_t base = 0; base < kFlashBytes; base += kPageBytes) {
    if (image.read(page, sizeof(page)) != sizeof(page)) {
      return fail(F("AVR image ended during page load"));
    }
    for (uint16_t offset = 0; offset < kPageBytes; offset += 2) {
      const uint16_t word = static_cast<uint16_t>((base + offset) >> 1);
      transfer4(0x40, static_cast<uint8_t>(word >> 8),
                static_cast<uint8_t>(word), page[offset]);
      transfer4(0x48, static_cast<uint8_t>(word >> 8),
                static_cast<uint8_t>(word), page[offset + 1]);
    }
    const uint16_t pageWord = static_cast<uint16_t>(base >> 1);
    transfer4(0x4C, static_cast<uint8_t>(pageWord >> 8),
              static_cast<uint8_t>(pageWord), 0x00);
    delay(6);
    report("program_avr",base+kPageBytes,kFlashBytes);
  }

  image.seek(0, SeekSet);
  for (uint32_t base = 0; base < kFlashBytes; base += sizeof(page)) {
    if (image.read(page, sizeof(page)) != sizeof(page)) {
      return fail(F("AVR image ended during verification"));
    }
    for (uint16_t i = 0; i < sizeof(page); ++i) {
      if (readFlashByte(base + i) != page[i]) {
        return fail(F("AVR flash verification mismatch"));
      }
    }
    report("verify_flash",base+sizeof(page),kFlashBytes);
  }
  for(uint16_t i=0;i<kEepromBytes;++i) {
    if(readEepromByte(i)!=savedEeprom[i]) return fail(F("EEPROM final verification mismatch"));
    if((i&127U)==127U)report("verify_eeprom",i+1,kEepromBytes);
  }
  leave();
  return true;
}
