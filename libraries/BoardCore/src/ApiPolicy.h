#pragma once
#include <string.h>

namespace BoardCore {
namespace ApiPolicy {
// Adapted from PCController ipcjson/ipc.go's top-level 404 guard.
// Pakar rejects every numbered root namespace, not only the old v1 spelling.
inline bool numberedRoute(const char *path) {
  if (!path || strncmp(path, "/api/v", 6)) return false;
  const char *p = path + 6;
  if (*p < '0' || *p > '9') return false;
  do { ++p; } while (*p >= '0' && *p <= '9');
  return !*p || *p == '/' || *p == '?';
}
} // namespace ApiPolicy
} // namespace BoardCore
