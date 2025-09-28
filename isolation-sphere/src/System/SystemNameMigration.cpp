#include "system/SystemNameMigration.h"
#include <cstring>

namespace {
constexpr const char* kNewPrefix = "sphere-";     // 長さ7
constexpr const char* kOldPrefix = "joystick-";   // 長さ9
constexpr std::size_t kDigits = 3;
constexpr std::size_t kNewLen = 7 + kDigits;      // "sphere-" + 3桁 = 10
constexpr std::size_t kOldLen = 9 + kDigits;      // "joystick-" + 3桁 = 12
constexpr std::size_t kNewBufNeeded = kNewLen + 1;

bool isDigits(const char* p, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
  }
  return true;
}
}

bool isValidSystemName(const char* name) {
  if (!name) return false;
  if (std::strlen(name) != kNewLen) return false;
  if (std::strncmp(name, kNewPrefix, 7) != 0) return false;
  return isDigits(name + 7, kDigits);
}

bool migrateSystemName(const char* oldName, char* outBuf, std::size_t outSize) {
  if (!oldName || !outBuf) return false;

  // 既に有効
  if (isValidSystemName(oldName)) {
    if (outSize < kNewBufNeeded) return false;
    if (outBuf != oldName) std::memcpy(outBuf, oldName, kNewBufNeeded);
    return true;
  }

  std::size_t inLen = std::strlen(oldName);

  // joystick-### → sphere-###
  if (inLen == kOldLen &&
      std::strncmp(oldName, kOldPrefix, 9) == 0 &&
      isDigits(oldName + 9, kDigits)) {

    if (outSize < kNewBufNeeded) return false;
    char tmp[kNewBufNeeded];
    std::memcpy(tmp, kNewPrefix, 7);
    std::memcpy(tmp + 7, oldName + 9, kDigits);
    tmp[kNewLen] = '\0';
    std::memcpy(outBuf, tmp, kNewBufNeeded);
    return true;
  }

  return false;
}