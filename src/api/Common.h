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

struct AudioHeader {
  int32_t sec = 0;
  int32_t usec = 0;
  size_t size = 0;
  codec_type codec = NO_CODEC;

  uint64_t operator-(AudioHeader&h1){
    return (sec - h1.sec) * 1000000 + usec - h1.usec;
  }
};

inline void checkHeap(){
#if CONFIG_CHECK_HEAP
    heap_caps_check_integrity_all(true);   
#endif 
}
