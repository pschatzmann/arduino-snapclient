#pragma once
#include "SnapOutput.h"

/**
 * @brief Direct output of received data
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 **/

class SnapOutputSimple : public SnapOutput {
public:
  SnapOutputSimple() = default;

  /// nop
  bool begin() {
    is_sync_active = true;
    return true;
  }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%zu", size);

    if(!synchAtStart()){
      return size;
    }

    size_t result = audioWrite(data, size);
    if (measure_stream.isUpdate()) measure_stream.logResult();
    return result;
  }

  /// nop
  bool writeHeader(SnapAudioHeader &header) {
    this->header = header;
    local_dsp_measure_time(header);
    return true;
  }

  /// nop
  void end(void) {}

  void doLoop() {}

protected:
  const char *TAG = "SnapOutputSimple";
  SnapAudioHeader header;
  bool is_sync_active = true;

  /// start to play audio only in valid server time
  bool synchAtStart(){
    // start audio when first package in the future becomes valid
    if (is_sync_active){
      auto msg_time = snap_time.toMillis(header.sec,header.usec);
      auto server_time = snap_time.serverMillis();
      if (msg_time < server_time){
        // ignore the data and report it as processed
        ESP_LOGW(TAG, "audio data expired: msg(%u) < time(%u)", msg_time, server_time);
        return false;
      } else {
        // wait for the audio to become valid
        int diff_ms = msg_time - server_time;
        delay(diff_ms + SnapTime::instance().getStartDelay());
        is_sync_active = false;
      }
    }
    return true;

  }
};