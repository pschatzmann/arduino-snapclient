/**
 * Global Shard Objects
 */

#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <sys/time.h>

enum codec_type { NO_CODEC, PCM, FLAC, OGG, OPUS };

struct SnapCtxDef {
  uint32_t buffer_ms =  400;
  const char *mac_address = "";
  struct timeval tdif, tavg;
};

struct AudioHeader {
  int32_t sec = 0;
  int32_t usec = 0;
  size_t size = 0;
  codec_type codec = NO_CODEC;
};

extern SnapCtxDef ctx;
