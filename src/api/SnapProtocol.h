/**
 * Snapcast Protocol
 */

#pragma once

#include "SnapConfig.h"
#include "SnapLogger.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define BASE_MESSAGE_SIZE 26
#define TIME_MESSAGE_SIZE 8
#define MAX_JSON_LEN 256

namespace snap_arduino {

/// @brief Buffer to read different data types
struct SnapReadBuffer {
  const char *buffer;
  size_t size, index;

  void begin(const char *data, size_t size) {
    this->buffer = data;
    this->size = size;
    this->index = 0;
  }

  int read(char *data, size_t size) {
    int i;

    if (this->size - this->index < size) {
      return 1;
    }

    for (i = 0; i < size; i++) {
      data[i] = this->buffer[this->index++];
    }

    return 0;
  }

  int read_uint32(uint32_t *data) {
    if (this->size - this->index < sizeof(uint32_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    *data |= (this->buffer[this->index++] & 0xff) << 8;
    *data |= (this->buffer[this->index++] & 0xff) << 16;
    *data |= (this->buffer[this->index++] & 0xff) << 24;
    return 0;
  }

  int read_uint16(uint16_t *data) {
    if (this->size - this->index < sizeof(uint16_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    *data |= (this->buffer[this->index++] & 0xff) << 8;
    return 0;
  }

  int read_uint8(uint8_t *data) {
    if (this->size - this->index < sizeof(uint8_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    return 0;
  }

  int read_int32(int32_t *data) {
    if (this->size - this->index < sizeof(int32_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    *data |= (this->buffer[this->index++] & 0xff) << 8;
    *data |= (this->buffer[this->index++] & 0xff) << 16;
    *data |= (this->buffer[this->index++] & 0xff) << 24;
    return 0;
  }

  int read_int16(int16_t *data) {
    if (this->size - this->index < sizeof(int16_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    *data |= (this->buffer[this->index++] & 0xff) << 8;
    return 0;
  }

  int read_int8(int8_t *data) {
    if (this->size - this->index < sizeof(int8_t)) {
      return 1;
    }

    *data = this->buffer[this->index++] & 0xff;
    return 0;
  }
};

/// @brief Buffer to write different data types
struct SnapWriteBuffer {
  char *buffer;
  size_t size, index;

  void begin(char *data, size_t size) {
    this->buffer = data;
    this->size = size;
    this->index = 0;
  }

  int write(const char *data, size_t size) {
    int i;

    if (this->size - this->index < size) {
      return 1;
    }

    for (i = 0; i < size; i++) {
      this->buffer[this->index++] = data[i];
    }

    return 0;
  }

  int write_uint32(uint32_t data) {
    if (this->size - this->index < sizeof(uint32_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    this->buffer[this->index++] = (data >> 8) & 0xff;
    this->buffer[this->index++] = (data >> 16) & 0xff;
    this->buffer[this->index++] = (data >> 24) & 0xff;
    return 0;
  }

  int write_uint16(uint16_t data) {
    if (this->size - this->index < sizeof(uint16_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    this->buffer[this->index++] = (data >> 8) & 0xff;
    return 0;
  }

  int write_uint8(uint8_t data) {
    if (this->size - this->index < sizeof(uint8_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    return 0;
  }

  int write_int32(int32_t data) {
    if (this->size - this->index < sizeof(int32_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    this->buffer[this->index++] = (data >> 8) & 0xff;
    this->buffer[this->index++] = (data >> 16) & 0xff;
    this->buffer[this->index++] = (data >> 24) & 0xff;
    return 0;
  }

  int write_int16(int16_t data) {
    if (this->size - this->index < sizeof(int16_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    this->buffer[this->index++] = (data >> 8) & 0xff;
    return 0;
  }

  int write_int8(int8_t data) {
    if (this->size - this->index < sizeof(int8_t)) {
      return 1;
    }

    this->buffer[this->index++] = data & 0xff;
    return 0;
  }
};

enum MessageType {
  SNAPCAST_MESSAGE_BASE = 0,
  SNAPCAST_MESSAGE_CODEC_HEADER = 1,
  SNAPCAST_MESSAGE_WIRE_CHUNK = 2,
  SNAPCAST_MESSAGE_SERVER_SETTINGS = 3,
  SNAPCAST_MESSAGE_TIME = 4,
  SNAPCAST_MESSAGE_HELLO = 5,
  SNAPCAST_MESSAGE_STREAM_TAGS = 6,
  SNAPCAST_MESSAGE_FIRST = SNAPCAST_MESSAGE_BASE,
  SNAPCAST_MESSAGE_LAST = SNAPCAST_MESSAGE_STREAM_TAGS
};

/// @brief Time as sec & usec
struct tv_t {
  int32_t sec;
  int32_t usec;
};

/// @brief Snapcast Base Message
struct SnapMessageBase {
  uint16_t type;
  uint16_t id;
  uint16_t refersTo;
  tv_t sent;
  tv_t received;
  uint32_t size;

  int serialize(char *data, uint32_t reqSize) {
    SnapWriteBuffer buffer;
    int result = 0;

    buffer.begin(data, reqSize);

    result |= buffer.write_uint16(this->type);
    result |= buffer.write_uint16(this->id);
    result |= buffer.write_uint16(this->refersTo);
    result |= buffer.write_int32(this->sent.sec);
    result |= buffer.write_int32(this->sent.usec);
    result |= buffer.write_int32(this->received.sec);
    result |= buffer.write_int32(this->received.usec);
    result |= buffer.write_uint32(this->size);

    return result;
  }

  int deserialize(const char *data, uint32_t reqSize) {
    SnapReadBuffer buffer;
    int result = 0;

    buffer.begin(data, reqSize);

    result |= buffer.read_uint16(&(this->type));
    result |= buffer.read_uint16(&(this->id));
    result |= buffer.read_uint16(&(this->refersTo));
    result |= buffer.read_int32(&(this->sent.sec));
    result |= buffer.read_int32(&(this->sent.usec));
    result |= buffer.read_int32(&(this->received.sec));
    result |= buffer.read_int32(&(this->received.usec));
    result |= buffer.read_uint32(&(this->size));

    return result;
  }
};


/// @brief Snapcast Hallo Message
struct SnapMessageHallo {
  const char *mac;
  const char *hostname;
  const char *version;
  const char *client_name;
  const char *os;
  const char *arch;
  int instance;
  const char *id;
  int protocol_version;


  char *serialize(size_t *size) {
    int str_length, prefixed_length;

    to_json();
    str_length = strlen(json_result);
    // result must end with closing baces
    if (json_result[str_length - 1] != '}'){
        ESP_LOGE(TAG, "invalid json: %s - MAX_JSON_LEN too small?", json_result);
        return nullptr;
    }

    prefixed_length = str_length + 4;
    // add header information with length
    result[0] = str_length & 0xff;
    result[1] = (str_length >> 8) & 0xff;
    result[2] = (str_length >> 16) & 0xff;
    result[3] = (str_length >> 24) & 0xff;
    *size = prefixed_length;

    return result;
  }

protected:
  char result[MAX_JSON_LEN];
  char* json_result = result + 4;
  const char *TAG = "SnapMessageHallo";
  // Removed dependency to json library because of the overhead: this should be good enough!
  // e.g {"MAC":"A8:48:FA:0B:93:40","HostName":"arduino-snapclient","Version":"0.0.2","ClientName":"libsnapcast","OS":"esp32","Arch":"xtensa","Instance":1,"ID":"A8:48:FA:0B:93:40","SnapStreamProtocolVersion":2}
  void to_json() {
    const char *fmt =
        "{\"MAC\":\"%s\",\"HostName\":\"%s\",\"Version\":\"%s\",\"ClientName\":\"%s\",\"OS\":\"%s\""
        ",\"Arch\":\"%s\",\"Instance\":%d,\"ID\":\"%s\",\"SnapStreamProtocolVersion\":%d}";
    snprintf(json_result, MAX_JSON_LEN - 4, fmt, mac, hostname, version, client_name,
             os, arch, instance, id, protocol_version);
    ESP_LOGD(TAG, "%s", json_result);
  }
};

/// @brief Snapcast Server Settings Message
struct SnapMessageServerSettings {
  int32_t buffer_ms = 0;
  int32_t latency = 0;
  uint32_t volume = 0;
  bool muted = false;

  int deserialize(const char *json) {
    if (json == nullptr)
      return 1;
    ESP_LOGD("SnapMessageServerSettings", "%s", json);

    // Removed dependency to json library: this should be good enough!
    audio_tools::Str json_str(json);
    // parse e.g. {"bufferMs":1000,"latency":0,"muted":false,"volume":57}:
    int idx = json_str.indexOf("bufferMs");
    if (idx > 0) {
      buffer_ms = atoi(json + idx + 10);
      ESP_LOGD(TAG, "bufferMs: %d", buffer_ms);
    }
    idx = json_str.indexOf("latency");
    if (idx > 0) {
      latency = atoi(json + idx + 9);
      ESP_LOGD(TAG, "latency: %d", latency);
    }
    idx = json_str.indexOf("volume");
    if (idx > 0) {
      volume = atoi(json + idx + 8);
      ESP_LOGD(TAG, "volume: %d", volume);
    }
    idx = json_str.indexOf("muted");
    if (idx > 0) {
      // set to true if we find a true string
      muted = json_str.indexOf("true") > idx;
      ESP_LOGD(TAG, "muted: %s", muted ? "true" : "false");
    }

    return 0;
  }
};

/// @brief Snapcast Codec Header Message
struct SnapMessageCodecHeader {
  std::vector<char> v_codec;
  uint32_t size;
  std::vector<char> v_payload;

  char *payload() { return &v_payload[0]; }
  char *codec() { return &v_codec[0]; }

  int deserialize(const char *data, uint32_t reqSize) {
    SnapReadBuffer buffer;
    uint32_t string_size;
    int result = 0;
    
    buffer.begin(data, reqSize);

    result |= buffer.read_uint32(&string_size);
    if (result) {
      // Can\"t allocate the proper size string if we didn\"t read the size, so
      // fail early
      return 1;
    }

    if (v_codec.size() < string_size + 1)
      v_codec.resize(string_size + 1);

    result |= buffer.read(codec(), string_size);
    // Make sure the codec is a proper C string by terminating it with a null
    // character
    this->codec()[string_size] = '\0';

    result |= buffer.read_uint32(&(this->size));
    if (result) {
      // Can't allocate the proper size string if we didn't read the size, so
      // fail early
      return 1;
    }

    if (v_payload.size() < reqSize)
      v_payload.resize(reqSize);

    result |= buffer.read(payload(), this->size);
    return result;
  }
};

/// @brief Snapcast Wire Chunk Message
struct SnapMessageWireChunk {
  tv_t timestamp;
  uint32_t size;
  char *payload = nullptr;

  int deserialize(const char *data, uint32_t reqSize) {
    SnapReadBuffer buffer;
    int result = 0;

    buffer.begin(data, reqSize);

    result |= buffer.read_int32(&(this->timestamp.sec));
    result |= buffer.read_int32(&(this->timestamp.usec));
    result |= buffer.read_uint32(&(this->size));

    // If there\"s been an error already (especially for the size bit) return
    // early
    if (result) {
      return result;
    }

    // avoid malloc we just provide the pointer to the data in the
    // buffer
    if (buffer.size - buffer.index < this->size) {
      return 1;
    }
    this->payload = (char *)&buffer.buffer[buffer.index];
    return result;
  }
};

/// @brief Snapcast Time Message
struct SnapMessageTime {
  tv_t latency;

  int serialize(char *data, uint32_t size) {
    SnapWriteBuffer buffer;
    int result = 0;

    buffer.begin(data, size);

    result |= buffer.write_int32(this->latency.sec);
    result |= buffer.write_int32(this->latency.usec);

    return result;
  }

  int deserialize(const char *data, uint32_t size) {
    SnapReadBuffer buffer;
    int result = 0;

    buffer.begin(data, size);

    result |= buffer.read_int32(&(this->latency.sec));
    result |= buffer.read_int32(&(this->latency.usec));

    return result;
  }
};

}
