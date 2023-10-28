#pragma once

#include "WiFi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "vector"

// Web socket server
#include "Common.h"
#include "SnapOutput.h"
#include "SnapProcessor.h"
#include "SnapConfig.h"
#include "lightsnapcast/snapcast.h"
#include "opus.h"

#define BASE_MESSAGE_SIZE 26
#define TIME_MESSAGE_SIZE 8

/**
 * @brief Snap Processor implementation which does not rely on FreeRTOS
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
class SnapProcessor {
public:
  SnapProcessor(SnapOutput &output) {
    server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST);
    p_snap_output = &output;
  }

  virtual bool begin() {
    bool result = false;
    if (output_start) {
      result = audioBegin();
    }

    if (http_task_start) {
      last_time_sync.tv_sec = 0;
      last_time_sync.tv_usec = 0;
      id_counter = 0;
      resizeData();
    }
    return result;
  }

  virtual void end() {
    ESP_LOGD(TAG, "");
    audioEnd();
    send_receive_buffer.resize(0);
    base_message_serialized.resize(0);
  }

  void setServerIP(IPAddress address) { server_ip = address; }

  void setServerPort(int port) { server_port = port; }

  void setMacAddress(const char *adr) { mac_address = adr; }

  tv getLatency() { return time_message.latency; }

  void setStartOutput(bool start) { output_start = start; }
  void setStartTask(bool flag) { http_task_start = flag; }

  virtual void doLoop() { processLoop(); }

  /// Defines the output class
  void setOutput(AudioOutput &output) { p_snap_output->setOutput(output); }

  /// Defines the decoder class
  void setDecoder(AudioDecoder &dec) { p_snap_output->setDecoder(dec); }

  float volume(void) { return p_snap_output->volume(); }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { p_snap_output->setVolumeFactor(fact); }

  void setSnapOutput(SnapOutput &out){
    p_snap_output = &out;
  }

protected:
  const char *TAG = "SnapProcessor";
  WiFiClient client;
  SnapOutput *p_snap_output = nullptr;
  std::vector<int16_t> audio;
  std::vector<char> send_receive_buffer;
  std::vector<char> base_message_serialized;
  int16_t frame_size = 512;
  uint16_t channels = 2;
  codec_type codec_from_server = NO_CODEC;
  char *start = nullptr;
  base_message_t base_message;
  int size = 0;
  uint32_t client_state_muted = 0;
  struct timeval now, last_time_sync;
  uint8_t timestamp_size[12];
  float time_diff = 0.0;
  int id_counter = 0;
  IPAddress server_ip;
  int server_port = CONFIG_SNAPCAST_SERVER_PORT;
  const char *mac_address = nullptr;
  bool output_start = true;
  bool http_task_start = true;
  time_message_t time_message;
  uint32_t buffer_ms =  400;
  struct timeval tdif, tavg;

  void processLoop(void *pvParameters = nullptr) {
    ESP_LOGI(TAG, "processLoop");

    while (true) {
      ESP_LOGD(TAG, "startig new connection");

      if (connectClient()) {
        ESP_LOGI(TAG, "... connected");
      } else {
        delay(10);
        continue;
      }

      int result = gettimeofday(&now, NULL);
      if (result) {
        ESP_LOGI(TAG, "Failed to gettimeofday");
        return;
      }

      if (!writeHallo())
        return;

      bool rc = true;
      while (rc) {
        rc = processMessageLoop();
        p_snap_output->doLoop();
        ESP_LOGD(TAG, "Free Heap: %d / Free Heap PSRAM %d", ESP.getFreeHeap(),
                 ESP.getFreePsram());
        checkHeap();
      }

      ESP_LOGI(TAG, "... done reading from socket");
      client.stop();

      if (id_counter % 100 == 0) {
        ESP_LOGI(TAG, "Free Heap: %d / Free Heap PSRAM %d", ESP.getFreeHeap(),
                 ESP.getFreePsram());
      }
      checkHeap();
    }
  }

  bool resizeData() {
    ESP_LOGD(TAG, "");
    audio.resize(frame_size);
    send_receive_buffer.resize(CONFIG_SNAPCAST_BUFF_LEN);
    base_message_serialized.resize(BASE_MESSAGE_SIZE);
    ESP_LOGD(TAG, "end!");
    return true;
  }

  bool processMessageLoop() {
    ESP_LOGD(TAG, "");
    bool header_received = false;

    if (!readBaseMessage())
      return false;

    if (!readData())
      return false;

    switch (base_message.type) {
    case SNAPCAST_MESSAGE_CODEC_HEADER:
      processMessageCodecHeader();
      header_received = true;
      break;

    case SNAPCAST_MESSAGE_WIRE_CHUNK:
      // chunk_cnt++;
      if (!header_received) {
        return true;
      }
      processMessageWireChunk();
      break;

    case SNAPCAST_MESSAGE_SERVER_SETTINGS:
      processMessageServerSettings();
      break;

    case SNAPCAST_MESSAGE_TIME:
      processMessageTime();
      break;

    default:
      ESP_LOGD(TAG, "Invalid Message:", base_message.type);
    }

    // If it's been a second or longer since our last time message was
    // sent, do so now
    if (!writeTimedMessage())
      return false;

    return true;
  }

  bool connectClient() {
    ESP_LOGD(TAG, "");
    client.setTimeout(CONFIG_CLIENT_TIMEOUT_SEC);

    if (!client.connect(server_ip, server_port)) {
      ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
      vTaskDelay(4000 / portTICK_PERIOD_MS);
      return false;
    }
    return true;
  }

  bool writeHallo() {
    ESP_LOGD(TAG, "");
    base_message_t base_message_init = {
        SNAPCAST_MESSAGE_HELLO,             // type
        0x0,                                // id
        0x0,                                // refersTo
        {(int32_t)now.tv_sec, now.tv_usec}, // sent
        {0x0, 0x0},                         // received
        0x0,                                // size
    };
    base_message = base_message_init;

    hello_message_t hello_message = {
        (char *)mac_address,
        (char *)CONFIG_SNAPCAST_CLIENT_NAME, // hostname
        (char *)"0.0.2",                     // client version
        (char *)"libsnapcast",               // client name
        (char *)"esp32",                     // os name
        (char *)"xtensa",                    // arch
        1,                                   // instance
        (char *)mac_address,                 // id
        2,                                   // protocol version
    };

    char *hello_message_serialized =
        hello_message_serialize(&hello_message, (size_t *)&(base_message.size));
    if (!hello_message_serialized) {
      ESP_LOGI(TAG, "Failed to serialize hello message\r\b");
      return false;
    }

    int result = base_message_serialize(
        &base_message, &base_message_serialized[0], BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGI(TAG, "Failed to serialize base message");
      free(hello_message_serialized);
      return false;
    }

    client.write(&base_message_serialized[0], BASE_MESSAGE_SIZE);
    client.write(hello_message_serialized, base_message.size);

    free(hello_message_serialized);
    return true;
  }

  bool readBaseMessage() {
    ESP_LOGD(TAG, "%d", BASE_MESSAGE_SIZE);
    // Wait for timeout
    // uint64_t end = millis() + CONFIG_WEBSOCKET_SERVER_TIMEOUT_SEC * 1000;
    while (client.available() < BASE_MESSAGE_SIZE) {
      delay(100);
    }

    // Read Header Record with size
    size = client.readBytes(&send_receive_buffer[0], BASE_MESSAGE_SIZE);
    ESP_LOGD(TAG, "Bytes read: %d", size);

    int result = gettimeofday(&now, NULL);
    if (result) {
      ESP_LOGW(TAG, "Failed to gettimeofday");
      return false;
    }

    result =
        base_message_deserialize(&base_message, &send_receive_buffer[0], size);
    if (result) {
      ESP_LOGW(TAG, "Failed to read base message: %d", result);
      return false;
    }

    // ESP_LOGI(TAG,"Rx dif : %d %d", base_message.sent.sec,
    // base_message.sent.usec/1000);
    base_message.received.sec = now.tv_sec;
    base_message.received.usec = now.tv_usec;
    return true;
  }

  bool readData() {
    ESP_LOGD(TAG, "%d", base_message.size);
    if (base_message.size > send_receive_buffer.size()) {
      send_receive_buffer.resize(base_message.size);
    }
    start = &send_receive_buffer[0];
    while (client.available() < base_message.size)
      delay(100);
    size = client.readBytes(&(send_receive_buffer[0]), base_message.size);
    return true;
  }

  bool processMessageCodecHeader() {
    ESP_LOGD(TAG, "");
    codec_header_message_t codec_header_message;

    int result =
        codec_header_message_deserialize(&codec_header_message, start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to read codec header: %d", result);
      return false;
    }

    ESP_LOGI(TAG, "Received codec header message");

    size = codec_header_message.size;
    start = codec_header_message.payload;
    if (strcmp(codec_header_message.codec, "opus") == 0) {
      if (!processMessageCodecHeaderOpus())
        return false;
    } else if (strcmp(codec_header_message.codec, "pcm") == 0) {
      if (!processMessageCodecHeaderPCM())
        return false;
    } else {
      ESP_LOGI(TAG, "Codec : %s not supported", codec_header_message.codec);
      ESP_LOGI(TAG, "Change encoder codec to opus in /etc/snapserver.conf on "
                    "server");
      return false;
    }
    ESP_LOGI(TAG, "Codec : %s , Size: %d ", codec_header_message.codec, size);
    codec_header_message_free(&codec_header_message);

    return true;
  }

  bool processMessageCodecHeaderOpus() {
    ESP_LOGD(TAG, "");
    uint32_t rate;
    memcpy(&rate, start + 4, sizeof(rate));
    uint16_t bits;
    memcpy(&bits, start + 8, sizeof(bits));
    memcpy(&channels, start + 10, sizeof(channels));
    // notify output about format
    audioConfig(rate, channels, bits);
    codec_from_server = OPUS;
    return true;
  }

  bool processMessageCodecHeaderPCM() {
    ESP_LOGD(TAG, "");
    codec_from_server = PCM;
    return true;
  }

  bool processMessageWireChunk() {
    ESP_LOGD(TAG, "");
    wire_chunk_message_t wire_chunk_message;
    int result =
        wire_chunk_message_deserialize(&wire_chunk_message, start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to read wire chunk: %d", result);
      return false;
    }
    size = wire_chunk_message.size;
    start = (wire_chunk_message.payload);
    switch (codec_from_server) {
    case OPUS:
    case PCM:
      wireChunk(wire_chunk_message);
      break;
    }
    wire_chunk_message_free(&wire_chunk_message);
    return true;
  }

  bool wireChunk(wire_chunk_message_t &wire_chunk_message) {
    ESP_LOGD(TAG, "");
    AudioHeader header;
    header.size = size;
    header.sec = wire_chunk_message.timestamp.sec;
    header.usec = wire_chunk_message.timestamp.usec;
    header.codec = codec_from_server;
    writeAudioInfo(header);

    size_t chunk_res;
    if ((chunk_res = writeAudio((const uint8_t *)start, size)) != size) {
      ESP_LOGW(TAG, "Error writing data to ring buffer: %d", chunk_res);
    }
    return true;
  }

  bool processMessageServerSettings() {
    ESP_LOGD(TAG, "");
    server_settings_message_t server_settings_message;

    // The first 4 bytes in the buffer are the size of the string.
    // We don't need this, so we'll shift the entire buffer over 4 bytes
    // and use the extra room to add a null character so cJSON can pares
    // it.
    memmove(start, start + 4, size - 4);
    start[size - 3] = '\0';
    int result =
        server_settings_message_deserialize(&server_settings_message, start);
    if (result) {
      ESP_LOGI(TAG, "Failed to read server settings: %d", result);
      return false;
    }
    // log mute state, buffer, latency
    buffer_ms = server_settings_message.buffer_ms;
    ESP_LOGI(TAG, "Buffer length:  %d", server_settings_message.buffer_ms);
    ESP_LOGI(TAG, "Ringbuffer size:%d",
             server_settings_message.buffer_ms * 48 * 4);
    ESP_LOGI(TAG, "Latency:        %d", server_settings_message.latency);
    ESP_LOGI(TAG, "Mute:           %d", server_settings_message.muted);
    ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);

    // set volume
    setMute(server_settings_message.muted);
    setVolume(server_settings_message.volume);

    if (server_settings_message.muted != client_state_muted) {
      client_state_muted = server_settings_message.muted;
    }
    return true;
  }

  bool processMessageTime() {
    ESP_LOGD(TAG, "");
    int result = time_message_deserialize(&time_message, start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to deserialize time message");
      return false;
    }
    // Calculate TClienctx.tdif : Trx-Tsend-Tnetdelay/2
    struct timeval ttx, trx;
    ttx.tv_sec = base_message.sent.sec;
    ttx.tv_usec = base_message.sent.usec;
    trx.tv_sec = base_message.received.sec;
    trx.tv_usec = base_message.received.usec;

    timersub(&trx, &ttx, &tdif);

    char retbuf[10];
    retbuf[0] = 5;
    retbuf[1] = 5;
    uint32_t usec = tdif.tv_usec;
    uint32_t uavg = tavg.tv_usec;

    retbuf[2] = (usec & 0xff000000) >> 24;
    retbuf[3] = (usec & 0x00ff0000) >> 16;
    retbuf[4] = (usec & 0x0000ff00) >> 8;
    retbuf[5] = (usec & 0x000000ff);
    retbuf[6] = (uavg & 0xff000000) >> 24;
    retbuf[7] = (uavg & 0x00ff0000) >> 16;
    retbuf[8] = (uavg & 0x0000ff00) >> 8;
    retbuf[9] = (uavg & 0x000000ff);
    // ws_server_send_bin_client(0,(char*)retbuf, 10);

    time_diff = time_message.latency.usec / 1000 +
                base_message.received.usec / 1000 -
                base_message.sent.usec / 1000;
    time_diff = (time_diff > 1000) ? time_diff - 1000 : time_diff;
    ESP_LOGI(TAG, "TM loopback latency: %03.1f ms", time_diff);
    return true;
  }

  bool writeTimedMessage() {
    ESP_LOGD(TAG, "");
    int result = gettimeofday(&now, NULL);
    if (result) {
      ESP_LOGI(TAG, "Failed to gettimeofday");
      return false;
    }
    struct timeval tv1;
    timersub(&now, &last_time_sync, &tv1);

    if (tv1.tv_sec >= 1) {
      if (!writeMessage()) {
        return false;
      }
    }
    return true;
  }

  bool writeMessage() {
    ESP_LOGD(TAG, "");
    last_time_sync.tv_sec = now.tv_sec;
    last_time_sync.tv_usec = now.tv_usec;

    base_message.type = SNAPCAST_MESSAGE_TIME;
    base_message.id = id_counter++;
    base_message.refersTo = 0;
    base_message.received.sec = 0;
    base_message.received.usec = 0;
    base_message.sent.sec = now.tv_sec;
    base_message.sent.usec = now.tv_usec;
    base_message.size = TIME_MESSAGE_SIZE;

    int result = base_message_serialize(
        &base_message, &base_message_serialized[0], BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGE(TAG, "Failed to serialize base message for time");
      return true;
    }

    result = time_message_serialize(&time_message, &send_receive_buffer[0],
                                    CONFIG_SNAPCAST_BUFF_LEN);
    if (result) {
      ESP_LOGI(TAG, "Failed to serialize time message\r\b");
      return true;
    }
    // ESP_LOGI(TAG, "---------------------------Write back time message
    // \r\b");
    client.write(&base_message_serialized[0], BASE_MESSAGE_SIZE);
    client.write(&send_receive_buffer[0], TIME_MESSAGE_SIZE);
    return true;
  }

  void setMute(bool flag) { p_snap_output->setMute(flag); }

  void setVolume(float vol) { p_snap_output->setVolume(vol); }

  bool audioBegin() { return p_snap_output->begin(); }

  void audioConfig(int rate, uint8_t channels, uint8_t bits) {
    p_snap_output->audioBegin(rate, channels, bits);
  }

  void audioEnd() { p_snap_output->end(); }

  size_t writeAudio(const uint8_t *data, size_t size) {
    return p_snap_output->write(data, size);
  }

  size_t writeAudioInfo(AudioHeader &header) {
    return p_snap_output->writeHeader(header);
  }



};
