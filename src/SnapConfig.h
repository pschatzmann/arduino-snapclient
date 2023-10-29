#pragma once

// activate extended functionality
#ifndef CONFIG_USE_RTOS
#  define CONFIG_USE_RTOS false
#endif
#ifndef CONFIG_USE_PSRAM 
#  define CONFIG_USE_PSRAM true
#endif
#ifndef CONFIG_PSRAM_LIMIT 
#  define CONFIG_PSRAM_LIMIT 512
#endif
#ifndef CONFIG_SNAPCLIENT_SNTP_ENABLE 
#  define CONFIG_SNAPCLIENT_SNTP_ENABLE true
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

// snapast parameters; 
#ifndef CONFIG_SNAPCAST_SERVER_HOST 
#  define CONFIG_SNAPCAST_SERVER_HOST "192.168.1.38"
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

// wifi
#define CONFIG_WIFI_SSID "piratnet"
#define CONFIG_WIFI_PASSWORD "12341234"

// Only relevant with FreeRTOS implementation
#define CONFIG_TASK_PRIORITY 5
#define CONFIG_TASK_CORE 1
#define CONFIG_TASK_STACK_HTTP (6 * 1024)
#define CONFIG_TASK_STACK_DSP_I2S (6 * 1024)
#define BUFFER_SIZE_PSRAM 32 * 1024
#define BUFFER_SIZE_NO_PSRAM 10 * 1024


