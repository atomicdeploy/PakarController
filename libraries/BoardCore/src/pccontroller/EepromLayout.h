#pragma once
// Pakar EEPROM ownership; deliberately not the donor's unrelated RF/reset/
// status-profile image. Settings and automation banks are disjoint from Prog.
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
static_assert(AutomationEnd <= 1008, "EEPROM overlaps durable Prog latch");
}
