#pragma once
// Only an SDK double; tests do not claim entropy or authentication validation.
inline int os_get_random(unsigned char *, unsigned int) { return -1; }
