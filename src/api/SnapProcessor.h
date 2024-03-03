#pragma once

#include "SnapCommon.h"
#include "SnapConfig.h"
#include "SnapLogger.h"
#include "SnapOutput.h"
#include "SnapProcessor.h"
#include "SnapProtocol.h"
#include "SnapTime.h"
#include "vector"

namespace snap_arduino {

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

  SnapProcessor() {
    // default setup
    static SnapOutput snap_output;
    server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST);
    p_snap_output = &snap_output;
  }

  virtual bool begin() {
    bool result = false;
    if (output_start) {
      result = audioBegin();
    }

    if (http_task_start) {
      last_time_sync = 0;
      id_counter = 0;
      resizeData();
    }
    header_received = false;

    return result;
  }

  virtual void end() {
    ESP_LOGD(TAG, "end");
    audioEnd();
    // ESP_LOGI(TAG, "... done reading from socket");
    p_client->stop();
    send_receive_buffer.resize(0);
    base_message_serialized.resize(0);
  }

  void setServerIP(IPAddress address) { server_ip = address; }

  void setServerPort(int port) { server_port = port; }

  void setMacAddress(const char *adr) { mac_address = adr; }

  tv_t getLatency() { return time_message.latency; }

  void setStartOutput(bool start) { output_start = start; }

  void setStartTask(bool flag) { http_task_start = flag; }

  /// Call via SnapClient in Arduino Loop!
  virtual void doLoop() { processLoopStep(); }

  /// Defines the output class
  void setOutput(AudioOutput &output) { p_snap_output->setOutput(output); }

  /// Defines the decoder class
  void setDecoder(AudioDecoder &dec) { p_snap_output->setDecoder(dec); }

  /// Provides the volume (in the range of 0.0 to 1.0)
  float volume(void) { return p_snap_output->volume(); }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { p_snap_output->setVolumeFactor(fact); }

  /// Defines the SnapOutput implementation
  void setSnapOutput(SnapOutput &out) { p_snap_output = &out; }

  SnapOutput &snapOutput() { return *p_snap_output; }

  /// Defines an alternative client to the WiFiClient
  void setClient(Client &client) { p_client = &client; }

  void setAudioInfo(AudioInfo info) {
    p_snap_output->setAudioInfo(info);
  }

protected:
  const char *TAG = "SnapProcessor";
  //  WiFiClient default_client;
  Client *p_client = nullptr;
  SnapOutput *p_snap_output = nullptr;
  std::vector<int16_t> audio;
  std::vector<char> send_receive_buffer;
  std::vector<char> base_message_serialized;
  int16_t frame_size = 512;
  uint16_t channels = 2;
  codec_type codec_from_server = NO_CODEC;
  SnapMessageBase base_message;
  SnapMessageTime time_message;
  SnapMessageServerSettings server_settings_message;
  uint32_t client_state_muted = 0;
  char *start = nullptr;
  int size = 0;
  timeval now;
  uint64_t last_time_sync = 0;
  int id_counter = 0;
  IPAddress server_ip;
  int server_port = CONFIG_SNAPCAST_SERVER_PORT;
  const char *mac_address = "00-00-00-00-00";
  bool output_start = false;
  bool http_task_start = true;
  bool header_received = false;
  bool is_time_set = false;
  SnapTime &snap_time = SnapTime::instance();

  bool processLoopStep() {
    if (connectClient()) {
      ESP_LOGI(TAG, "... connected");
    } else {
      delay(10);
      return true;
    }

    now = snap_time.time();

    if (!writeHallo()){
      ESP_LOGI(TAG, "writeHallo");
      return false;
    }

    bool rc = true;
    while (rc) {
      rc = processMessageLoop();
      // some additional processing while we wait for data
      processExt();
      //logHeap();
      checkHeap();
    }

    if (id_counter % 100 == 0) {
      logHeap();
    }
    // For rtos, give audio output some space
    delay(1);
    return true;
  }

  /// additional processing
  virtual void processExt() {
    // For rtos, give audio output some space
    delay(5);
  }

  bool resizeData() {
    audio.resize(frame_size);
    send_receive_buffer.resize(CONFIG_SNAPCAST_BUFF_LEN);
    base_message_serialized.resize(BASE_MESSAGE_SIZE);
    return true;
  }

  bool processMessageLoop() {
    ESP_LOGD(TAG, "processMessageLoop");
    // Wait for data
    if (p_client->available() < BASE_MESSAGE_SIZE) {
      return true;
    }

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
      ESP_LOGD(TAG, "Invalid Message: %u", base_message.type);
    }

    // If it's been a second or longer since our last time message was
    // sent, do so now
    if (!writeTimedMessage())
      return false;

    return true;
  }

  bool connectClient() {
    ESP_LOGD(TAG, "start");
    if (p_client->connected()) return true;
    p_client->setTimeout(CONFIG_CLIENT_TIMEOUT_SEC);
    if (!p_client->connect(server_ip, server_port)) {
      ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
      delay(4000);
      return false;
    }
    return true;
  }

  bool writeHallo() {
    ESP_LOGD(TAG, "start");
    // setup base_message
    base_message.type = SNAPCAST_MESSAGE_HELLO;
    base_message.id = 0x0;
    base_message.refersTo = 0x0;
    base_message.sent =  {(int32_t)now.tv_sec, (int32_t)now.tv_usec};
    base_message.received =  {0x0, 0x0};
    base_message.size = 0x0;

    // setup hello_message
    SnapMessageHallo hello_message;
    hello_message.mac = mac_address;
    hello_message.hostname = CONFIG_SNAPCAST_CLIENT_NAME;
    hello_message.version = "0.0.2";
    hello_message.client_name = "libsnapcast";
    hello_message.os = "arduino";
    hello_message.arch = "xtensa";
    hello_message.instance = 1;
    hello_message.id = mac_address;
    hello_message.protocol_version = 2;

    char *hello_message_serialized =
        hello_message.serialize((size_t *)&(base_message.size));
    if (hello_message_serialized == nullptr) {
      ESP_LOGE(TAG, "Failed to serialize hello message");
      return false;
    }

    int result =
        base_message.serialize(&base_message_serialized[0], BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGE(TAG, "Failed to serialize base message");
      return false;
    }

    p_client->write((const uint8_t *)&base_message_serialized[0],
                    BASE_MESSAGE_SIZE);
    p_client->write((const uint8_t *)hello_message_serialized,
                    base_message.size);

    return true;
  }

  bool readBaseMessage() {
    ESP_LOGD(TAG, "%d", BASE_MESSAGE_SIZE);

    // Read Header Record with size
    size = p_client->readBytes(&send_receive_buffer[0], BASE_MESSAGE_SIZE);
    ESP_LOGD(TAG, "Bytes read: %d", size);

    now = snap_time.time();

    int result = base_message.deserialize(&send_receive_buffer[0], size);
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
    while (p_client->available() < base_message.size)
      delay(10);
    size = p_client->readBytes(&(send_receive_buffer[0]), base_message.size);
    return true;
  }

  bool processMessageCodecHeader() {
    ESP_LOGD(TAG, "start");
    SnapMessageCodecHeader codec_header_message;
    start = &send_receive_buffer[0];
    int result = codec_header_message.deserialize(start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to read codec header: %d", result);
      return false;
    }

    ESP_LOGI(TAG, "Received codec header message");

    size = codec_header_message.size;
    start = codec_header_message.payload();
    if (strcmp(codec_header_message.codec(), "opus") == 0) {
      if (!processMessageCodecHeaderOpus(OPUS))
        return false;
    } else if (strcmp(codec_header_message.codec(), "flac") == 0) {
      if (!processMessageCodecHeaderExt(FLAC))
        return false;
    } else if (strcmp(codec_header_message.codec(), "ogg") == 0) {
      if (!processMessageCodecHeaderExt(OGG))
        return false;
    } else if (strcmp(codec_header_message.codec(), "pcm") == 0) {
      if (!processMessageCodecHeaderWav(PCM))
        return false;
    } else {
      ESP_LOGI(TAG, "Codec : %s not supported", codec_header_message.codec());
      ESP_LOGI(TAG, "Change encoder codec to opus in /etc/snapserver.conf on "
                    "server");
      return false;
    }
    ESP_LOGI(TAG, "Codec : %s , Size: %d ", codec_header_message.codec(), size);
    // codec_header_message.release();

    return true;
  }

  bool processMessageCodecHeaderOpus(codec_type codecType) {
    ESP_LOGD(TAG, "start");
    uint32_t rate;
    memcpy(&rate, start + 4, sizeof(rate));
    uint16_t bits;
    memcpy(&bits, start + 8, sizeof(bits));
    memcpy(&channels, start + 10, sizeof(channels));
    // notify output about format
    AudioInfo info(rate, channels, bits);
    setAudioInfo(info);
    audioBegin();
    codec_from_server = codecType;
    return true;
  }

  bool processMessageCodecHeaderWav(codec_type codecType) {
    ESP_LOGD(TAG, "start");
    codec_from_server = codecType;
    audioBegin();
    // send the wav header to the codec
    p_snap_output->audioWrite((const uint8_t*)start, 44);
    return true;
  }

  bool processMessageCodecHeaderExt(codec_type codecType) {
    ESP_LOGD(TAG, "start");
    codec_from_server = codecType;
    audioBegin();
    // send the data to the codec
    //p_snap_output->audioWrite((const uint8_t*)start, size);
    return true;
  }

  bool processMessageWireChunk() {
    ESP_LOGD(TAG, "start");
    start = &send_receive_buffer[0];
    SnapMessageWireChunk wire_chunk_message;
    memset((void*)&wire_chunk_message, 0, sizeof(wire_chunk_message));
    int result = wire_chunk_message.deserialize(start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to read wire chunk: %d", result);
      return false;
    }
    switch (codec_from_server) {
    case FLAC:
    case OGG:
    case OPUS:
    case PCM:
      wireChunk(wire_chunk_message);
      break;
    case NO_CODEC:
      ESP_LOGE(TAG, "Invalid codec");
      break;
    }
    // wire_chunk_message_free(&wire_chunk_message);
    return true;
  }

  bool wireChunk(SnapMessageWireChunk &wire_chunk_message) {
    ESP_LOGD(TAG, "start");
    SnapAudioHeader header;
    header.size = wire_chunk_message.size;
    
    header.sec = wire_chunk_message.timestamp.sec;
    header.usec = wire_chunk_message.timestamp.usec;
    header.codec = codec_from_server;
    writeAudioInfo(header);

    size_t chunk_res;
    if ((chunk_res = writeAudio((const uint8_t *)wire_chunk_message.payload,
                                wire_chunk_message.size)) !=
        wire_chunk_message.size) {
      ESP_LOGW(TAG, "Error writing audio chunk: %zu -> %zu",(size_t)wire_chunk_message.size, chunk_res);
    }
    return true;
  }

  bool processMessageServerSettings() {
    ESP_LOGD(TAG, "start");
    // The first 4 bytes in the buffer are the size of the string.
    // We don't need this, so we'll shift the entire buffer over 4 bytes
    // and use the extra room to add a null character so we can pares
    // it.
    memmove(start, start + 4, size - 4);
    start[size - 3] = '\0';
    int result = server_settings_message.deserialize(start);
    if (result) {
      ESP_LOGI(TAG, "Failed to read server settings: %d", result);
      return false;
    }
    // log mute state, buffer, latency
    ESP_LOGI(TAG, "Message Buff.ms:%d", server_settings_message.buffer_ms);
    ESP_LOGI(TAG, "Latency:        %d", server_settings_message.latency);
    ESP_LOGI(TAG, "Mute:           %s", server_settings_message.muted ? "true":"false");
    ESP_LOGI(TAG, "Setting volume: %d", server_settings_message.volume);

    // define the start delay from the server settings
    p_snap_output->snapTimeSync().setMessageBufferDelay(
        server_settings_message.buffer_ms + server_settings_message.latency);

    // set volume
    if (header_received) {
      setMute(server_settings_message.muted);
    }
    setVolume(0.01f * server_settings_message.volume);

    if (server_settings_message.muted != client_state_muted) {
      client_state_muted = server_settings_message.muted;
    }
    return true;
  }

  bool processMessageTime() {
    ESP_LOGD(TAG, "start");
    int result = time_message.deserialize(start, size);
    if (result) {
      ESP_LOGI(TAG, "Failed to deserialize time message");
      return false;
    }

    // // Calculate TClienctx.tdif : Trx-Tsend-Tnetdelay/2
    struct timeval ttx, trx;
    ttx.tv_sec = base_message.sent.sec;
    ttx.tv_usec = base_message.sent.usec;
    trx.tv_sec = base_message.received.sec;
    trx.tv_usec = base_message.received.usec;

    // for time management
    snap_time.updateServerTime(trx);
    // for synchronization
    p_snap_output->snapTimeSync().updateServerTime(snap_time.toMillis(trx));

    int64_t time_diff = snap_time.timeDifferenceMs(trx, ttx);
    uint32_t time_diff_int = time_diff;
    assert(time_diff_int == time_diff);
    ESP_LOGD(TAG, "Time Difference to Server: %ld ms", time_diff);
    snap_time.setTimeDifferenceClientServerMs(time_diff);

    return true;
  }

  bool writeTimedMessage() {
    ESP_LOGD(TAG, "start");
    uint32_t time_ms = millis() - last_time_sync;
    if (time_ms >= 1000) {
      last_time_sync = millis();
      if (!writeMessage()) {
        return false;
      }
    }
    return true;
  }

  bool writeMessage() {
    ESP_LOGD(TAG, "start");

    now = snap_time.time();

    base_message.type = SNAPCAST_MESSAGE_TIME;
    base_message.id = id_counter++;
    base_message.refersTo = 0;
    base_message.received.sec = 0;
    base_message.received.usec = 0;
    base_message.sent.sec = now.tv_sec;
    base_message.sent.usec = now.tv_usec;
    base_message.size = TIME_MESSAGE_SIZE;

    int result =
        base_message.serialize(&base_message_serialized[0], BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGE(TAG, "Failed to serialize base message for time");
      return true;
    }

    result = time_message.serialize(&send_receive_buffer[0],
                                    CONFIG_SNAPCAST_BUFF_LEN);
    if (result) {
      ESP_LOGI(TAG, "Failed to serialize time message\r\b");
      return true;
    }
    p_client->write((const uint8_t *)&base_message_serialized[0],
                    BASE_MESSAGE_SIZE);
    p_client->write((const uint8_t *)&send_receive_buffer[0],
                    TIME_MESSAGE_SIZE);
    return true;
  }

  void setMute(bool flag) { p_snap_output->setMute(flag); }

  void setVolume(float vol) { p_snap_output->setVolume(vol); }

  bool audioBegin() { return p_snap_output->begin(); }

  void audioEnd() { p_snap_output->end(); }

  virtual size_t writeAudio(const uint8_t *data, size_t size) {
    return p_snap_output->write(data, size);
  }

  size_t writeAudioInfo(SnapAudioHeader &header) {
    return p_snap_output->writeHeader(header);
  }
};

}