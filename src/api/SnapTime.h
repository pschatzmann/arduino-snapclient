#pragma once
#include "AudioBasic/Collections/Vector.h"
#include "AudioConfig.h"
#include <stdint.h>
#include <sys/time.h>

namespace snap_arduino {

/**
 * @brief The the sys/time functions are used to represent the server time.
 * The local time will be measured with the help of the Arduino millis() method.
 * This class provides the basic functionality to translate between local and server time.
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
class SnapTime {
public:
  static SnapTime &instance() {
    static SnapTime self;
    return self;
  }

  /// Provides the actual time as timeval
  timeval time() {
    timeval result;
    int rc = gettimeofday(&result, NULL);
    if (rc) {
      uint32_t ms = millis();
      result.tv_sec = ms / 1000;
      result.tv_usec = (ms - (result.tv_sec * 1000)) * 1000;
    }
    return result;
  }

  /// Provides the current server time in ms
  uint32_t serverMillis() {
    return toMillis(server_time) - server_ms + millis() -
           timeDifferenceClientServerMs();
  }

  uint32_t localMillis() { return toMillis(time()); }

  /// Provides the avg latecy in milliseconds
  int timeDifferenceClientServerMs() {
    int result = time_diff;
    assert(result == time_diff);
    return result;
  }

  uint32_t toMillis(timeval tv) { return toMillis(tv.tv_sec, tv.tv_usec); }

  inline uint32_t toMillis(uint32_t sec, uint32_t usec) {
    return sec * 1000 + (usec / 1000);
  }

  bool printLocalTime(const char *msg) {
    const timeval val = time();
    auto *tm_result = gmtime(&val.tv_sec);

    char str[80];
    strftime(str, 80, "%d-%m-%Y %H-%M-%S", tm_result);
    ESP_LOGI(TAG, "%s: Time is %s", msg, str);
    return true;
  }

  /// Record the last time difference between client and server
  void setTimeDifferenceClientServerMs(uint32_t diff) {
    time_diff = diff;
    time_update_count++;
  }

  // updates the time difference between the local and server time
  void updateServerTime(timeval &server) {
    server_ms = millis();
    server_time = server;
  }

  // Calculat the difference between 2 timeval
  timeval timeDifference(timeval t1, timeval t2) {
    timeval result;
    timersub(&t1, &t2, &result);
    return result;
  }

  // Calculat the difference between 2 timeval -> result in ms
  uint32_t timeDifferenceMs(timeval t1, timeval t2) {
    timeval result;
    timersub(&t1, &t2, &result);
    return toMillis(result);
  }

#ifdef ESP32
  void setupSNTPTime() {
    ESP_LOGD(TAG, "start");
    const char *ntpServer = CONFIG_SNTP_SERVER;
    const long gmtOffset_sec = 1 * 60 * 60;
    const int daylightOffset_sec = 1 * 60 * 60;
    for (int retry = 0; retry < 5; retry++) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      tm time;
      if (!getLocalTime(&time)) {
        continue;
      }
      SnapTime::instance().printLocalTime("SNTP");
      has_sntp_time = true;
      break;
    }
  }
#endif

protected:
  const char *TAG = "SnapTime";
  uint32_t time_diff = 0;
  uint32_t server_ms = 0;
  uint32_t local_ms;
  uint32_t time_update_count = 0;
  timeval server_time;
  bool has_sntp_time = false;
  
};

}
