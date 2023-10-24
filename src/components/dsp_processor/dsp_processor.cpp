
#include "Arduino.h" // for ESP.getPsramSize()
#include "SnapClient.h"
#include "config.h"
#include <stdint.h>
#include <sys/time.h>

#include "AudioToolsAPI.h"
//#include "dsps_biquad.h"
//#include "dsps_biquad_gen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "include/dsp_processor.h"

// #ifdef CONFIG_USE_BIQUAD_ASM
// #define BIQUAD dsps_biquad_f32_ae32
// #else
// #define BIQUAD dsps_biquad_f32
// #endif

uint32_t bits_per_sample = CONFIG_BITS_PER_SAMPLE;

static xTaskHandle s_dsp_i2s_task_handle = NULL;
static RingbufHandle_t s_ringbuf_i2s = NULL;

extern xQueueHandle i2s_queue;
extern xQueueHandle flow_queue;

extern struct timeval tdif;

extern uint32_t buffer_ms;
extern uint8_t muteCH[4];

// uint8_t dspFlow = dspfStereo; // dspfStereo; //dspfBassBoost; //dspfStereo;

// ptype_t bq[8];

void setup_dsp_i2s(uint32_t sample_rate, bool slave_i2s) {
  audio_begin(sample_rate, 16);
}

static void dsp_i2s_task_handler(void *arg) {
  const char *TAG = "i2s";

  struct timeval now, tv1;
  uint32_t cnt = 0;
  uint8_t *audio = NULL;
  uint8_t *drainPtr = NULL;
  uint8_t *timestampSize = NULL;
  // float sbuffer0[1024];
  // float sbuffer1[1024];
  // float sbuffer2[1024];
  // float sbufout0[1024];
  // float sbufout1[1024];
  // float sbufout2[1024];
  // float sbuftmp0[1024];

  // uint8_t dsp_audio[4 * 1024];
  // uint8_t dsp_audio1[4 * 1024];
  size_t n_byte_read = 0;
  size_t chunk_size = 0;
  size_t bytes_written = 0;
  // muteCH[0] = 0;
  // muteCH[1] = 0;
  // muteCH[2] = 0;
  // muteCH[3] = 0;
  int32_t age = 0;
  int32_t agesec;
  int32_t ageusec;
  int playback_synced = 0;
  uint32_t flow_que_msg = 0;
  int flow_state = 0;
  int flow_drain_counter = 0;
  double dynamic_vol = 1.0;
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

    if (xQueueReceive(flow_queue, &flow_que_msg, 0)) {
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
      selfSnapClient->writeSilence();
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

    timersub(&now, &tdif, &tv1);
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
      if (age < buffer_ms) { // Too slow speedup playback
        // rtc_clk_apll_enable(1, sdm0, sdm1, sdm2, odir);
        // rtc_clk_apll_enable(1, 149, 212, 5, 2);
      } // ESP_LOGI("i2s", "%d %d", buffer_ms, age );
      else {
        // rtc_clk_apll_enable(1, 149, 212, 5, 2);
      }
      // ESP_LOGI("i2s", "%d %d %1.2f ", buffer_ms, age, dynamic_vol );
    } else {
      while (age < buffer_ms) {
        vTaskDelay(2);
        gettimeofday(&now, NULL);
        timersub(&now, &tdif, &tv1);
        ageusec = tv1.tv_usec - ts_usec;
        agesec = tv1.tv_sec - ts_sec;
        if (ageusec < 0) {
          ageusec += 1000000;
          agesec -= 1;
        }
        age = agesec * 1000 + ageusec / 1000;

        ESP_LOGI(TAG, "%d %d syncing ... ", buffer_ms, age);
      }
      ESP_LOGI(TAG, "%d %d SYNCED ", buffer_ms, age);

      playback_synced = 1;
    }

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
        ESP_LOGI(TAG, "Chunk :%d %d ms", chunk_size, age);
      }

      // for (uint16_t i = 0; i < len; i++) {
      //   sbuffer0[i] =
      //       dynamic_vol * 0.5 *
      //       ((float)((int16_t)(audio[i * 4 + 1] << 8) + audio[i * 4 + 0])) /
      //       32768;
      //   sbuffer1[i] =
      //       dynamic_vol * 0.5 *
      //       ((float)((int16_t)(audio[i * 4 + 3] << 8) + audio[i * 4 + 2])) /
      //       32768;
      //   sbuffer2[i] = ((sbuffer0[i] / 2) + (sbuffer1[i] / 2));
      // }
      // switch (dspFlow) {
      // case dspfStereo: { // if (cnt%120==0)
        //{ ESP_LOGI(TAG, "In dspf Stero :%d",chunk_size);
        // ws_server_send_bin_client(0,(char*)audio, 240);
        // printf("%d %d \n",byteWritten, i2s_evt.size );
        //}
        // for (uint16_t i = 0; i < len; i++) {
        //   audio[i * 4 + 0] = (muteCH[0] == 1) ? 0 : audio[i * 4 + 0];
        //   audio[i * 4 + 1] = (muteCH[0] == 1) ? 0 : audio[i * 4 + 1];
        //   audio[i * 4 + 2] = (muteCH[1] == 1) ? 0 : audio[i * 4 + 2];
        //   audio[i * 4 + 3] = (muteCH[1] == 1) ? 0 : audio[i * 4 + 3];
        // }
        if (bits_per_sample == 16) {
          audio_write((char *)audio, chunk_size, &bytes_written);
        } else {
          audio_write_expand((char *)audio, chunk_size, 16, 32, &bytes_written);
        }
      // } break;
      // case dspfBassBoost: { // CH0 low shelf 6dB @ 400Hz
      //   BIQUAD(sbuffer0, sbufout0, len, bq[6].coeffs, bq[6].w);
      //   BIQUAD(sbuffer1, sbufout1, len, bq[7].coeffs, bq[7].w);
      //   int16_t valint[2];
      //   for (uint16_t i = 0; i < len; i++) {
      //     valint[0] =
      //         (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
      //     valint[1] =
      //         (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
      //     dsp_audio[i * 4 + 0] = (valint[0] & 0xff);
      //     dsp_audio[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
      //     dsp_audio[i * 4 + 2] = (valint[1] & 0xff);
      //     dsp_audio[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      //   }
      //   if (bits_per_sample == 16) {
      //     ((char *)dsp_audio, chunk_size, &bytes_written, portMAX_DELAY);
      //   } else {
      //     audio_write_expand((char *)dsp_audio, chunk_size, 16, 32,
      //                        &bytes_written, portMAX_DELAY);
      //   }

      // } break;
      // case dspfBiamp: {
      //   if (cnt % 120 == 0) {
      //     ESP_LOGI(TAG, "In dspf biamp :%d", chunk_size);
      //     // ws_server_send_bin_client(0,(char*)audio, 240);
      //     // printf("%d %d \n",byteWritten, i2s_evt.size );
      //   }
      //   // Process audio ch0 LOW PASS FILTER
      //   BIQUAD(sbuffer0, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      //   BIQUAD(sbuftmp0, sbufout0, len, bq[1].coeffs, bq[1].w);

      //   // Process audio ch1 HIGH PASS FILTER
      //   BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      //   BIQUAD(sbuftmp0, sbufout1, len, bq[3].coeffs, bq[3].w);

      //   int16_t valint[2];
      //   for (uint16_t i = 0; i < len; i++) {
      //     valint[0] =
      //         (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
      //     valint[1] =
      //         (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
      //     dsp_audio[i * 4 + 0] = (valint[0] & 0xff);
      //     dsp_audio[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
      //     dsp_audio[i * 4 + 2] = (valint[1] & 0xff);
      //     dsp_audio[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      //   }
      //   if (bits_per_sample == 16) {
      //     audio_write((char *)dsp_audio, chunk_size, &bytes_written,
      //                 portMAX_DELAY);
      //   } else {
      //     audio_write_expand((char *)dsp_audio, chunk_size, 16, 32,
      //                        &bytes_written, portMAX_DELAY);
      //   }
      // } break;

      // case dspf2DOT1: { // Process audio L + R LOW PASS FILTER
      //   BIQUAD(sbuffer2, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      //   BIQUAD(sbuftmp0, sbufout2, len, bq[1].coeffs, bq[1].w);

      //   // Process audio L HIGH PASS FILTER
      //   BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      //   BIQUAD(sbuftmp0, sbufout0, len, bq[3].coeffs, bq[3].w);

      //   // Process audio R HIGH PASS FILTER
      //   BIQUAD(sbuffer1, sbuftmp0, len, bq[4].coeffs, bq[4].w);
      //   BIQUAD(sbuftmp0, sbufout1, len, bq[5].coeffs, bq[5].w);

      //   int16_t valint[5];
      //   for (uint16_t i = 0; i < len; i++) {
      //     valint[0] =
      //         (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
      //     valint[1] =
      //         (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
      //     valint[2] =
      //         (muteCH[2] == 1) ? (int16_t)0 : (int16_t)(sbufout2[i] * 32768);
      //     dsp_audio[i * 4 + 0] = (valint[2] & 0xff);
      //     dsp_audio[i * 4 + 1] = ((valint[2] & 0xff00) >> 8);
      //     dsp_audio[i * 4 + 2] = 0;
      //     dsp_audio[i * 4 + 3] = 0;

      //     dsp_audio1[i * 4 + 0] = (valint[0] & 0xff);
      //     dsp_audio1[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
      //     dsp_audio1[i * 4 + 2] = (valint[1] & 0xff);
      //     dsp_audio1[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      //   }
      //   audio_write_expand((char *)dsp_audio, chunk_size, 16, 32,
      //                      &bytes_written, portMAX_DELAY);
      //   // audio_write_expand(1, (char *)dsp_audio1, chunk_size, 16, 32,
      //   //                  &bytes_written, portMAX_DELAY);
      // } break;
      // case dspfFunkyHonda: { // Process audio L + R LOW PASS FILTER
      //   BIQUAD(sbuffer2, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      //   BIQUAD(sbuftmp0, sbufout2, len, bq[1].coeffs, bq[1].w);

      //   // Process audio L HIGH PASS FILTER
      //   BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      //   BIQUAD(sbuftmp0, sbufout0, len, bq[3].coeffs, bq[3].w);

      //   // Process audio R HIGH PASS FILTER
      //   BIQUAD(sbuffer1, sbuftmp0, len, bq[4].coeffs, bq[4].w);
      //   BIQUAD(sbuftmp0, sbufout1, len, bq[5].coeffs, bq[5].w);

      //   uint16_t scale = 16384; // 32768
      //   int16_t valint[5];
      //   for (uint16_t i = 0; i < len; i++) {
      //     valint[0] =
      //         (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * scale);
      //     valint[1] =
      //         (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * scale);
      //     valint[2] =
      //         (muteCH[2] == 1) ? (int16_t)0 : (int16_t)(sbufout2[i] * scale);
      //     valint[3] = valint[0] + valint[2];
      //     valint[4] = -valint[2];
      //     valint[5] = -valint[1] - valint[2];
      //     dsp_audio[i * 4 + 0] = (valint[3] & 0xff);
      //     dsp_audio[i * 4 + 1] = ((valint[3] & 0xff00) >> 8);
      //     dsp_audio[i * 4 + 2] = (valint[2] & 0xff);
      //     dsp_audio[i * 4 + 3] = ((valint[2] & 0xff00) >> 8);

      //     dsp_audio1[i * 4 + 0] = (valint[4] & 0xff);
      //     dsp_audio1[i * 4 + 1] = ((valint[4] & 0xff00) >> 8);
      //     dsp_audio1[i * 4 + 2] = (valint[5] & 0xff);
      //     dsp_audio1[i * 4 + 3] = ((valint[5] & 0xff00) >> 8);
      //   }
      //   audio_write_expand((char *)dsp_audio, chunk_size, 16, 32,
      //                      &bytes_written, portMAX_DELAY);
      // } break;
      // default:
      //   break;
      // }

      vRingbufferReturnItem(s_ringbuf_i2s, (void *)audio);
    // }
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
      "buffer_ms parameter ignored \n";

#ifdef CONFIG_USE_PSRAM
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
  xTaskCreatePinnedToCore(dsp_i2s_task_handler, "DSP_I2S", CONFIG_TASK_STACK_DSP_I2S, NULL, 5,
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

// // ESP32 DSP processor
// //======================================================
// // Each time a buffer of audio is passed to the DSP - samples are
// // processed according to a dynamic list of audio processing nodes.

// // Each audio processor node consist of a data struct holding the
// // required weights and states for processing an automomous processing
// // function. The high level parameters is maintained in the structure
// // as well

// // Release - Prove off concept
// // ----------------------------------------
// // Fixed 2x2 biquad flow Xover for biAmp systems
// // Interface for cross over frequency and level

// void dsp_setup_flow(double freq, uint32_t samplerate) {
//   float f = freq / samplerate / 2.0;

//   bq[0] = (ptype_t){LPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[1] = (ptype_t){LPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[2] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[3] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[4] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[5] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[6] = (ptype_t){LOWSHELF, f, 6, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
//   bq[7] = (ptype_t){LOWSHELF, f, 6, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};

//   // pnode_t *aflow = new pnode_t;
//   // if (aflow == NULL) {
//   //   printf("Could not create node");
//   // }

//   for (uint8_t n = 0; n <= 7; n++) {
//     switch (bq[n].filtertype) {
//     case LOWSHELF:
//       dsps_biquad_gen_lowShelf_f32(bq[n].coeffs, bq[n].freq, bq[n].gain,
//                                    bq[n].q);
//       break;
//     case LPF:
//       dsps_biquad_gen_lpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
//       break;
//     case HPF:
//       dsps_biquad_gen_hpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
//       break;
//     default:
//       break;
//     }
//     for (uint8_t i = 0; i <= 4; i++) {
//       printf("%.6f ", bq[n].coeffs[i]);
//     }
//     printf("\n");
//   }
// }

// void dsp_set_xoverfreq(uint8_t freqh, uint8_t freql, uint32_t samplerate) {
//   float freq = freqh * 256 + freql;
//   printf("%f\n", freq);
//   float f = freq / samplerate / 2.;
//   for (int8_t n = 0; n <= 5; n++) {
//     bq[n].freq = f;
//     switch (bq[n].filtertype) {
//     case LPF:
//       for (uint8_t i = 0; i <= 4; i++) {
//         printf("%.6f ", bq[n].coeffs[i]);
//       }
//       printf("\n");
//       dsps_biquad_gen_lpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
//       for (uint8_t i = 0; i <= 4; i++) {
//         printf("%.6f ", bq[n].coeffs[i]);
//       }
//       printf("%f \n", bq[n].freq);
//       break;
//     case HPF:
//       dsps_biquad_gen_hpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
//       break;
//     default:
//       break;
//     }
//   }
// }
