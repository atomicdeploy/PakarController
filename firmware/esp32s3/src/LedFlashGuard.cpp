#include "LedFlashGuard.h"
#include <esp_partition.h>
#include <esp_image_format.h>
#include <esp_attr.h>
#include <esp_private/cache_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ESP-IDF 5.5.5 partition_target.c is the common task-context boundary used by
// LittleFS, NVS, esp_ota_ops and Arduino Update's ESP.partitionWrite/EraseRange.
// Reads also disable cache and suppress this SDK's non-cache-safe GDMA ISR.
// Image verification bypasses partition reads through bootloader_flash_read,
// so its complete task-level operation must also hold the pause.
// Encrypted writes also pass through esp_partition_write. Do NOT wrap the lower
// esp_flash APIs: SDK panic/core-dump writers call them without a scheduler.
// SDK files remain unchanged; the build wrapper supplies GNU --wrap flags.
extern "C" esp_err_t __real_esp_partition_write(const esp_partition_t *, size_t, const void *, size_t);
extern "C" esp_err_t __real_esp_partition_write_raw(const esp_partition_t *, size_t, const void *, size_t);
extern "C" esp_err_t __real_esp_partition_erase_range(const esp_partition_t *, size_t, size_t);
extern "C" esp_err_t __real_esp_partition_read(const esp_partition_t *, size_t, void *, size_t);
extern "C" esp_err_t __real_esp_partition_read_raw(const esp_partition_t *, size_t, void *, size_t);
extern "C" esp_err_t __real_esp_image_verify(esp_image_load_mode_t, const esp_partition_pos_t *, esp_image_metadata_t *);

namespace {
bool IRAM_ATTR canPause() {
  // All checks before the task-context hook are IRAM/ROM-safe in the reviewed
  // S3 SDK. Never block from an ISR, critical section, cache-off or suspended
  // scheduler context. Boot-time NOT_STARTED is allowed; the hook is then inert.
  return spi_flash_cache_enabled() && !xPortInIsrContext() && xPortCanYield() &&
      xTaskGetSchedulerState() != taskSCHEDULER_SUSPENDED;
}
}
bool IRAM_ATTR PakarS3::acquireTaskLedFlashPause() {
  return canPause() && acquireLedFlashPause();
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_partition_write(
    const esp_partition_t *partition, size_t offset, const void *source, size_t bytes) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_partition_write(partition,offset,source,bytes);
  PakarS3::releaseLedFlashPause();
  return result;
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_partition_write_raw(
    const esp_partition_t *partition, size_t offset, const void *source, size_t bytes) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_partition_write_raw(partition,offset,source,bytes);
  PakarS3::releaseLedFlashPause();
  return result;
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_partition_erase_range(
    const esp_partition_t *partition, size_t offset, size_t bytes) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_partition_erase_range(partition,offset,bytes);
  PakarS3::releaseLedFlashPause();
  return result;
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_partition_read(
    const esp_partition_t *partition, size_t offset, void *destination, size_t bytes) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_partition_read(partition,offset,destination,bytes);
  PakarS3::releaseLedFlashPause();
  return result;
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_partition_read_raw(
    const esp_partition_t *partition, size_t offset, void *destination, size_t bytes) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_partition_read_raw(partition,offset,destination,bytes);
  PakarS3::releaseLedFlashPause();
  return result;
}

extern "C" esp_err_t IRAM_ATTR __wrap_esp_image_verify(
    esp_image_load_mode_t mode, const esp_partition_pos_t *position, esp_image_metadata_t *metadata) {
  if (!PakarS3::acquireTaskLedFlashPause()) return ESP_ERR_TIMEOUT;
  const esp_err_t result=__real_esp_image_verify(mode,position,metadata);
  PakarS3::releaseLedFlashPause();
  return result;
}
