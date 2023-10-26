#pragma once
/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API
 */

#include "WiFi.h"
#include <string.h>

//#include "esp_event.h"
#include "esp_log.h"
// #include "esp_netif.h"
// #include "esp_system.h"
// #include "esp_wifi.h"
// #include "lwip/dns.h"
// #include "lwip/err.h"
// #include "lwip/netdb.h"
// #include "lwip/sockets.h"
// #include "lwip/sys.h"
//#include "mdns.h"
#include "nvs_flash.h"

#include "AudioTools.h"
#include "config.h"
#include "api/SnapAudioToolsAPI.h"
#include "api/SnapGetHttp.h"
#include "api/SnapOutput.h"
#include "api/common.h"
//#include "lightsnapcast/snapcast.h"
//#include "net_functions/net_functions.h"
#include "opus.h"

class SnapClient;
extern SnapClient *selfSnapClient;

/**
 * @brief Snap Client for ESP32 Arduino
 */
class SnapClient {
  friend bool audio_begin(uint32_t sample_rate, uint8_t bits);
  friend void audio_end();
  friend bool audio_write(const void *src, size_t size, size_t *bytes_written);

public:
  SnapClient(AudioStream &stream) {
    selfSnapClient = this;
    output_adapter.setStream(stream);
    this->out = &output_adapter;
    vol_stream.setTarget(output_adapter);
  }

  SnapClient(AudioOutput &output) {
    selfSnapClient = this;
    this->out = &output;
    vol_stream.setTarget(output);
  }

  bool begin(void) {
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGE(TAG, "WiFi not connected");
      return false;
    }
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

    //net_mdns_register("snapclient");

#if CONFIG_SNAPCLIENT_SNTP_ENABLE
    // set_time_from_sntp();
    setupTime();
#endif

    // allow amplification
    auto vol_cfg = vol_stream.defaultConfig();
    vol_cfg.allow_boost = true;
    vol_cfg.channels = 2;
    vol_cfg.bits_per_sample = 16;
    vol_stream.begin(vol_cfg);

    ctx.flow_queue = xQueueCreate(10, sizeof(uint32_t));
    assert(ctx.flow_queue != NULL);

    if (is_start_http) {
      xTaskCreatePinnedToCore(&http_get_task, "HTTP", CONFIG_TASK_STACK_HTTP,
                              NULL, 5, &http_get_task_handle, 1);
    }
    return true;
  }

  /// Adjust the volume
  void setVolume(float vol) {
    this->vol = vol / 100.0;
    ESP_LOGI(TAG, "Volume: %f", this->vol);
    vol_stream.setVolume(this->vol * vol_factor);
  }

  /// provides the actual volume
  float volume() { return vol; }

  /// mute / unmute
  void setMute(bool mute) {
    is_mute = mute;
    vol_stream.setVolume(mute ? 0 : vol);
    writeSilence();
  }

  /// checks if volume is mute
  bool isMute() { return is_mute; }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { vol_factor = fact; }

  void end() {
    if (http_get_task_handle != nullptr)
      vTaskDelete(http_get_task_handle);
    SnapOutput::instance().end();
    // ws_server_stop();
  }

  /// For testing to deactivate the starting of the http task
  void setStartTask(bool flag) { is_start_http = flag; }

protected:
  const char *TAG = "SnapClient";
  AudioOutput *out = nullptr;
  VolumeStream vol_stream;
  AdapterAudioStreamToAudioOutput output_adapter;
  xTaskHandle http_get_task_handle = nullptr;
  float vol = 1.0;
  float vol_factor = 1.0;
  bool is_mute = false;
  bool is_start_http = true;

  void setupTime() {
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

  static void http_get_task(void *pvParameters) {
    SnapGetHttp::instance().http_get_task(pvParameters);
  }

  void writeSilence() {
    for (int j = 0; j < 50; j++) {
      out->writeSilence(1024);
    }
  }

  bool snap_audio_begin(uint32_t sample_rate, uint8_t bits) {
    ESP_LOGD(TAG, "sample_rate: %d, bits: %d", sample_rate, bits);
    if (out == nullptr)
      return false;
    AudioInfo info;
    info.sample_rate = sample_rate;
    info.bits_per_sample = bits;
    info.channels = 2;

    out->setAudioInfo(info);
    bool result = out->begin();
    ESP_LOGD(TAG, "end");
    return result;
  }

  void snap_audio_end() {
    ESP_LOGD(TAG, "");
    if (out == nullptr)
      return;
    out->end();
  }

  bool snap_audio_write(const void *src, size_t size, size_t *bytes_written) {
    ESP_LOGD(TAG, "");
    if (out == nullptr) {
      *bytes_written = 0;
      return false;
    }
    *bytes_written = vol_stream.write((const uint8_t *)src, size);
    return *bytes_written > 0;
  }
};
