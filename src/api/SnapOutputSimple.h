#pragma once
#include "SnapOutput.h"

/**
 * Direct output of received data
 */

class SnapOutputSimple : public SnapOutput {
public:
  SnapOutputSimple() = default;

  /// nop
  bool begin() { return true; }

  /// Writes audio data to the queue
  size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%d", size);
    return audioWrite(data, size);
  }

  /// nop
  bool writeHeader(AudioHeader &header) { return true; }

  /// nop
  void end(void) {}

  void doLoop() {}

protected:
  const char *TAG = "SnapOutputSimple";
};