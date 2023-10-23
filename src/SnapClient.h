/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API 
*/


#include "config.h"
#include "AudioTools.h"
#include "WiFi.h"

#include <string.h>

extern "C" {

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "components/net_functions/include/net_functions.h"

// Web socket server 
#include "components/websocket_if/include/websocket_if.h"
#include "components/websocket/include/websocket_server.h"

// Opus decoder is implemented as a subcomponet from master git repo
#include <sys/time.h>

//#include "driver/i2s.h"
#include "components/dsp_processor/include/dsp_processor.h"
//#include "opus.h"
#include "components/ota_server/include/ota_server.h"
#include "components/lightsnapcast/include/snapcast.h"
#include "opus.h"

extern xTaskHandle t_http_get_task;
extern xQueueHandle flow_queue;
extern xQueueHandle prot_queue;
extern xQueueHandle i2s_queue;

//extern EventGroupHandle_t s_wifi_event_group;
extern uint32_t buffer_ms;
extern uint8_t muteCH[4];
extern char mac_address[18];
extern struct timeval tdif,tavg;

}

class SnapClient;
extern SnapClient *selfSnapClient;

/**
 * @brief Snap Client for ESP32 Arduino
*/
class SnapClient {
  public: 
    SnapClient(AudioStream &output){
      selfSnapClient = this;
      this->out = &output;
    }

    AudioStream &output() {
      return *out;
    }

    bool begin(void) {
      if (WiFi.status() != WL_CONNECTED){
        ESP_LOGE(TAG, "WiFi not connected");
        return false;
      }
      ESP_LOGI(TAG, "Connected to AP");

      // Get MAC address for WiFi station
      strcpy(mac_address, WiFi.macAddress().c_str());
      ESP_LOGI(TAG, "mac: %s", mac_address);


      esp_err_t ret = nvs_flash_init();
      if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
          ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
      }
      ESP_ERROR_CHECK(ret);


      // setup dsp
      dsp_setup_flow(500, 48000);
    
      // Enable websocket server  
      ESP_LOGI(TAG, "Setup ws server");
      websocket_if_start();
    
      net_mdns_register("snapclient");

    #ifdef CONFIG_SNAPCLIENT_SNTP_ENABLE
      set_time_from_sntp();
    #endif
      
      flow_queue = xQueueCreate(10, sizeof(uint32_t));
      assert(flow_queue!=NULL);
      xTaskCreate(&ota_server_task, "ota_server_task", 4096, NULL, 15, NULL);
      
      xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 3 * 4096, NULL, 5,
                              &t_http_get_task, 1);
      return true;
    }

  protected:
    AudioStream *out = nullptr;
    char buff[CONFIG_SNAPCAST_BUFF_LEN];
    unsigned int addr;
    uint32_t port = CONFIG_SNAPCAST_SERVER_PORT;
    const char *TAG = "SNAPCAST";
    enum codec_type { PCM, FLAC, OGG, OPUS };
    uint32_t avg[32];  
    int avgptr = 0; 
    int avgsync = 0; 

    static void http_get_task(void *pvParameters) {
      selfSnapClient->local_http_get_task(pvParameters);
    }

    void local_http_get_task(void *pvParameters);
    void timeravg(struct timeval *tavg,struct timeval *tdif);

};
