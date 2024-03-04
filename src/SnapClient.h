#pragma once
/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API
 */

#include "AudioTools.h"
#include "SnapConfig.h"
#include "api/SnapCommon.h"
#include "api/SnapLogger.h"
//#include <WiFi.h>
#include "Client.h"


#if CONFIG_NVS_FLASH
#include "nvs_flash.h"
#endif
#if CONFIG_SNAPCLIENT_USE_MDNS && defined(ESP32)
#include <ESPmDNS.h>
#endif

#include "api/SnapOutput.h"
#include "api/SnapProcessor.h"

namespace snap_arduino {

/**
 * @brief Snap Client for ESP32 Arduino
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
class SnapClient {

public:
  SnapClient(Client &client, AudioStream &stream, AudioDecoder &decoder) {
    static AdapterAudioStreamToAudioOutput output_adapter;
    output_adapter.setStream(stream);
    p_decoder = &decoder;
    p_output = &output_adapter;
    p_client = &client;
    server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST);
  }

  SnapClient(Client &client, AudioStream &stream, StreamingDecoder &decoder,
             int bufferSize = CONFIG_STREAMIN_DECODER_BUFFER) {
    static DecoderFromStreaming decoder_adapter(decoder, bufferSize);
    static AdapterAudioStreamToAudioOutput output_adapter;
    p_decoder = &decoder_adapter;
    output_adapter.setStream(stream);
    p_output = &output_adapter;
    p_client = &client;
    server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST);
  }

  SnapClient(Client &client, AudioOutput &output, AudioDecoder &decoder) {
    p_decoder = &decoder;
    p_output = &output;
    p_client = &client;
    server_ip.fromString(CONFIG_SNAPCAST_SERVER_HOST);
 }

  SnapClient(Client &client, AudioOutput &output, StreamingDecoder &decoder,
             int bufferSize = CONFIG_STREAMIN_DECODER_BUFFER) {
    p_decoder = new DecoderFromStreaming(decoder, bufferSize);
    p_output = &output;
    p_client = &client;
  }

  /// Destructor
  ~SnapClient() {
    end();
  }

  /// Defines an alternative commnuication client (default is WiFiClient)
  void setClient(Client &client) { p_client = &client; }

  /// @brief Defines the Snapcast Server IP address
  /// @param address 
  void setServerIP(IPAddress ipAddress) { this->server_ip = ipAddress; }

  /// Defines the time synchronization logic
  void setSnapTimeSync(SnapTimeSync &timeSync){
    p_time_sync = &timeSync;
  }

  /// Starts the processing
  bool begin(SnapTimeSync &timeSync) {
    setSnapTimeSync(timeSync);
    return begin();
  }

  /// Starts the processing
  bool begin() {
#if defined(ESP32)
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGE(TAG, "WiFi not connected");
      return false;
    }
    // use maximum speed
    WiFi.setSleep(false);
    ESP_LOGI(TAG, "Connected to AP");
#endif

    // Get MAC address for WiFi station
    setupMACAddress();

    setupNVS();

    setupMDNS();

#if CONFIG_SNAPCLIENT_SNTP_ENABLE
    SnapTime::instance().setupSNTPTime();
#endif

    p_snapprocessor->setServerIP(server_ip);
    p_snapprocessor->setServerPort(server_port);
    p_snapprocessor->setOutput(*p_output);
    p_snapprocessor->snapOutput().setSnapTimeSync(*p_time_sync);
    p_snapprocessor->setDecoder(*p_decoder);
    p_snapprocessor->setClient(*p_client);

    // start tasks
    return p_snapprocessor->begin();
  }

  /// ends the processing and releases the resources
  void end(void) { p_snapprocessor->end(); }

  /// Provides the actual volume (in the range of 0.0 to 1.0)
  float volume(void) { return p_snapprocessor->volume(); }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { p_snapprocessor->setVolumeFactor(fact); }

  /// For testing to deactivate the starting of the http task
  void setStartTask(bool flag) { p_snapprocessor->setStartTask(flag); }

  /// For Testing: Used to prevent the starting of the output task
  void setStartOutput(bool start) { p_snapprocessor->setStartOutput(start); }

  /// Defines an alternative Processor
  void setSnapProcessor(SnapProcessor &processor) {
    p_snapprocessor = &processor;
  }

  /// Defines the Snap output implementation to be used
  void setSnapOutput(SnapOutput &out){
    p_snapprocessor->setSnapOutput(out);
  }

  /// Call from Arduino Loop - to receive and process the audio data
  void doLoop() { p_snapprocessor->doLoop(); }

protected:
  const char *TAG = "SnapClient";
  SnapTime &snap_time = SnapTime::instance();
  SnapProcessor default_processor;
  SnapProcessor *p_snapprocessor = &default_processor;
  AudioOutput *p_output = nullptr;
  AudioDecoder *p_decoder = nullptr;
  Client *p_client = nullptr;
  SnapTimeSync *p_time_sync = nullptr;
  IPAddress server_ip;
  int server_port = CONFIG_SNAPCAST_SERVER_PORT;

  void setupMDNS() {
#if CONFIG_SNAPCLIENT_USE_MDNS && defined(ESP32)
    ESP_LOGD(TAG, "start");
    if (!MDNS.begin(CONFIG_SNAPCAST_CLIENT_NAME)) {
      LOGE(TAG, "Error starting mDNS");
      return;
    }

    // we just take the first address
    int nrOfServices = MDNS.queryService("snapcast", "tcp");
    if (nrOfServices > 0) {
      server_ip = MDNS.IP(0);
      char str_address[20] = {0};
      sprintf(str_address, "%d.%d.%d.%d", server_ip[0], server_ip[1],
              server_ip[2], server_ip[3]);
      server_port = MDNS.port(0);

      ESP_LOGI(TAG, "MDNS: SNAPCAST ip: %s, port: %d", str_address, server_port);

    } else {
      ESP_LOGE(TAG, "SNAPCAST server not found");
    }

    MDNS.end();
    checkHeap();
#endif
  }

  void setupNVS() {
#if CONFIG_NVS_FLASH && defined(ESP32)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    checkHeap();
    ESP_ERROR_CHECK(ret);
#endif
  }


  void setupMACAddress() {
#ifdef ESP32
    const char *adr = strdup(WiFi.macAddress().c_str());
    p_snapprocessor->setMacAddress(adr);
    ESP_LOGI(TAG, "mac: %s", adr);
    checkHeap();
#endif
  }
};

}