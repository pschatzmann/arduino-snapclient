#pragma once
/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API
 */

#include "SnapConfig.h"
#include <WiFi.h>
#include "AudioTools.h"
#include "api/SnapCommon.h"
#include "api/SnapLogger.h"

#if CONFIG_NVS_FLASH
#  include "nvs_flash.h"
#endif
#if CONFIG_SNAPCLIENT_USE_MDNS
#  include <ESPmDNS.h>
#endif

#if CONFIG_USE_RTOS
#  include "api/SnapProcessorTasks.h"
#  include "api/SnapOutputTasks.h"
#else
#  include "api/SnapProcessor.h"
#  include "api/SnapOutputSimple.h"
#endif

/**
 * @brief Snap Client for ESP32 Arduino
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 */
class SnapClient {

public:
  SnapClient(AudioStream &stream, AudioDecoder &decoder) {
    output_adapter.setStream(stream);
    p_snapprocessor->setOutput(output_adapter);
    p_snapprocessor->setDecoder(decoder);
  }

  SnapClient(AudioStream &stream, StreamingDecoder &decoder, int bufferSize = 5*1024) {
    p_decoder_adapter = new DecoderFromStreaming(decoder, bufferSize);
    output_adapter.setStream(stream);
    p_snapprocessor->setOutput(output_adapter);
    p_snapprocessor->setDecoder(*p_decoder_adapter);
  }


  SnapClient(AudioOutput &output, AudioDecoder &decoder) {
    p_snapprocessor->setOutput(output);
    p_snapprocessor->setDecoder(decoder);
  }

  SnapClient(AudioOutput &output, StreamingDecoder &decoder, int bufferSize = 5*1024) {
    p_decoder_adapter = new DecoderFromStreaming(decoder, bufferSize);
    p_snapprocessor->setOutput(output);
    p_snapprocessor->setDecoder(*p_decoder_adapter);
  }

  /// Destructor
  ~SnapClient(){
    if (p_decoder_adapter != nullptr) delete p_decoder_adapter;
    end();
  }

  /// Defines an alternative comminucation client (default is WiFiClient)
  void setClient(Client &client){
    p_snapprocessor->setClient(client);
  }

  /// Starts the processing
  bool begin(void) {
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGE(TAG, "WiFi not connected");
      return false;
    }
    // use maximum speed
    WiFi.setSleep(false);
    ESP_LOGI(TAG, "Connected to AP");

    // Get MAC address for WiFi station
    setupMACAddress();

    setupNVS();

    setupMDNS();

    setupPSRAM();

    SnapTime::instance().setupSNTPTime();

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
  void setSnapProcessor(SnapProcessor &processor){
    p_snapprocessor = &processor;
  }

  /// Call from Arduino Loop (when no tasks are used)
  void doLoop(){
    p_snapprocessor->doLoop();
  }

protected:
  const char *TAG = "SnapClient";
  AdapterAudioStreamToAudioOutput output_adapter;
  DecoderFromStreaming *p_decoder_adapter = nullptr;
  // default setup
#if CONFIG_USE_RTOS
  SnapOutputTasks snap_output;
  SnapProcessorTasks default_processor{snap_output};
#else
  SnapOutputSimple snap_output;
  SnapProcessor default_processor{snap_output};
#endif
  SnapProcessor *p_snapprocessor = &default_processor;


  void setupMDNS() {
#if CONFIG_SNAPCLIENT_USE_MDNS
    ESP_LOGD(TAG, "start");
    if (!MDNS.begin(CONFIG_SNAPCAST_CLIENT_NAME)) {
      LOGE(TAG, "Error starting mDNS");
      return;
    }

    // we just take the first address
    int nrOfServices = MDNS.queryService("snapcast", "tcp");
    if (nrOfServices > 0) {
      IPAddress server_ip = MDNS.IP(0);
      char str_address[20] = {0};
      sprintf(str_address, "%d.%d.%d.%d", server_ip[0], server_ip[1],
              server_ip[2], server_ip[3]);
      int server_port = MDNS.port(0);

      // update addres information
      p_snapprocessor->setServerIP(server_ip);
      p_snapprocessor->setServerPort(server_port);
      ESP_LOGI(TAG, "SNAPCAST ip: %s, port: %d", str_address, server_port);

    } else {
      ESP_LOGE(TAG, "SNAPCAST server not found");
    }

    MDNS.end();
    checkHeap();
 #endif 
  }

  void setupNVS(){
#if CONFIG_NVS_FLASH
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

  void setupPSRAM(){
#if CONFIG_USE_PSRAM
    if (ESP.getPsramSize() > 0) {
      heap_caps_malloc_extmem_enable(CONFIG_PSRAM_LIMIT);
      ESP_LOGD(TAG, "PSRAM for allocations > %d bytes", CONFIG_PSRAM_LIMIT);
    } else {
      ESP_LOGW(TAG, "No PSRAM available or PSRAM not activated");
    }
#endif
  }

  void setupMACAddress(){
#ifdef ESP32
    const char *adr = strdup(WiFi.macAddress().c_str());
    p_snapprocessor->setMacAddress(adr);
    ESP_LOGI(TAG, "mac: %s", adr);
    checkHeap();
#endif
  }

};
