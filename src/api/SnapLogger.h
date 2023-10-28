#pragma once

#ifdef ESP32
#  include "esp_log.h"
#else
#  define ESP_LOGE(tag, format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#  define ESP_LOGW(tag, format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#  define ESP_LOGI(tag, format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#  define ESP_LOGD(tag, format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#endif