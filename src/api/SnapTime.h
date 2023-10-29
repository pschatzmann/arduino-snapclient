#pragma once
#include "AudioConfig.h"
#include "AudioBasic/Collections/Vector.h"
#include <stdint.h>
#include <sys/time.h>

#define US_PER_SECOND 1000000l

/**
 * We use the the sys/time functions to represent the server time.
 * The local time will be measured with the help of the Arduino millis() method.
 */
class SnapTime {
public:
  static SnapTime &instance() {
    static SnapTime self;
    return self;
  }

  void setTime(timeval ttx) {
    timeval diff, result;
    diff.tv_usec = latencyMs() * 1000;
    sub_timeval(ttx, diff, &result);
  }

  /// Record the last latency value so that we can calculate the avg
  void addLatency(int latency) {
    if (latency_vector.size() > 50) {
      latency_vector.pop_front();
    }
    latency_vector.push_back(latency);
  }

  /// Provides the current server time
  uint64_t serverMillis() {
    //timeval time_server;
    //gettimeofday(&time_server, nullptr);
    //return toMillis(time_server.tv_sec, time_server.tv_usec);
    return millis();
  }

  /// Provides the avg latecy in milliseconds
  int latencyMs() {
    if (latency_vector.size() == 0)
      return 0;
    // calculate avg latency
    int64_t total = 0;
    for (int j = 0; j < latency_vector; j++) {
      total += latency_vector[j];
    }
    return total / latency_vector.size();
  }

  uint64_t toMillis(uint32_t sec, uint32_t usec) {
    return sec * 1000 + (usec / 1000);
  }

protected:
  Vector<int> latency_vector;

  void sub_timeval(struct timeval t1, struct timeval t2, struct timeval *td) {
    td->tv_usec = t2.tv_usec - t1.tv_usec;
    td->tv_sec = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_usec < 0) {
      td->tv_usec += US_PER_SECOND;
      td->tv_sec--;
    } else if (td->tv_sec < 0 && td->tv_usec > 0) {
      td->tv_usec -= US_PER_SECOND;
      td->tv_sec++;
    }
  }

  void add_timeval(struct timeval t1, struct timeval t2, struct timeval *td) {
    td->tv_usec = t2.tv_usec + t1.tv_usec;
    td->tv_sec = t2.tv_sec + t1.tv_sec;
    if (td->tv_usec >= US_PER_SECOND) {
      td->tv_usec -= US_PER_SECOND;
      td->tv_sec++;
    } else if (td->tv_usec <= -US_PER_SECOND) {
      td->tv_usec += US_PER_SECOND;
      td->tv_sec--;
    }
  }
};
