#pragma once
// The actual NetworkConfig declaration needs a filesystem TYPE, not storage.
// No flash write implementation is provided: accidental persistence must fail
// linkage rather than silently pass this route-only test.
namespace fs { class FS {}; }
