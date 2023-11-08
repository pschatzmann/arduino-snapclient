#pragma once
#include "AudioTools.h"
#include "SnapLogger.h"
#include "SnapTime.h"

/**
 * @brief Abstract (Common) Time Synchronization Logic
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
    update_count = 0;
    sample_rate = rate;
  }

  void updateServerTime(uint32_t serverMillis) {
    update_count++;
    active = true;
    SnapTimePoints tp{serverMillis};
    if (time_points.size()>=interval){
      time_points.pop_front();
    }
    time_points.push_back(tp);
  }

  /// Calculate the resampling factor: with a positive delay we play too fast
  /// and need to slow down
  virtual float getFactor() = 0;

  bool isSync() {
    bool result = active && update_count > 2 && update_count % interval == 0;
    active = false;
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

  /// Provides the effective delay to be used (Message buffer lag -
  /// decoding/playback time)
  int getStartDelay() {
    int delay = std::max(0, message_buffer_delay_ms - processing_lag);
    ESP_LOGD(TAG, "delay: %d", delay);
    return delay;
  }

protected:
  const char *TAG = "SnapTimeSync";
  uint64_t update_count = 0;
  float sample_rate = 48000;
  int interval = 10;
  bool active = false;
  Vector<SnapTimePoints> time_points;
  // start delay
  uint16_t processing_lag = 0;
  uint16_t message_buffer_delay_ms = 0;

};

/**
 * @brief Dynamically adjusts the effective playback sample rate based on the differences
 * of the local and server clock.
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

  float getFactor() {
    int last_idx = time_points.size()-1;
    if (last_idx <=1) return 1.0;
    float timespan_local_ms = time_points[last_idx].local_ms - time_points[0].local_ms;
    float timespan_server_ms = time_points[last_idx].server_ms - time_points[0].server_ms;
    if (timespan_local_ms == 0) return 1.0;
    // if server time span is smaller then local, local runs faster and needs to be slowed down
    float result_factor = timespan_server_ms / timespan_local_ms;    
    ESP_LOGI(TAG, "=> adjusting playback speed by factor: %f", result_factor);
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

