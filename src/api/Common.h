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

struct SnapCtxDef{
  uint32_t buffer_ms;
  const char *mac_address;
  struct timeval tdif, tavg;
};

struct AudioHeader {
  int32_t sec;
  int32_t usec;
  size_t size;
  codec_type codec;
};

extern SnapCtxDef ctx;


