#pragma once

// activate extended functionality

#ifndef CONFIG_USE_PSRAM 
#  define CONFIG_USE_PSRAM true
#endif
#ifndef CONFIG_PSRAM_LIMIT 
#  define CONFIG_PSRAM_LIMIT 512
#endif
#ifndef CONFIG_SNAPCLIENT_SNTP_ENABLE 
#  define CONFIG_SNAPCLIENT_SNTP_ENABLE false
#endif
#ifndef CONFIG_SNAPCLIENT_USE_MDNS 
#  define CONFIG_SNAPCLIENT_USE_MDNS true
#endif
#ifndef CONFIG_NVS_FLASH 
#  define CONFIG_NVS_FLASH false
#endif
#ifndef CONFIG_CHECK_HEAP 
#  define CONFIG_CHECK_HEAP false
#endif

// snapcast parameters; 
#ifndef CONFIG_SNAPCAST_SERVER_HOST 
#  define CONFIG_SNAPCAST_SERVER_HOST "192.168.1.33"
#endif
#ifndef CONFIG_SNAPCAST_SERVER_PORT 
#  define CONFIG_SNAPCAST_SERVER_PORT 1704
#endif
#ifndef CONFIG_SNAPCAST_BUFF_LEN 
#  define CONFIG_SNAPCAST_BUFF_LEN 1024
#endif
#ifndef CONFIG_SNAPCAST_CLIENT_NAME 
#  define CONFIG_SNAPCAST_CLIENT_NAME "arduino-snapclient"
#endif
#ifndef CONFIG_SNTP_SERVER 
#  define CONFIG_SNTP_SERVER "pool.ntp.org"
#endif
#ifndef CONFIG_CLIENT_TIMEOUT_SEC 
#  define CONFIG_CLIENT_TIMEOUT_SEC 5
#endif
#ifndef CONFIG_PROCESSING_TIME_MS 
#  define CONFIG_PROCESSING_TIME_MS 172
#endif
#ifndef CONFIG_STREAMIN_DECODER_BUFFER
#  define CONFIG_STREAMIN_DECODER_BUFFER (12 * 1024)
#endif

// wifi
#ifndef CONFIG_WIFI_SSID
#  define CONFIG_WIFI_SSID "piratnet"
#endif
#ifndef CONFIG_WIFI_PASSWORD
#  define CONFIG_WIFI_PASSWORD "12341234"
#endif

// FreeRTOS - max number of entries in the queue
#ifndef RTOS_MAX_QUEUE_ENTRY_COUNT 
#   define RTOS_MAX_QUEUE_ENTRY_COUNT 200
#endif

#ifndef RTOS_STACK_SIZE 
#   define RTOS_STACK_SIZE 10 * 1024
#endif
