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
  bool begin() { return true; }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%zu", size);
    return audioWrite(data, size);
  }

  /// nop
  bool writeHeader(SnapAudioHeader &header) { return true; }

  /// nop
  void end(void) {}

  void doLoop() {}

protected:
  const char *TAG = "SnapOutputSimple";
};