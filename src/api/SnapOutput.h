#pragma once

#include "Arduino.h" // for ESP.getPsramSize()
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <sys/time.h>

#include "api/Common.h"
#include "config.h"

/**
 * Output Class which reads the data from the Queue and sends it out
 * to the audio API
 */
class SnapOutput {

public:
  static SnapOutput &instance() {
    static SnapOutput self;
    return self;
  }

  /// Starts the processing which is also starting the the dsp_i2s_task_handler
  /// task
  void begin(uint32_t sample_rate) {
    const char *TAG = "I2S";
    ESP_LOGD(TAG, "");

    // allow amplification
    auto vol_cfg = vol_stream.defaultConfig();
    vol_cfg.allow_boost = true;
    vol_cfg.channels = 2;
    vol_cfg.bits_per_sample = 16;
    vol_stream.begin(vol_cfg);

    if (ringbuf_i2s == nullptr) {
      int buffer_size = ESP.getPsramSize() > BUFFER_SIZE_PSRAM
                            ? BUFFER_SIZE_PSRAM
                            : BUFFER_SIZE_NO_PSRAM;
      ringbuf_i2s = xRingbufferCreate(buffer_size, RINGBUF_TYPE_BYTEBUF);
      if (ringbuf_i2s == nullptr) {
        printf("nospace for ringbuffer\n");
        return;
      }
    }
    printf("Ringbuffer ok\n");

    if (header_queue == nullptr){
      header_queue = xQueueCreate(100, sizeof(AudioHeader)); 
    }

    setup_dsp_i2s(sample_rate);
    ESP_LOGI(TAG, "Start i2s task");
    xTaskCreatePinnedToCore(dsp_i2s_task_handler, "I2S_TASK",
                            CONFIG_TASK_STACK_DSP_I2S, NULL, CONFIG_TASK_PRIORITY,
                            &dsp_i2s_task_handle, CONFIG_TASK_CORE);
  }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%d", size);
    if (ringbuf_i2s == nullptr)
      return 0;
    BaseType_t done = xRingbufferSend(ringbuf_i2s, (void *)data, size,
                                      (portTickType)portMAX_DELAY);
    return (done) ? size : 0;
  }

  bool writeHeader(AudioHeader &header){
    return xQueueSend(header_queue, &header, (TickType_t)portMAX_DELAY)==pdTRUE;
  }

  /// Ends the processing and releases the memory
  void end(void) {
    ESP_LOGD(TAG, "");
    if (dsp_i2s_task_handle) {
      vTaskDelete(dsp_i2s_task_handle);
      dsp_i2s_task_handle = NULL;
    }
    if (ringbuf_i2s) {
      vRingbufferDelete(ringbuf_i2s);
      ringbuf_i2s = NULL;
    }
  }

  /// Adjust the volume
  void setVolume(float vol) {
    this->vol = vol / 100.0;
    ESP_LOGI(TAG, "Volume: %f", this->vol);
    vol_stream.setVolume(this->vol * vol_factor);
  }

  /// provides the actual volume
  float volume() { return vol; }

  /// mute / unmute
  void setMute(bool mute) {
    is_mute = mute;
    vol_stream.setVolume(mute ? 0 : vol);
    writeSilence();
  }

  /// checks if volume is mute
  bool isMute() { return is_mute; }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { vol_factor = fact; }

  void setOutput(AudioOutput &output) {
    this->out = &output;
    vol_stream.setTarget(output);
    decoder_stream.setStream(&vol_stream);
  }

  void setDecoder(AudioDecoder &dec){
    decoder_stream.setDecoder(&dec);
  }

  void setAudioInfo(AudioInfo info){
    out->setAudioInfo(info);
    decoder_stream.setAudioInfo(info);
    vol_stream.setAudioInfo(info);
  }

protected:
  const char *TAG = "SnapOutput";
  xTaskHandle dsp_i2s_task_handle = NULL;
  RingbufHandle_t ringbuf_i2s = NULL;
  QueueHandle_t header_queue;
  EncodedAudioStream decoder_stream;
  AudioOutput *out = nullptr;
  VolumeStream vol_stream;
  float vol = 1.0;
  float vol_factor = 1.0;
  bool is_mute = false;

  SnapOutput() = default;

  void writeSilence() {
    for (int j = 0; j < 50; j++) {
      out->writeSilence(1024);
    }
  }

  bool audioBegin(uint32_t sample_rate, uint8_t bits) {
    ESP_LOGD(TAG, "sample_rate: %d, bits: %d", sample_rate, bits);
    if (out == nullptr)
      return false;
    AudioInfo info;
    info.sample_rate = sample_rate;
    info.bits_per_sample = bits;
    info.channels = 2;

    setAudioInfo(info);
    bool result = out->begin();
    ESP_LOGD(TAG, "end");
    return result;
  }

  void audioEnd() {
    ESP_LOGD(TAG, "");
    if (out == nullptr)
      return;
    out->end();
  }

  // split up writes into batches of 512 bytes
  bool audioWrite(const void *src, size_t size, size_t *bytes_written) {
    ESP_LOGD(TAG, "%d", size);
    size_t result = 0;
    if (out != nullptr) {
      int open = size;
      int written = 0;
      while(open>0) {
        int max_write = std::min((int)size, 512);
        int result = decoder_stream.write((const uint8_t *)src+written, max_write);
        written += result;
        open -= result;
      }
      result = written;
    }
    if (bytes_written) *bytes_written = result;
    return result > 0;
  }

  void setup_dsp_i2s(uint32_t sample_rate) {
    ESP_LOGD(TAG, "sample_rate: %d", sample_rate);
    audioBegin(sample_rate, 16);
  }

  static void dsp_i2s_task_handler(void *arg) {
    instance().local_dsp_i2s_task_handler(arg);
  }

  void local_dsp_i2s_task_handler(void *arg) {
    ESP_LOGD(TAG, "");
    uint32_t counter = 0;

    AudioHeader audio_info;
    while (true) {
      // receive header
      xQueueReceive(header_queue, &audio_info, portMAX_DELAY);

      // receive audio
      size_t pxItemSize;
      void* result = xRingbufferReceive(ringbuf_i2s, &pxItemSize, portMAX_DELAY);

      // write (encoded) audio
      size_t bytes_written;
      audioWrite(result, pxItemSize, &bytes_written);

      // release memory
      vRingbufferReturnItem(ringbuf_i2s, (void *)result);
      if (counter++ % 100 == 0) {
        ESP_LOGI(TAG, "Free Heap: %d / Free Heap PSRAM %d",ESP.getFreeHeap(),ESP.getFreePsram());
      }
    }
  }
};