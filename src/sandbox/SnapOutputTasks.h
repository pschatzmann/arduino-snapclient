#pragma once
#include "SnapOutputSimple.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

class SnapOutputTasks;
static SnapOutputTasks *selfSnapOutputTasks = nullptr;

/**
 * @brief Output Class which reads the data from the Queue and sends it
 * to the audio API
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 **/
class SnapOutputTasks : public SnapOutput {
public:
  // singleton: only access via instance class method
  SnapOutputTasks() : SnapOutput() { selfSnapOutputTasks = this; }

  /// Starts the processing which is also starting the the dsp_i2s_task_handler
  /// task
  bool begin() {
    const char *TAG = "I2S";
    ESP_LOGD(TAG, "start");

    createHeaderQueue();
    createRingBuffer();
    checkHeap();

    // start output task
    ESP_LOGI(TAG, "Start i2s task");
    xTaskCreatePinnedToCore(
        dsp_i2s_task_handler, "I2S_TASK", CONFIG_TASK_STACK_OUTPUT, nullptr,
        CONFIG_TASK_PRIORITY, &dsp_i2s_task_handle, CONFIG_TASK_CORE);

    return true;
  }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%zu", size);
    if (audio_ringbuffer == nullptr) {
      ESP_LOGE(TAG, "audio_ringbuffer is null");
      return 0;
    }
    BaseType_t done = xRingbufferSend(audio_ringbuffer, (void *)data, size,
                                      (portTickType)portMAX_DELAY);
    yield();
    return (done) ? size : 0;
  }

  /// Provides info about the audio data
  bool writeHeader(SnapAudioHeader &header) {
    ESP_LOGD(TAG, "start");
    if (header_queue == nullptr) {
      ESP_LOGE(TAG, "header_queue is null");
      return false;
    }
    return xQueueSend(header_queue, &header, (TickType_t)portMAX_DELAY) ==
           pdTRUE;
  }

  /// Ends the processing and releases the memory
  void end(void) {
    ESP_LOGD(TAG, "start");
    if (dsp_i2s_task_handle) {
      vTaskDelete(dsp_i2s_task_handle);
      dsp_i2s_task_handle = nullptr;
    }
    if (audio_ringbuffer) {
      vRingbufferDelete(audio_ringbuffer);
      audio_ringbuffer = nullptr;
    }
  }

  static void dsp_i2s_task_handler(void *arg) {
    selfSnapOutputTasks->local_dsp_i2s_task_handler(arg);
  }

  virtual void doLoop(){};

protected:
  xTaskHandle dsp_i2s_task_handle = nullptr;
  RingbufHandle_t audio_ringbuffer = nullptr;
  QueueHandle_t header_queue = nullptr;

  void local_dsp_i2s_task_handler(void *arg) {
    ESP_LOGI(TAG, "Waiting for data");
    uint32_t counter = 0;
    SnapAudioHeader header;
    while (true) {
      // receive header
      if (xQueueReceive(header_queue, &header, pdMS_TO_TICKS(5000)) == pdTRUE) {
        ESP_LOGI(TAG, "processing data...");
        if (counter == 0) {
          start_us = micros();
          first_header = header;
        }
        local_dsp_audio_write(counter, header.size);
      } else {
        ESP_LOGI(TAG, "waiting for data...");
        continue;
      }
      if (counter > 10)
        local_dsp_measure_time(header);
      if (counter++ % 100 == 0) {
        logHeap();
      }
      checkHeap();
    }
  }

  bool createHeaderQueue() {
    if (header_queue == nullptr) {
      header_queue = xQueueCreate(200, sizeof(SnapAudioHeader));
    }
    return header_queue != nullptr;
  }

  bool createRingBuffer() {
    if (audio_ringbuffer == nullptr) {
      int buffer_size = ESP.getPsramSize() > BUFFER_SIZE_PSRAM
                            ? BUFFER_SIZE_PSRAM
                            : BUFFER_SIZE_NO_PSRAM;
      audio_ringbuffer = xRingbufferCreate(buffer_size, RINGBUF_TYPE_BYTEBUF);
      if (audio_ringbuffer == nullptr) {
        ESP_LOGE(TAG, "nospace for ringbuffer");
        return false;
      }
    }
    ESP_LOGD(TAG, "Ringbuffer ok");
    return true;
  }

  void local_dsp_audio_write(uint32_t counter, size_t open) {
    ESP_LOGI(TAG, "packet %d -> %d", counter, open);
    // receive audio
    while (true) {
      size_t pxItemSize;
      void *result = xRingbufferReceiveUpTo(audio_ringbuffer, &pxItemSize,
                                            portMAX_DELAY, open);
      if (result != nullptr) {
        // write (encoded) audio
        ESP_LOGI(TAG, "playing packet %d", counter);
        size_t bytes_written = audioWrite(result, pxItemSize);
        if (bytes_written != pxItemSize) {
          ESP_LOGE(TAG, "audioWrite %d -> %d", pxItemSize, bytes_written);
        }
        assert(bytes_written == pxItemSize);
        // release memory
        vRingbufferReturnItem(audio_ringbuffer, result);
        break;
      } else {
        ESP_LOGE(TAG, "xRingbufferReceiveUpTo");
      }
    }
    ESP_LOGI(TAG, "end");
  }
};