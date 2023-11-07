/**
 * @brief SnapClient with Opus decoder: I2S OUtput on an ESP32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioCodecs/CodecOpus.h"

#define ARDUINO_LOOP_STACK_SIZE (10 * 1024)

OpusAudioDecoder opus;
I2SStream out;
WiFiClient wifi;
SnapClient client(wifi, out, opus);

void setup() {
  Serial.begin(115200);
  // login to wifi
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // print ip address
  Serial.println();
  Serial.println(WiFi.localIP());

  // setup I2S to define custom pins
  auto cfg = out.defaultConfig();
  cfg.pin_bck = 14;
  cfg.pin_ws = 15;
  cfg.pin_data = 22;
  out.begin(cfg);

  // start snap client
  client.begin();
}

void loop() {
  client.doLoop();
}