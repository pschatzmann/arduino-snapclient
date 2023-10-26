#pragma once

#include "WiFi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"

// Web socket server
#include "SnapOutput.h"
#include "common.h"
#include "config.h"
#include "lightsnapcast/snapcast.h"
#include "opus.h"


#define BASE_MESSAGE_SIZE 26
#define TIME_MESSAGE_SIZE 8

/**
 * Event handler providing local_http_get_task
 */
class SnapGetHttp {

public:
  static SnapGetHttp &instance() {
    static SnapGetHttp self;
    return self;
  }

  /// For Testing: Used to prevent the starting of the output task
  void setStartOutput(bool start) { output_start = start; }

  /// @brief start output (for testing)
  void startOutput() {
    ESP_LOGD(TAG, "");
    if (output_started)
      return;
    if (output_start) {
      SnapOutput::instance().begin(48000);
    }
    output_started = true;
  }

  void end() {
    ESP_LOGD(TAG, "");
    send_receive_buffer.resize(0);
    base_message_serialized.resize(0);
  }

  void setServerIP(IPAddress address){
    server_ip = address;
  }

  void setServerPort(int port){
    server_port = port;
  }

  /// FreeRTOS Task Handler
  static void http_get_task(void *pvParameters) {
    SnapGetHttp::instance().http_get_local_task(pvParameters);
  }

protected:
  const char *TAG = "SnapGetHttp";
  WiFiClient client;
  IPAddress server_ip;
  int server_port = CONFIG_SNAPCAST_SERVER_PORT;
  OpusDecoder *decoder = nullptr;
  std::vector<int16_t> audio;
  std::vector<char> send_receive_buffer;
  std::vector<char> base_message_serialized;
  int16_t frame_size = 512;
  uint16_t channels = 2;
  codec_type codec_from_server = NO_CODEC;
  char *start = nullptr;
  base_message_t base_message;
  time_message_t time_message;
  int size = 0;
  uint32_t client_state_muted = 0;
  struct timeval now, last_time_sync;
  uint8_t timestamp_size[12];
  float time_diff = 0.0;
  int id_counter = 0;
  bool received_header = false;
  bool output_start = true;
  bool output_started = false;

  SnapGetHttp() { server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST); }

  void http_get_local_task(void *pvParameters) {
    ESP_LOGI(TAG, "http_get_task");

    last_time_sync.tv_sec = 0;
    last_time_sync.tv_usec = 0;
    id_counter = 0;

    resizeData();

    startOutput();

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
        rc = processLoop();
        ESP_LOGD(TAG, "Free Heap: %d / Free Heap PSRAM %d",ESP.getFreeHeap(),ESP.getFreePsram());
      }

      ESP_LOGI(TAG, "... done reading from socket");
      client.stop();

      if(id_counter%100 ==0){
        ESP_LOGI(TAG, "Free Heap: %d / Free Heap PSRAM %d",ESP.getFreeHeap(),ESP.getFreePsram());
      }

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

  bool processLoop() {
    ESP_LOGD(TAG, "");

    if (!readBaseMessage())
      return false;
    
    if (!readData())
      return false;

    switch (base_message.type) {
    case SNAPCAST_MESSAGE_CODEC_HEADER:
      processMessageCodecHeader();
      break;

    case SNAPCAST_MESSAGE_WIRE_CHUNK:
      // chunk_cnt++;
      if (!received_header) {
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
        (char *)ctx.mac_address,
        (char *)CONFIG_SNAPCAST_CLIENT_NAME, // hostname
        (char *)"0.0.2",                     // client version
        (char *)"libsnapcast",               // client name
        (char *)"esp32",                     // os name
        (char *)"xtensa",                    // arch
        1,                                   // instance
        (char *)ctx.mac_address,             // id
        2,                                   // protocol version
    };

    char* hello_message_serialized =
        hello_message_serialize(&hello_message, (size_t *)&(base_message.size));
    if (!hello_message_serialized) {
      ESP_LOGI(TAG, "Failed to serialize hello message\r\b");
      return false;
    }

    int result = base_message_serialize(&base_message, &base_message_serialized[0],
                                        BASE_MESSAGE_SIZE);
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

  bool readBaseMessage(){
    ESP_LOGD(TAG, "%d", BASE_MESSAGE_SIZE);
    // Wait for timeout
    //uint64_t end = millis() + CONFIG_WEBSOCKET_SERVER_TIMEOUT_SEC * 1000;
    while (client.available() < BASE_MESSAGE_SIZE) {
      delay(100);
    }

    // Read Header Record with size
    size = client.readBytes(&send_receive_buffer[0],BASE_MESSAGE_SIZE);
    ESP_LOGI(TAG, "Bytes read: %d", size);

    int result = gettimeofday(&now, NULL);
    if (result) {
      ESP_LOGI(TAG, "Failed to gettimeofday");
      return false;
    }

    result = base_message_deserialize(&base_message, &send_receive_buffer[0], size);
    if (result) {
      ESP_LOGI(TAG, "Failed to read base message: %d", result);
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
    if (base_message.size > send_receive_buffer.size()){
      send_receive_buffer.resize(base_message.size);
    }
    start = &send_receive_buffer[0];
    while(client.available() < base_message.size) delay(100);
    size = client.readBytes(&(send_receive_buffer[0]), base_message.size);
    return true;
  }

  bool processMessageCodecHeader() {
    ESP_LOGD(TAG, "");
    codec_header_message_t codec_header_message;
    received_header = false;

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
    received_header = true;

    return true;
  }

  bool processMessageCodecHeaderOpus() {
    ESP_LOGD(TAG, "");
    uint32_t rate;
    memcpy(&rate, start + 4, sizeof(rate));
    uint16_t bits;
    memcpy(&bits, start + 8, sizeof(bits));
    memcpy(&channels, start + 10, sizeof(channels));
    ESP_LOGI(TAG, "Opus sampleformat: %d:%d:%d", rate, bits, channels);
    int error = 0;
    decoder = opus_decoder_create(rate, channels, &error);
    if (error != 0) {
      ESP_LOGI(TAG, "Failed to init opus coder");
      return false;
    }
    codec_from_server = OPUS;
    ESP_LOGI(TAG, "Initialized opus Decoder: %d", error);
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
      wireChunkOpus(wire_chunk_message);
      break;

    case PCM:
      wireChunkPCM(wire_chunk_message);
      break;
    }
    wire_chunk_message_free(&wire_chunk_message);
    return true;
  }

  bool wireChunkPCM(wire_chunk_message_t &wire_chunk_message) {
    ESP_LOGD(TAG, "");
    memcpy(timestamp_size, &wire_chunk_message.timestamp.sec,
           sizeof(wire_chunk_message.timestamp.sec));
    memcpy(timestamp_size + 4, &wire_chunk_message.timestamp.usec,
           sizeof(wire_chunk_message.timestamp.usec));
    uint32_t chunk_size = size;
    memcpy(timestamp_size + 8, &chunk_size, sizeof(chunk_size));

    // ESP_LOGI(TAG, "Network jitter %d %d",(uint32_t)
    // wire_chunk_message.timestamp.usec/1000,
    //                                          (uint32_t)
    //                                          base_message.sent.usec/1000);

    int chunk_res;
    if ((chunk_res = write_ringbuf(timestamp_size, 3 * sizeof(uint32_t))) !=
        12) {
      ESP_LOGI(TAG, "Error writing timestamp to ring buffer: %d", chunk_res);
    }

    if ((chunk_res = write_ringbuf((const uint8_t *)start, size)) != size) {
      ESP_LOGE(TAG, "Error writing data to ring buffer: %d", chunk_res);
    }
    return true;
  }

  bool wireChunkOpus(wire_chunk_message_t &wire_chunk_message) {
    ESP_LOGD(TAG, "");
    int frame_size_eff = 0;
    while ((frame_size_eff = opus_decode(decoder, (unsigned char *)start, size,
                                     (opus_int16 *)&audio[0], this->frame_size,
                                     0)) == OPUS_BUFFER_TOO_SMALL) {
      frame_size = frame_size * 2;
      audio.resize(frame_size);
      ESP_LOGI(TAG,
               "OPUS encoding buffer too small, resizing to %d "
               "samples per channel",
               frame_size);
    }
    // ESP_LOGI(TAG, "time stamp in :
    // %d",wire_chunk_message.timestamp.sec);
    if (frame_size_eff < 0) {
      ESP_LOGE(TAG, "Decode error : %d ", frame_size);
    } else {
      // pack(&timestamp_size,wire_chunk_message.timestamp,frame_size*2*size(uint16_t))
      memcpy(timestamp_size, &wire_chunk_message.timestamp.sec,
             sizeof(wire_chunk_message.timestamp.sec));
      memcpy(timestamp_size + 4, &wire_chunk_message.timestamp.usec,
             sizeof(wire_chunk_message.timestamp.usec));
      uint32_t chunk_size = frame_size_eff * 2 * sizeof(uint16_t);
      memcpy(timestamp_size + 8, &chunk_size, sizeof(chunk_size));

      // ESP_LOGI(TAG, "Network jitter %d %d",(uint32_t)
      // <wire_chunk_message>.timestamp.usec/1000,
      //                                          (uint32_t)
      //                                          base_message.sent.usec/1000);

      int chunk_res;
      if ((chunk_res = write_ringbuf(timestamp_size, 3 * sizeof(uint32_t))) !=
          12) {
        ESP_LOGI(TAG, "Error writing timestamp to ring buffer: %d", chunk_res);
      }
      if ((chunk_res = write_ringbuf((const uint8_t *)&audio[0], chunk_size)) !=
          chunk_size) {
        ESP_LOGI(TAG, "Error writing data to ring buffer: %d", chunk_res);
      }
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
    ctx.buffer_ms = server_settings_message.buffer_ms;
    ESP_LOGI(TAG, "Buffer length:  %d", server_settings_message.buffer_ms);
    ESP_LOGI(TAG, "Ringbuffer size:%d",
             server_settings_message.buffer_ms * 48 * 4);
    ESP_LOGI(TAG, "Latency:        %d", server_settings_message.latency);
    ESP_LOGI(TAG, "Mute:           %d", server_settings_message.muted);
    ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);

    // set volume
    SnapOutput &out = SnapOutput::instance();
    out.setMute(server_settings_message.muted);
    out.setVolume(server_settings_message.volume);

    if (server_settings_message.muted != client_state_muted) {
      client_state_muted = server_settings_message.muted;
      xQueueSend(ctx.flow_queue, &client_state_muted, 10);
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

    timersub(&trx, &ttx, &ctx.tdif);

    char retbuf[10];
    retbuf[0] = 5;
    retbuf[1] = 5;
    uint32_t usec = ctx.tdif.tv_usec;
    uint32_t uavg = ctx.tavg.tv_usec;

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

    int result = base_message_serialize(&base_message, &base_message_serialized[0],
                                        BASE_MESSAGE_SIZE);
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

  size_t write_ringbuf(const uint8_t *data, size_t size) {
    return SnapOutput::instance().write(data, size);
  }
};
