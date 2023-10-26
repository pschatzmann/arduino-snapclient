/**
 * Global Shard Objects
 */

#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <sys/time.h>

#define WIFI_CONNECTED_BIT BIT0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t buffer_ms;
  xQueueHandle flow_queue;
  const char *mac_address;
  struct timeval tdif, tavg;
} SnapCtxDef;

extern SnapCtxDef ctx;

enum codec_type { NO_CODEC, PCM, FLAC, OGG, OPUS };

#ifdef __cplusplus
}
#endif
