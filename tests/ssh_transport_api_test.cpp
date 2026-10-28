#include "../firmware/esp32s3/src/SshTransport.h"
// Parse every actual production method against the pinned real libssh API.
// Stubs cover Arduino/FreeRTOS declarations ONLY; no crypto/wire claim.
static_assert(sizeof(PakarS3::SshTransport::Packet)<160);
