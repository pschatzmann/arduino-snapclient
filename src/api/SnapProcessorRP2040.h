#pragma once
#include "AudioTools/Concurrency/RP2040.h"
#include "SnapOutput.h"

namespace snap_arduino {

/**
 * @brief Processor for which the encoded output is buffered in a queue. The decoding and 
 * audio output can be done on the second core by calling loop1();
 * 
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2024-02-26
 * @copyright Copyright (c) 2023
 */
class SnapProcessorRP2040 : public SnapProcessor {
 public:
  /// Default constructor
  SnapProcessorRP2040(SnapOutput &output, int bufferSizeBytes, int activationAtPercent = 75)
      : SnapProcessor(output) {
    active_percent = activationAtPercent;
    initQueues(bufferSizeBytes);
  }
  /// Default constructor
  SnapProcessorRP2040(int bufferSizeBytes, int activationAtPercent = 75)
      : SnapProcessor() {
    active_percent = activationAtPercent;
    initQueues(bufferSizeBytes);
  }

  bool begin() override {
    ESP_LOGW(TAG, "begin: %d", buffer_count * 1024);
    // regular begin logic
    bool result = SnapProcessor::begin();

    // allocate buffer
    buffer.setBlockingRead(true);
    buffer.setBlockingWrite(true);
    buffer.resize(buffer_count * 1024);

    // we need to read back the buffer with the written sizes
    size_queue.resize(RTOS_MAX_QUEUE_ENTRY_COUNT);

    is_active = false;
    return result;
  }

  void end(void) override {
    size_queue.clear();
    buffer.reset();
    SnapProcessor::end();
  }

  bool doLoop1() override {
    ESP_LOGD(TAG, "doLoop1 %d / %d", size_queue.available(), buffer.available());
    if (!isBufferActive()) return true;

    size_t size = 0;
    if (size_queue.readArray(&size, 1)) {
      uint8_t data[size];
      int read = buffer.readArray(data, size);
      if (read != size){
        ESP_LOGE(TAG, "readArray failed %d -> %d", size, read);
      }
      int written = p_snap_output->audioWrite(data, size);
      if (written != size) {
        ESP_LOGE(TAG, "write error: %d of %d", written, size);
      }
    }
    return true;
  }

 protected:
  const char *TAG = "SnapProcessorRP2040";
  audio_tools::BufferRP2040T<size_t> size_queue{1, 0};
  audio_tools::BufferRP2040T<uint8_t> buffer{1024, 0};  // size defined in begin
  int buffer_count = 0;
  bool is_active = false;
  int active_percent = 0;

  bool isBufferActive() {
    if (!is_active && buffer.available()>0) {

      int limit = buffer.size() * active_percent / 100;
      if (buffer.available() >= limit) {
        LOGI("Setting buffer active");
        is_active = true;
      }
      delay(10);
    }
    return is_active;
  }

  /// store parameters provided by constructor
  void initQueues(int bufferSizeBytes) {
    buffer_count = bufferSizeBytes / 1024;
    if (buffer_count <= 2){
      buffer_count = 2;
    }
  }

  /// Writes the encoded audio data to a queue
  size_t writeAudio(const uint8_t *data, size_t size) override {

    if (size > buffer.size()) {
      ESP_LOGE(TAG, "The buffer is too small with %d. Use a multiple of %d", buffer.size(), size);
      stop();
    }

    ESP_LOGI(TAG, "size: %zu / buffer %d", size, buffer.available());
    if (!p_snap_output->isStarted() || size == 0) {
      ESP_LOGW(TAG, "not started");
      return 0;
    }

    if (!p_snap_output->synchronizePlayback()) {
      return size;
    }

    if (!size_queue.writeArray(&size, 1)) {
      ESP_LOGW(TAG, "size_queue full");
      return 0;
    }

    size_t size_written = buffer.writeArray(data, size);
    if (size_written != size) {
      ESP_LOGE(TAG, "buffer-overflow");
    }

    return size;
  }
};

}  // namespace snap_arduino
