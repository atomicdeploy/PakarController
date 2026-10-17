#pragma once
// Only an SDK double; tests do not claim entropy or authentication validation.
inline int pakarTestEntropyResult = -1; // Preserve entropy failure by default.
inline unsigned char pakarTestEntropySeed = 1;
inline int os_get_random(unsigned char *out, unsigned int size) {
  if (pakarTestEntropyResult != 0) return pakarTestEntropyResult;
  for (unsigned int i = 0; i < size; ++i) out[i] = pakarTestEntropySeed + i;
  ++pakarTestEntropySeed;
  return 0; // Deterministic fixture only, never evidence for live entropy.
}
