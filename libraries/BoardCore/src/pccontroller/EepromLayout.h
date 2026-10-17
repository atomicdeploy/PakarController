#pragma once
// Pakar EEPROM ownership; not the donor's unrelated RF/status-profile image.
// Settings, automation and reset-health journal are disjoint from Prog.
namespace EepromLayout {
constexpr int SettingsBankA = 32;
constexpr int SettingsBankBytes = 48;
constexpr int SettingsBankB = SettingsBankA + SettingsBankBytes;
constexpr int AutomationBankAAddress = SettingsBankB + SettingsBankBytes;
constexpr unsigned char AutomationHeaderBytes = 10;
constexpr unsigned char AutomationCapacity = 11;
constexpr unsigned char AutomationRecordBytes = 12;
constexpr int AutomationBankBytes = AutomationHeaderBytes +
                                   AutomationCapacity * AutomationRecordBytes;
constexpr int AutomationBankBAddress = AutomationBankAAddress + AutomationBankBytes;
constexpr int AutomationEnd = AutomationBankBAddress + AutomationBankBytes;
constexpr int ResetJournalAddress = 416;
constexpr unsigned char ResetJournalSlots = 64;
constexpr unsigned char ResetRecordBytes = 6;
constexpr int ResetJournalEnd = ResetJournalAddress + ResetJournalSlots * ResetRecordBytes;
static_assert(AutomationEnd <= ResetJournalAddress, "Automation overlaps reset journal");
static_assert(ResetJournalEnd <= 1008, "Reset journal overlaps durable Prog latch");
}
