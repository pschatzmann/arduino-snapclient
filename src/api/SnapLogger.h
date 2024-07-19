/**
 * Logging: on the ESP32 we use the provided logger. On other platforms we write to stderr
 */

#pragma once

#ifdef ESP32
#  include "esp_log.h"
#else
static char msg[100] = {0};
#  define DEFAULT_LOG(...) { Serial.println(__PRETTY_FUNCTION__); snprintf((char*)msg, 100, __VA_ARGS__); Serial.println(msg); }
#  define ESP_LOGE(tag,  ...) DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGW(tag,  ...) DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGI(tag,  ...) //DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGD(tag,  ...) //DEFAULT_LOG(__VA_ARGS__)
#endif