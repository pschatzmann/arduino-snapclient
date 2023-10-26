
#include "Arduino.h" // for ESP.getPsramSize()
#include "SnapClient.h"
#include "config.h"
#include <stdint.h>
#include <sys/time.h>

#include "api/SnapAudioToolsAPI.h"
#include "api/common.h"

//#include "dsps_biquad.h"
//#include "dsps_biquad_gen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "dsp_processor.h"

// #ifdef CONFIG_USE_BIQUAD_ASM
// #define BIQUAD dsps_biquad_f32_ae32
// #else
// #define BIQUAD dsps_biquad_f32
// #endif

uint32_t bits_per_sample = 16;

static xTaskHandle s_dsp_i2s_task_handle = NULL;
static RingbufHandle_t s_ringbuf_i2s = NULL;
// extern xQueueHandle i2s_queue;
// extern xQueueHandle flow_queue;
// extern struct timeval tdif;
// extern uint32_t ctx.buffer_ms;

// uint8_t dspFlow = dspfStereo; // dspfStereo; //dspfBassBoost; //dspfStereo;

// ptype_t bq[8];

void setup_dsp_i2s(uint32_t sample_rate, bool slave_i2s) {
  audio_begin(sample_rate, bits_per_sample);
}

void dsp_i2s_task_handler_legacy(void *arg) {
  const char *TAG = "i2s";
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

    ESP_LOGD(TAG, "xQueueReceive");
    assert(ctx.flow_queue!=NULL);
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
        s_ringbuf_i2s, &n_byte_read, (portTickType)40, 3 * 4);
    if (timestampSize == NULL) {
      ESP_LOGI(TAG, "Wait: no data in buffer %d %d", cnt, n_byte_read);
      // write silence to avoid noise
      selfSnapClient->setMute(true);
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
    vRingbufferReturnItem(s_ringbuf_i2s, (void *)timestampSize);

    // what's this for?
    // while (tdif.tv_sec == 0) {
    //  vTaskDelay(1);
    //}
    gettimeofday(&now, NULL);

    timersub(&now, &ctx.tdif, &tv1);
    // ESP_LOGI("log", "diff :% 11ld.%03ld ", tdif.tv_sec ,
    // tdif.tv_usec/1000); ESP_LOGI("log", "head :% 11ld.%03ld ", tv1.tv_sec ,
    // tv1.tv_usec/1000); ESP_LOGI("log", "tsamp :% 11d.%03d ", ts_sec ,
    // ts_usec/1000);

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

        ESP_LOGD(TAG, "%d %d syncing ... ", ctx.buffer_ms, age);
      }
      ESP_LOGD(TAG, "%d %d SYNCED ", ctx.buffer_ms, age);

      playback_synced = 1;
    }

    ESP_LOGD(TAG, "xRingbufferReceiveUpTo");
    audio = (uint8_t *)xRingbufferReceiveUpTo(s_ringbuf_i2s, &chunk_size,
                                              (portTickType)20, ts_size);
    if (chunk_size != ts_size) {
      uint32_t missing = ts_size - chunk_size;
      ESP_LOGI(
          TAG,
          "Error readding audio from ring buf : read %d of %d , missing %d",
          chunk_size, ts_size, missing);
      vRingbufferReturnItem(s_ringbuf_i2s, (void *)audio);
      uint8_t *ax = audio;
      ax = (uint8_t *)xRingbufferReceiveUpTo(s_ringbuf_i2s, &chunk_size,
                                             (portTickType)20, missing);
      ESP_LOGI(TAG, "Read the next %d ", chunk_size);
    }
    if ((flow_state >= 1) & (flow_drain_counter > 0)) {
      flow_drain_counter--;
      dynamic_vol = 1.0 / (20 - flow_drain_counter);
      if (flow_drain_counter == 0) {
        // Drain buffer
        vRingbufferReturnItem(s_ringbuf_i2s, (void *)audio);
        xRingbufferPrintInfo(s_ringbuf_i2s);

        size_t drainSize;
        drainPtr = (uint8_t *)xRingbufferReceive(s_ringbuf_i2s, &drainSize,
                                                 (portTickType)0);
        ESP_LOGI(TAG, "Drained Ringbuffer (bytes):%d ", drainSize);
        if (drainPtr != NULL) {
          vRingbufferReturnItem(s_ringbuf_i2s, (void *)drainPtr);
        }
        xRingbufferPrintInfo(s_ringbuf_i2s);
        playback_synced = 0;
        dynamic_vol = 1.0;

        continue;
      }
    }

    // {
      int16_t len = chunk_size / 4;
      if (cnt % 100 == 2) {
        ESP_LOGD(TAG, "Chunk :%d %d ms", chunk_size, age);
      }

      audio_write((char *)audio, chunk_size, &bytes_written);
      vRingbufferReturnItem(s_ringbuf_i2s, (void *)audio);
  }
}
// buffer size must hold 400ms-1000ms  // for 2ch16b48000 that is 76800 -
// 192000 or 75-188 x 1024

#define BUFFER_SIZE 192 * (3840 + 12)

//#define BUFFER_SIZE 192 * (3528 + 12)    // 44100/16/2
//(3840 + 12)  PCM 48000/16/2

// 3852 3528

void dsp_i2s_task_init(uint32_t sample_rate, bool slave) {
  const char *TAG = "I2S";
  ESP_LOGI(TAG, "dsp_i2s_task_init");

  setup_dsp_i2s(sample_rate, slave);
  ESP_LOGI(TAG, "Setup i2s dma and interface");

  const char *no_psram_msg =
      "Setup ringbuffer using internal ram - only space for 150ms - Snapcast "
      "ctx.buffer_ms parameter ignored \n";

#if CONFIG_USE_PSRAM
  if (ESP.getPsramSize() > 0) {
    printf("Setup ringbuffer using PSRAM \n");
    StaticRingbuffer_t *buffer_struct = (StaticRingbuffer_t *)heap_caps_malloc(
        sizeof(StaticRingbuffer_t), MALLOC_CAP_SPIRAM);
    printf("Buffer_struct ok\n");

    uint8_t *buffer_storage = (uint8_t *)heap_caps_malloc(
        sizeof(uint8_t) * BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    printf("Buffer_stoarge ok\n");
    s_ringbuf_i2s = xRingbufferCreateStatic(BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF,
                                            buffer_storage, buffer_struct);
    printf("Ringbuf ok\n");
  } else {
    printf(no_psram_msg);
    s_ringbuf_i2s = xRingbufferCreate(32 * 1024, RINGBUF_TYPE_BYTEBUF);
  }
#else
  printf(no_psram_msg);
  s_ringbuf_i2s = xRingbufferCreate(32 * 1024, RINGBUF_TYPE_BYTEBUF);
#endif
  if (s_ringbuf_i2s == NULL) {
    printf("nospace for ringbuffer\n");
    return;
  }
  printf("Ringbuffer ok\n");
  xTaskCreatePinnedToCore(dsp_i2s_task_handler_legacy, "DSP_I2S", CONFIG_TASK_STACK_DSP_I2S, NULL, 5,
                          &s_dsp_i2s_task_handle, 0);
}

void dsp_i2s_task_deinit(void) {
  if (s_dsp_i2s_task_handle) {
    vTaskDelete(s_dsp_i2s_task_handle);
    s_dsp_i2s_task_handle = NULL;
  }
  if (s_ringbuf_i2s) {
    vRingbufferDelete(s_ringbuf_i2s);
    s_ringbuf_i2s = NULL;
  }
}

size_t write_ringbuf(const uint8_t *data, size_t size) {
  BaseType_t done = xRingbufferSend(s_ringbuf_i2s, (void *)data, size,
                                    (portTickType)portMAX_DELAY);
  return (done) ? size : 0;
}

