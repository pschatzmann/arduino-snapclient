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
#include "api/common.h"

/**
 * @brief Snap Client for ESP32 Arduino
 */
class SnapClient {

public:
  SnapClient(AudioStream &stream) {
    output_adapter.setStream(stream);
    SnapOutput::instance().setOutput(output_adapter);
  }

  SnapClient(AudioOutput &output) {
    SnapOutput::instance().setOutput(output);
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
    ctx.mac_address = WiFi.macAddress().c_str();
    ESP_LOGI(TAG, "mac: %s", ctx.mac_address);

#if CONFIG_USE_PSRAM
    if (ESP.getPsramSize() > 0) {
      heap_caps_malloc_extmem_enable(CONFIG_PSRAM_LIMIT);
    } else {
      ESP_LOGW(TAG, "No PSRAM available or PSRAM not activated");
    }
#endif

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if CONFIG_SNAPCLIENT_SNTP_ENABLE
    setupSNTPTime ();
#endif

#if CONFIG_SNAPCLIENT_USE_MDNS
    setupMDNS();
#endif

    ctx.flow_queue = xQueueCreate(10, sizeof(uint32_t));
    assert(ctx.flow_queue != NULL);

    if (is_start_http) {
      xTaskCreatePinnedToCore(&SnapGetHttp::http_get_task, "HTTP", CONFIG_TASK_STACK_HTTP,
                              NULL, CONFIG_TASK_PRIORITY, &http_get_task_handle, CONFIG_TASK_CORE);
    }
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
  void setStartTask(bool flag) { is_start_http = flag; }

protected:
  const char *TAG = "SnapClient";
  bool is_start_http = true;
  AdapterAudioStreamToAudioOutput output_adapter;
  xTaskHandle http_get_task_handle = nullptr;

  void setupSNTPTime () {
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
      char str_address[13] = {0};
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
