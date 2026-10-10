#pragma once

#include <stdint.h>

namespace BoardCore {
namespace BuildInfo {

constexpr char kVersion[] = "0.2.1";
constexpr uint8_t kVersionMajor = 0;
constexpr uint8_t kVersionMinor = 2;
constexpr uint8_t kVersionPatch = 1;
constexpr uint8_t kProtocolVersion = 1;

}  // namespace BuildInfo
}  // namespace BoardCore
