/**
 * Snap Client from https://github.com/jorgenkraghjakobsen/snapclient converted
 * to an Arduino Library using the AudioTools as output API 
*/


#include "config.h"
#include "AudioTools.h"
#include "WiFi.h"

#include <string.h>
#include <sys/time.h>

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
#include "components/dsp_processor/include/dsp_processor.h"
#include "components/lightsnapcast/include/snapcast.h"
#include "opus.h"
#include "AudioToolsAPI.h"

extern xTaskHandle t_http_get_task;
extern xQueueHandle flow_queue;
extern xQueueHandle prot_queue;
extern xQueueHandle i2s_queue;

//extern EventGroupHandle_t s_wifi_event_group;
extern uint32_t buffer_ms;
// extern uint8_t muteCH[4];
extern char mac_address[18];
extern struct timeval tdif,tavg;

}

class SnapClient;
extern SnapClient *selfSnapClient;

/**
 * @brief Snap Client for ESP32 Arduino
*/
class SnapClient {
 friend bool audio_begin(uint32_t sample_rate, uint8_t bits );
 friend void audio_end();
 friend bool audio_write(const void *src, size_t size, size_t *bytes_written);
 friend bool audio_write_expand(const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written);

  public: 
    SnapClient(AudioStream &stream){
      selfSnapClient = this;
      output_adapter.setStream(stream);
      this->out = &output_adapter;
      vol_stream.setTarget(output_adapter);
    }

    SnapClient(AudioOutput &output){
      selfSnapClient = this;
      this->out = &output;
      vol_stream.setTarget(output);
    }

    ~SnapClient(){
      if (buff!=nullptr) delete(buff);
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

#ifdef CONFIG_USE_PSRAM
      if (ESP.getPsramSize()>0) heap_caps_malloc_extmem_enable(CONFIG_PSRAM_LIMIT);
#endif

      // allocate buff
      if (buff==nullptr){
        buff = new char[CONFIG_SNAPCAST_BUFF_LEN];
      }
      assert(buff!=nullptr);

      esp_err_t ret = nvs_flash_init();
      if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
          ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
      }
      ESP_ERROR_CHECK(ret);

      // setup dsp
      //dsp_setup_flow(500, 48000);
    
      // Enable websocket server  
      ESP_LOGI(TAG, "Setup ws server");
      websocket_if_start();
    
      net_mdns_register("snapclient");

    #ifdef CONFIG_SNAPCLIENT_SNTP_ENABLE
      set_time_from_sntp();
    #endif
      
      flow_queue = xQueueCreate(10, sizeof(uint32_t));
      assert(flow_queue!=NULL);
      
      xTaskCreatePinnedToCore(&http_get_task, "HTTP", CONFIG_TASK_STACK_HTTP, NULL, 5,
                              &t_http_get_task, 1);
      return true;
    }

    /// Adjust the volume
    void setVolume(float vol){
      this->vol = vol;
      vol_stream.setVolume(vol);
    }

    /// provides the actual volume
    float volume() {
      return vol;
    }

    /// mute / unmute
    void setMute(bool mute){
      is_mute = mute;
      vol_stream.setVolume(mute ? 0 : vol);
    }

    /// checks if volume is mute
    bool isMute(){
      return is_mute;
    }

    void writeSilence(){
      for(int j=0;j<50;j++){
        out->writeSilence(1024);
      }
    }

  protected:
    AudioOutput *out = nullptr;
    VolumeStream vol_stream;
    AdapterAudioStreamToAudioOutput output_adapter;
    float vol = 1.0;
    bool is_mute = false;
    char* buff = nullptr; //[CONFG_SNAPCAST_BUFF_LEN];
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

    bool snap_audio_begin(uint32_t sample_rate, uint8_t bits){
        AudioInfo info;
        info.sample_rate = sample_rate;
        info.bits_per_sample = bits;
        info.channels = 2;
        out->setAudioInfo(info);
        return out->begin();
    }

    void snap_audio_end() { out->end(); }

    bool snap_audio_write(const void *src, size_t size, size_t *bytes_written) {
        *bytes_written = vol_stream.write((const uint8_t*)src, size);
        return *bytes_written > 0;
    }

    bool snap_audio_write_expand(const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written){
        *bytes_written = vol_stream.write((const uint8_t*)src, size);
        return *bytes_written > 0;
    }
      

};
