#pragma once
#include <stddef.h>
#include <string.h>

namespace BoardCore { namespace PeerFence {
// Epochs are correlation, not authentication. Never substitute build/version
// or a human-editable hostname for the current boot's random identity.
inline bool bootValid(const char *boot) {
  if (!boot || strlen(boot) != 32) return false;
  bool nonzero = false;
  for (size_t i = 0; i < 32; ++i) {
    const char c = boot[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    nonzero = nonzero || c != '0';
  }
  return nonzero;
}
inline bool boardValid(const char *board) {
  if (!board) return false;
  const size_t length = strlen(board);
  if (!length || length > 64) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = board[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}
inline bool ready(const char *board, const char *boot) {
  return boardValid(board) && bootValid(boot);
}
inline bool accepts(const char *board, const char *boot,
                    const char *expectedBoard, const char *expectedBoot) {
  // Absent pair is legacy, explicitly unfenced compatibility. Partial/empty,
  // invalid or stale supplied pairs fail closed before any mutation.
  if (!expectedBoard && !expectedBoot) return true;
  return ready(board, boot) && ready(expectedBoard, expectedBoot) &&
      strcmp(board, expectedBoard) == 0 && strcmp(boot, expectedBoot) == 0;
}
} }
