/**
 * Logging: on the ESP32 we use the provided logger. On other platforms we write to stderr
 */

#pragma once

#ifdef ESP32
#  include "esp_log.h"
#else
#  define DEFAULT_LOG(...) fprintf(stderr,"[%s] ", __PRETTY_FUNCTION__);fprintf(stderr, __VA_ARGS__);fputc('\n',stderr)
#  define ESP_LOGE(tag, ...) DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGW(tag, ...) DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGI(tag, ...) DEFAULT_LOG(__VA_ARGS__)
#  define ESP_LOGD(tag, ...) 
#endif