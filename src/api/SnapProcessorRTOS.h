#pragma once
#include "SnapOutput.h"
#include "Concurrency/Concurrency.h"

namespace snap_arduino {

/**
 * @brief Processor for which the encoded output is buffered in a queue in order to
 * prevent any buffer underruns. A RTOS task feeds the output from the queue.
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2024-02-26
 * @copyright Copyright (c) 2023
 */
class SnapProcessorRTOS : public SnapProcessor {
 public:
  /// Default constructor
  SnapProcessorRTOS(SnapOutput &output, int buffer_size,
                    int activationAtPercent = 75)
      : SnapProcessor(output) {
    init_rtos(buffer_size, activationAtPercent);
  }
  /// Default constructor
  SnapProcessorRTOS(int buffer_size, int activationAtPercent = 75)
      : SnapProcessor() {
    init_rtos(buffer_size, activationAtPercent);
  }

  bool begin() override {
    // regular begin logic
    bool result = SnapProcessor::begin();
    // allocate buffer, so that we could use psram
    size_queue.resize(RTOS_MAX_QUEUE_ENTRY_COUNT);
    size_queue.setWriteMaxWait(5);
    buffer.resize(buffer_size);
    return result;
  }

  void end(void) override {
    task.suspend();
    task_started = false;
    size_queue.clear();
    buffer.reset();
    SnapProcessor::end();
  }

 protected:
  const char *TAG = "SnapProcessorRTOS";
  audio_tools::Task task{"output", RTOS_STACK_SIZE, RTOS_TASK_PRIORITY, 1};
  audio_tools::QueueRTOS<size_t> size_queue{0};
  audio_tools::BufferRTOS<uint8_t> buffer{0}; // size defined in constructor
  bool task_started = false;
  int active_percent;
  int buffer_size;
  static SnapProcessorRTOS *self;

  /// store parameters provided by constructor
  void init_rtos(int bufferSize, int activationAtPercent) {
    self = this;
    active_percent = activationAtPercent;
    buffer_size = bufferSize;
  }

  /// Writes the encoded audio data to a queue
  size_t writeAudio(const uint8_t *data, size_t size) override {

    if (size > buffer.size()){
      ESP_LOGE(TAG, "The buffer is too small. Use a multiple of %d", size);
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

    if (!size_queue.enqueue(size)) {
      ESP_LOGW(TAG, "size_queue full");
      return 0;
    }

    size_t size_written = buffer.writeArray(data, size);
    if (size_written != size) {
      ESP_LOGE(TAG, "buffer-overflow");
    }

    ESP_LOGD(TAG, "buffer %d - %d vs limit %d", size, buffer.available(),
             bufferTaskActivationLimit());
    if (!task_started && buffer.available() > bufferTaskActivationLimit()) {
      ESP_LOGI(TAG, "===> starting output task");
      task_started = true;
      task.begin(task_copy);
    }

    return size_written;
  }

  /// Determines the buffer fill limit at which we start to process the data
  int bufferTaskActivationLimit() {
    return static_cast<float>(active_percent) / 100.0 * buffer.size();
  }

  /// Copy the buffered data to the output
  void copy() {
    size_t size = 0;
    if (size_queue.dequeue(size)) {
      uint8_t data[size];
      int read = buffer.readArray(data, size);
      assert(read == size);
      int written = p_snap_output->audioWrite(data, size);
      if (written != size) {
        ESP_LOGW(TAG, "write %d of %d", written, size);
      }
    }
    delay(1);
  }

  /// static method for rtos task: make sure we constantly output audio
  static void task_copy() {
    while (self != nullptr) self->copy();
  }
};

SnapProcessorRTOS *SnapProcessorRTOS::self = nullptr;

}