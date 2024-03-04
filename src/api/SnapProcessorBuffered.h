#pragma once
#include "SnapOutput.h"

namespace snap_arduino {

/**
 * @brief Processor for which the encoded output is buffered in a ringbuffer in
 * order to prevent any buffer underruns.
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2024-03-04
 * @copyright Copyright (c) 2024
 */
class SnapProcessorBuffered : public SnapProcessor {
 public:
  /// Default constructor
  SnapProcessorBuffered(int buffer_size, int activationAtPercent = 75)
      : SnapProcessor() {
    buffer.resize(buffer_size);
    active_percent = activationAtPercent;
  }

  bool begin() override {
    // regular begin logic
    bool result = SnapProcessor::begin();
    // empty buffer
    buffer.reset();
    sizes.reset();
    is_active = false;
    return result;
  }

  /// fill buffer
  size_t writeAudio(const uint8_t *data, size_t size) override {
    if (size > buffer.size()) {
      ESP_LOGE(TAG, "The buffer is too small. Use a multiple of %d", size);
      stop();
    }
    sizes.write(size);
    size_t result = buffer.writeArray(data, size);
    ESP_LOGI(TAG, "size: %zu / buffer %d", size, buffer.available());

    if (result != size)
      ESP_LOGE(TAG, "Could not buffer all data %d->%d", size, result);

    return result;
  }

  /// Decode from buffer
  virtual void processExt() {
    if (isBufferActive()) {
      int16_t step_size = sizes.read();
      if (step_size > 0) {
        uint8_t tmp[step_size];
        int size_eff = buffer.readArray(tmp, step_size);
        int size_written = SnapProcessor::writeAudio(tmp, size_eff);
        if (size_written != size_eff) {
          ESP_LOGE(TAG, "Could not write all data %d->%d", size_eff,
                   size_written);
        }
      }
    }
    delay(1);
  }

 protected:
  const char *TAG = "SnapProcessorBuffered";
  RingBuffer<uint8_t> buffer{0};
  RingBuffer<int16_t> sizes{RTOS_MAX_QUEUE_ENTRY_COUNT};
  bool is_active = false;
  int active_percent;

  bool isBufferActive() {
    if (!is_active) {
      if (buffer.available() >= bufferTaskActivationLimit()) {
        is_active = true;
      }
    }
    return is_active;
  }

  /// Determines the buffer fill limit at which we start to process the data
  int bufferTaskActivationLimit() {
    return static_cast<float>(active_percent) / 100.0 * buffer.size();
  }
};

}  // namespace snap_arduino
