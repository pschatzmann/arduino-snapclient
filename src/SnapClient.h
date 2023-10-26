#pragma once
/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API
 */

//#include <string.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "AudioTools.h"
#include "config.h"
#include "api/SnapGetHttp.h"
#include "api/SnapOutput.h"
#include "api/Common.h"

/**
 * @brief Snap Client for ESP32 Arduino
 */
class SnapClient {

public:
  SnapClient(AudioStream &stream, AudioDecoder &decoder) {
    output_adapter.setStream(stream);
    SnapOutput &so = SnapOutput::instance();
    so.setOutput(output_adapter);
    so.setDecoder(decoder);
  }

  SnapClient(AudioOutput &output, AudioDecoder &decoder) {
    SnapOutput &so = SnapOutput::instance();
    so.setOutput(output);
    so.setDecoder(decoder);
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
    const char* adr = WiFi.macAddress().c_str();
    SnapGetHttp::instance().setMacAddress(adr);
    ESP_LOGI(TAG, "mac: %s", adr);

#if CONFIG_NVS_FLASH
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
#endif

#if CONFIG_SNAPCLIENT_SNTP_ENABLE
    setupSNTPTime ();
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

    startOutput();
    startGetHttp();
    return true;
  }

  void end() {
    if (http_get_task_handle != nullptr)
      vTaskDelete(http_get_task_handle);
    SnapOutput::instance().end();
  }

  /// provides the actual volume
  float volume() {
    return SnapOutput::instance().volume();
  }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { 
    SnapOutput::instance().setVolumeFactor(fact);
  }

  /// For testing to deactivate the starting of the http task
  void setStartTask(bool flag) { http_task_start = flag; }

  /// For Testing: Used to prevent the starting of the output task
  void setStartOutput(bool start) { output_start = start; }


protected:
  const char *TAG = "SnapClient";
  bool http_task_start = true;
  bool output_start = true;
  bool output_started = false;
  AdapterAudioStreamToAudioOutput output_adapter;
  xTaskHandle http_get_task_handle = nullptr;

  /// start output (for testing)
  void startOutput() {
    ESP_LOGD(TAG, "");
    if (output_started)
      return;
    if (output_start) {
      SnapOutput::instance().begin(48000);
    }
    output_started = true;
  }

  void startGetHttp(){
    if (http_task_start && http_get_task_handle==nullptr) {
      xTaskCreatePinnedToCore(&SnapGetHttp::http_get_task, "HTTP", CONFIG_TASK_STACK_HTTP,
                              NULL, CONFIG_TASK_PRIORITY, &http_get_task_handle, CONFIG_TASK_CORE);
    }

  }

  void setupSNTPTime () {
    ESP_LOGD(TAG, "");
    const char *ntpServer = CONFIG_SNTP_SERVER;
    const long gmtOffset_sec =  1 * 60 * 60;
    const int daylightOffset_sec =  1 * 60 * 60;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      ESP_LOGE(TAG, "Failed to obtain time");
      return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
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
      SnapGetHttp &hpp = SnapGetHttp::instance();
      char str_address[20] = {0};
      sprintf(str_address, "%d.%d.%d.%d", server_ip[0], server_ip[1],
              server_ip[2], server_ip[3]);
      int server_port = MDNS.port(0);

      // update addres information
      hpp.setServerIP(server_ip);
      hpp.setServerPort(server_port);
      ESP_LOGI(TAG, "SNAPCAST ip: %s, port: %d", str_address, server_port);

    } else {
      ESP_LOGE(TAG, "SNAPCAST server not found");
    }

    MDNS.end();
  }

};
