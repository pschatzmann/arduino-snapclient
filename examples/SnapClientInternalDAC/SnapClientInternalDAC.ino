/**
 * @brief SnapClient with Opus decoder: OUtput to internal DAC on an ESP32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioCodecs/CodecOpus.h"

#define ARDUINO_LOOP_STACK_SIZE (10 * 1024)

OpusAudioDecoder opus;
AnalogAudioStream out;
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

  // start snap client
  client.begin();
}

void loop() {
  client.doLoop();
}