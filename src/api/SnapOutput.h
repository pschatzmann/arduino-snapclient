#pragma once

#include "Arduino.h" // for ESP.getPsramSize()
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include <stdint.h>
#include <sys/time.h>

#include "config.h"
#include "SnapAudioToolsAPI.h"
#include "api/common.h"


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

  /// Starts the processing which is also starting the the dsp_i2s_task_handler task
  void begin(uint32_t sample_rate) {
    const char *TAG = "I2S";
    ESP_LOGD(TAG, "");

#if CONFIG_USE_PSRAM
    if (ESP.getPsramSize() > 0) {
      heap_caps_malloc_extmem_enable(CONFIG_PSRAM_LIMIT);
    } else {
      ESP_LOGW(TAG, "No PSRAM available or PSRAM not activated");
    }
#endif

    if (ringbuf_i2s == NULL) {
      int buffer_size =  ESP.getPsramSize() > BUFFER_SIZE_PSRAM ? BUFFER_SIZE_PSRAM : BUFFER_SIZE_NO_PSRAM;
      ringbuf_i2s = xRingbufferCreate(buffer_size, RINGBUF_TYPE_BYTEBUF);
      if (ringbuf_i2s == NULL) {
        printf("nospace for ringbuffer\n");
        return;
      }
    }
    printf("Ringbuffer ok\n");

    setup_dsp_i2s(sample_rate);
    ESP_LOGI(TAG, "Start i2s task");
    xTaskCreatePinnedToCore(dsp_i2s_task_handler, "I2S_TASK",
                            CONFIG_TASK_STACK_DSP_I2S, NULL, 5,
                            &dsp_i2s_task_handle, 0);
  }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%d", size);
    if(ringbuf_i2s == nullptr) return 0;
    BaseType_t done = xRingbufferSend(ringbuf_i2s, (void *)data, size,
                                      (portTickType)portMAX_DELAY);
    return (done) ? size : 0;
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

protected:
  xTaskHandle dsp_i2s_task_handle = NULL;
  RingbufHandle_t ringbuf_i2s = NULL;
  const char *TAG = "SnapOutput";

  SnapOutput() = default;

  void setup_dsp_i2s(uint32_t sample_rate) {
    ESP_LOGD(TAG, "sample_rate: %d", sample_rate);
    audio_begin(sample_rate, 16);
  }

  static void dsp_i2s_task_handler(void *arg) {
    instance().local_dsp_i2s_task_handler(arg);
  }

  void local_dsp_i2s_task_handler(void *arg) {
    ESP_LOGD(TAG, "");

    struct timeval now, tv1;
    uint32_t cnt = 0;
    uint8_t *audio = NULL;
    uint8_t *drainPtr = NULL;
    uint8_t *timestampSize = NULL;
    size_t n_byte_read = 0;
    size_t chunk_size = 0;
    size_t bytes_written = 0;
    int32_t age = 0;
    int32_t agesec;
    int32_t ageusec;
    int playback_synced = 0;
    uint32_t flow_que_msg = 0;
    int flow_state = 0;
    int flow_drain_counter = 0;
    float dynamic_vol = 1.0;
    while (true) {
      // 23.12.2020 - JKJ : Buffer protocol major change
      //   - Audio data prefaced with timestamp and size tag server timesamp
      //   (sec/usec (uint32_t)) and number of bytes (uint32_t)
      //   - DSP processor is now last in line to quanity if data must be passed
      //   on to hw i2s interface buffer or delayed based on
      //     timestamp
      //   - Timestamp vs now must be use to schedul if pack must be delay,
      //   dropped or played

      cnt++;

      ESP_LOGI(TAG, "xQueueReceive");
      assert(ctx.flow_queue != NULL);
      if (xQueueReceive(ctx.flow_queue, &flow_que_msg, 0)) {
        ESP_LOGI(TAG, "FLOW Queue message: %d ", flow_que_msg);
        if (flow_state != flow_que_msg) {
          switch (flow_que_msg) {
          case 2: // front end timed out -- network congestion more then 200
                  // msec or source off
            if (flow_state == 1) {
              flow_state = 1;
            } else {
              flow_state = 2;
              flow_drain_counter = 20;
              playback_synced = 0;
            }
            break;
          case 1: // Server has muted this channel. Turn down the volume over
                  // next 10 packages
            flow_state = 1;
            flow_drain_counter = 20;
            playback_synced = 0;
            break;
          case 0: // Reset sync counter and
            flow_state = 0;
            playback_synced = 0;
            dynamic_vol = 1.0;
            break;
          }
        }
      }

      timestampSize = (uint8_t *)xRingbufferReceiveUpTo(
          ringbuf_i2s, &n_byte_read, (portTickType)40, 3 * 4);
      if (timestampSize == NULL) {
        ESP_LOGI(TAG, "Wait: no data in buffer %d %d", cnt, n_byte_read);
        // write silence to avoid noise
        // selfSnapClient->writeSilence();
        audio_mute(true);
        continue;
      }

      if (n_byte_read != 12) {
        ESP_LOGE(TAG, "error read from ringbuf %d ", n_byte_read);
        // TODO find a decent stategy to fall back on our feet...
      }

      uint32_t ts_sec = *(timestampSize + 0) + (*(timestampSize + 1) << 8) +
                        (*(timestampSize + 2) << 16) +
                        (*(timestampSize + 3) << 24);

      uint32_t ts_usec = *(timestampSize + 4) + (*(timestampSize + 5) << 8) +
                         (*(timestampSize + 6) << 16) +
                         (*(timestampSize + 7) << 24);

      uint32_t ts_size = *(timestampSize + 8) + (*(timestampSize + 9) << 8) +
                         (*(timestampSize + 10) << 16) +
                         (*(timestampSize + 11) << 24);
      // free the item (timestampSize) space in the ringbuffer
      vRingbufferReturnItem(ringbuf_i2s, (void *)timestampSize);

      // what's this for?
      // while (ctx.tdif.tv_sec == 0) {
      //  vTaskDelay(1);
      //}
      gettimeofday(&now, NULL);

      timersub(&now, &ctx.tdif, &tv1);
      // ESP_LOGI("log", "diff :% 11ld.%03ld ", ctx.tdif.tv_sec ,
      // ctx.tdif.tv_usec/1000); ESP_LOGI("log", "head :% 11ld.%03ld ",
      // tv1.tv_sec , tv1.tv_usec/1000); ESP_LOGI("log", "tsamp :% 11d.%03d ",
      // ts_sec , ts_usec/1000);

      ageusec = tv1.tv_usec - ts_usec;
      agesec = tv1.tv_sec - ts_sec;
      if (ageusec < 0) {
        ageusec += 1000000;
        agesec -= 1;
      }
      age = agesec * 1000 + ageusec / 1000;
      if (age < 0) {
        age = 0;
      }

      if (playback_synced == 1) {
        if (age < ctx.buffer_ms) { // Too slow speedup playback
          // rtc_clk_apll_enable(1, sdm0, sdm1, sdm2, odir);
          // rtc_clk_apll_enable(1, 149, 212, 5, 2);
        } // ESP_LOGI("i2s", "%d %d", ctx.buffer_ms, age );
        else {
          // rtc_clk_apll_enable(1, 149, 212, 5, 2);
        }
        // ESP_LOGI("i2s", "%d %d %1.2f ", ctx.buffer_ms, age, dynamic_vol );
      } else {
        while (age < ctx.buffer_ms) {
          vTaskDelay(2);
          gettimeofday(&now, NULL);
          timersub(&now, &ctx.tdif, &tv1);
          ageusec = tv1.tv_usec - ts_usec;
          agesec = tv1.tv_sec - ts_sec;
          if (ageusec < 0) {
            ageusec += 1000000;
            agesec -= 1;
          }
          age = agesec * 1000 + ageusec / 1000;

          ESP_LOGI(TAG, "%d %d syncing ... ", ctx.buffer_ms, age);
        }
        ESP_LOGI(TAG, "%d %d SYNCED ", ctx.buffer_ms, age);

        playback_synced = 1;
      }

      audio = (uint8_t *)xRingbufferReceiveUpTo(ringbuf_i2s, &chunk_size,
                                                (portTickType)20, ts_size);
      if (chunk_size != ts_size) {
        uint32_t missing = ts_size - chunk_size;
        ESP_LOGI(
            TAG,
            "Error readding audio from ring buf : read %d of %d , missing %d",
            chunk_size, ts_size, missing);
        vRingbufferReturnItem(ringbuf_i2s, (void *)audio);
        uint8_t *ax = audio;
        ax = (uint8_t *)xRingbufferReceiveUpTo(ringbuf_i2s, &chunk_size,
                                               (portTickType)20, missing);
        ESP_LOGI(TAG, "Read the next %d ", chunk_size);
      }
      if ((flow_state >= 1) & (flow_drain_counter > 0)) {
        flow_drain_counter--;
        dynamic_vol = 1.0 / (20 - flow_drain_counter);
        if (flow_drain_counter == 0) {
          // Drain buffer
          vRingbufferReturnItem(ringbuf_i2s, (void *)audio);
          xRingbufferPrintInfo(ringbuf_i2s);

          size_t drainSize;
          drainPtr = (uint8_t *)xRingbufferReceive(ringbuf_i2s, &drainSize,
                                                   (portTickType)0);
          ESP_LOGI(TAG, "Drained Ringbuffer (bytes):%d ", drainSize);
          if (drainPtr != NULL) {
            vRingbufferReturnItem(ringbuf_i2s, (void *)drainPtr);
          }
          xRingbufferPrintInfo(ringbuf_i2s);
          playback_synced = 0;
          dynamic_vol = 1.0;

          continue;
        }
      }

      // {
      int16_t len = chunk_size / 4;
      if (cnt % 100 == 2) {
        ESP_LOGI(TAG, "Chunk :%d %d ms", chunk_size, age);
      }
      audio_write((char *)audio, chunk_size, &bytes_written);

      vRingbufferReturnItem(ringbuf_i2s, (void *)audio);
    }
  }
};