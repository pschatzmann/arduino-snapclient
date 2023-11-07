#pragma once
#include "AudioTools.h"
#include "SnapLogger.h"
#include "SnapTime.h"

/**
 * @brief Abstract Time Synchronization
 * The delay has a big variablility: therefore we calculate the avg.
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
*/
class SnapTimeSync {
public:
  SnapTimeSync(int processingLag = CONFIG_PROCESSING_TIME_MS, int interval = 10) {
    setInterval(interval);
    setProcessingLag(processingLag);
  }

  void begin(int rate) {
    delay_count = 0;
    delay_total = 0;
    sample_rate = rate;
    is_active = false;
  }

  /// Calculate the resampling factor: with a positive delay we play too fast
  /// and need to slow down
  virtual float getFactor() = 0;

  /// Register the delay
  void addDelay(int delay) {
    if (abs(delay) > 5000)
      return;
    // ESP_LOGD(TAG, "delay: %d", delay);
    delay_count++;
    delay_total += delay;
    // ignore first
    if (!is_active && delay_count > 50) {
      is_active = true;
      delay_count = 0;
      delay_total = 0;
    }
  }
  // returns true when we need to update the rate: we update at fixed interval
  // times
  bool isSync() {
    bool result = is_active && delay_count > 0 && delay_count % interval == 0;
    return result;
  }

  /// Defines the message buffer lag
  void setMessageBufferDelay(int ms) {
    ESP_LOGI(TAG, "delay: %d", ms);
    message_buffer_delay_ms = ms;
  }

  /// Defines the lag which is substracted from the message_buffer_delay_ms. It
  /// conists of the delay added by the decoder and your selected output device
  void setProcessingLag(int lag) { this->processing_lag = lag; }

  /// Defines the interval that is used to adjust the sample rate: 10 means
  /// every 10 updates.
  void setInterval(int interval) { this->interval = interval; }

  /// Provides the effective delay to be used (Mesage buffer lag -
  /// decoding/playback time)
  int getStartDelay() {
    int delay = std::max(0, message_buffer_delay_ms - processing_lag);
    ESP_LOGD(TAG, "delay: %d", delay);
    return delay;
  }

protected:
  const char *TAG = "SnapTimeSync";
  uint64_t delay_count = 0;
  uint64_t delay_total = 0;
  float sample_rate = 48000;
  int interval = 10;
  bool is_active = false;
  // start delay
  uint16_t processing_lag = 0;
  uint16_t message_buffer_delay_ms = 0;

  // provides the effective avg delay corrected by the start delay (so that the
  // to be avg is 0)
  int avgDelay() { return (delay_total / delay_count) - getStartDelay(); }
};

/**
 * @brief Dynamically adjusts the effective playback sample rate
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 **/
class SnapTimeSyncDynamic : public SnapTimeSync {
public:
  SnapTimeSyncDynamic(int processingLag = CONFIG_PROCESSING_TIME_MS,
                      int interval = 10)
      : SnapTimeSync(processingLag, interval) {}
  /// Calculate the resampling factor: with a positive delay we play too fast
  /// and need to slow down
  float getFactor() {
    int delay = avgDelay();
    float samples_diff = sample_rate * delay / 1000.0;
    float result_factor = sample_rate / (sample_rate + samples_diff);
    ESP_LOGI(TAG, "delay ms: %d -> factor: %f", del, result);
    return result_factor;
  }
};

/**
 * @brief Uses predefined fixed factor
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 **/
class SnapTimeSyncFixed : public SnapTimeSync {
public:
  SnapTimeSyncFixed(float factor = 1.0,
                    int processingLag = CONFIG_PROCESSING_TIME_MS,
                    int interval = 10)
      : SnapTimeSync(processingLag, interval) {
    resample_factor = factor;
  }
  float getFactor() { return resample_factor; }

protected:
  float resample_factor;
};
