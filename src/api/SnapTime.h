#pragma once
#include "AudioBasic/Collections/Vector.h"
#include "AudioConfig.h"
#include <stdint.h>
#include <sys/time.h>

/**
 * @briefW e use the the sys/time functions to represent the server time.
 * The local time will be measured with the help of the Arduino millis() method.
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

  /// Record the last time difference between client and server
  void setTimeDifferenceClientServerMs(int diff) { time_diff = diff; }

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

  bool printLocalTime() {
    const timeval val = time();
    auto *tm_result = gmtime(&val.tv_sec);

    char str[80];
    strftime(str, 80, "%d-%m-%Y %H-%M-%S", tm_result);
    ESP_LOGI(TAG, "Time is %s", str);
    return true;
  }

  // updates the time difference between the local and server time
  void updateServerTime(timeval &server) {
    server_time = server;
    server_ms = millis();
  }

  /// updates the actual time
  bool setTime(timeval time) {
    ESP_LOGI(TAG, "epoch: %lu", time.tv_sec);
#if !CONFIG_SNAPCLIENT_SNTP_ENABLE && CONFIG_SNAPCLIENT_SETTIME_ALLOWD
    int rc = settimeofday(&time, NULL);
    printLocalTime();
    return rc == 0;
#else
    ESP_LOGI(TAG, "setTime now allowed/active");
    return false;
#endif
  }

  /// Defines the message buffer lag
  void setDelay(int ms) {
    ESP_LOGI(TAG, "delay: %d", ms);
    message_buffer_delay_ms = ms;
  }

  /// Provides the effective delay to be used (Mesage buffer lag -
  /// decoding/playback time)
  int getStartDelay() {
    int delay = std::max(0, message_buffer_delay_ms - CONFIG_PLAYBACK_LAG_MS);
    ESP_LOGI(TAG, "delay: %d", delay);
    return delay;
  }

protected:
  int64_t time_diff = 0;
  const char *TAG = "SnapTime";
  uint32_t server_ms = 0;
  uint32_t local_ms;
  uint16_t message_buffer_delay_ms = 0;
  timeval server_time;
};
