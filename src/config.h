#pragma once

/* snapast parameters; configurable in menuconfig */
#define CONFIG_SNAPCAST_SERVER_HOST "192.168.1.38"
#define CONFIG_SNAPCAST_SERVER_PORT 1704
#define CONFIG_SNAPCAST_BUFF_LEN 256
#define CONFIG_SNAPCAST_CLIENT_NAME "arduino-snapclient"
#define CONFIG_SNTP_SERVER "pool.ntp.org"

// task settings
#define CONFIG_TASK_PRIORITY 5
#define CONFIG_TASK_CORE 1
#define CONFIG_CLIENT_TIMEOUT_SEC 5

// define stack for tasks
#define CONFIG_TASK_STACK_HTTP (15 * 1024)
#define CONFIG_TASK_STACK_DSP_I2S (10 * 1024)

// activate extended functionality
#define CONFIG_USE_PSRAM false
#define CONFIG_PSRAM_LIMIT 512
#define CONFIG_SNAPCLIENT_SNTP_ENABLE false
#define CONFIG_SNAPCLIENT_USE_MDNS false
#define CONFIG_NVS_FLASH false

// buffer size
#define BUFFER_SIZE_PSRAM 32 * 1024
#define BUFFER_SIZE_NO_PSRAM 10 * 1024

// wifi
#define CONFIG_WIFI_SSID "piratnet"
#define CONFIG_WIFI_PASSWORD "12341234"

