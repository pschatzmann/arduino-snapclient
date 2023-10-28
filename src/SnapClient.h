#pragma once
/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API
 */

//#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include <ESPmDNS.h>
#include <WiFi.h>

#include "AudioTools.h"
#include "api/Common.h"
#include "api/SnapProcessorTasks.h"
#include "api/SnapOutputTasks.h"
#include "config.h"

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

  SnapClient(AudioOutput &output, AudioDecoder &decoder) {
    p_snapprocessor->setOutput(output);
    p_snapprocessor->setDecoder(decoder);
  }

  bool begin(void) {
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGE(TAG, "WiFi not connected");
      return false;
    }
    // use maximum speed
    WiFi.setSleep(false);
    ESP_LOGI(TAG, "Connected to AP");

    // Get MAC address for WiFi station
    const char *adr = strdup(WiFi.macAddress().c_str());
    p_snapprocessor->setMacAddress(adr);
    ESP_LOGI(TAG, "mac: %s", adr);
    checkHeap();

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

#if CONFIG_SNAPCLIENT_SNTP_ENABLE
    setupSNTPTime();
#endif

#if CONFIG_SNAPCLIENT_USE_MDNS
    setupMDNS();
#endif

#if CONFIG_USE_PSRAM
    if (ESP.getPsramSize() > 0) {
      heap_caps_malloc_extmem_enable(CONFIG_PSRAM_LIMIT);
      ESP_LOGD(TAG, "PSRAM for allocations > %d bytes", CONFIG_PSRAM_LIMIT);
    } else {
      ESP_LOGW(TAG, "No PSRAM available or PSRAM not activated");
    }
#endif

    // start tasks
    return p_snapprocessor->begin();
  }

  // ends the processing
  void end(void) { p_snapprocessor->end(); }

  /// provides the actual volume
  float volume(void) { return p_snapprocessor->volume(); }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { p_snapprocessor->setVolumeFactor(fact); }

  /// For testing to deactivate the starting of the http task
  void setStartTask(bool flag) { p_snapprocessor->setStartTask(flag); }

  /// For Testing: Used to prevent the starting of the output task
  void setStartOutput(bool start) { p_snapprocessor->setStartOutput(start); }

  void setSnapProcessor(SnapProcessor &processor){
    p_snapprocessor = &processor;
  }
  void doLoop(){
    p_snapprocessor->doLoop();
  }

protected:
  const char *TAG = "SnapClient";
  AdapterAudioStreamToAudioOutput output_adapter;
  // default setup
  SnapOutputTasks snap_output;
  SnapProcessorTasks default_processor{snap_output};
  SnapProcessor *p_snapprocessor = &default_processor;

  void setupSNTPTime() {
    ESP_LOGD(TAG, "");
    const char *ntpServer = CONFIG_SNTP_SERVER;
    const long gmtOffset_sec = 1 * 60 * 60;
    const int daylightOffset_sec = 1 * 60 * 60;
    for (int retry = 0; retry < 5; retry++) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        ESP_LOGE(TAG, "Failed to obtain time");
        continue;
      }
      checkHeap();
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
      break;
    }
  }

  void setupMDNS() {
    ESP_LOGD(TAG, "");
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
  }
};
