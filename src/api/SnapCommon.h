/**
 * Global Shared Objects
 */

#pragma once
#include <iostream>
#include <iomanip>
#include <ctime>

namespace snap_arduino {

enum codec_type { NO_CODEC, PCM, FLAC, OGG, OPUS };
static const char *TAG="COMMON";
/**
 * @brief Information about the next bucket
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
struct SnapAudioHeader {
  int32_t sec = 0;
  int32_t usec = 0;
  size_t size = 0;
  codec_type codec = NO_CODEC;

  uint64_t operator-(SnapAudioHeader &h1) {
    return (sec - h1.sec) * 1000000 + usec - h1.usec;
  }
};

/// Recording of local_ms and server_ms
struct SnapTimePoints {
  uint32_t local_ms = millis();
  uint32_t server_ms;
  SnapTimePoints() = default;
  SnapTimePoints(uint32_t serverMs){
    server_ms = serverMs;
  }
};


inline void checkHeap() {
#if CONFIG_CHECK_HEAP && defined(ESP32)
  heap_caps_check_integrity_all(true);
#endif
}

inline void logHeap() {
#ifdef ESP32
  ESP_LOGD(TAG, "Free Heap: %d / Free Heap PSRAM %d", ESP.getFreeHeap(),
           ESP.getFreePsram());
#endif
}

}
