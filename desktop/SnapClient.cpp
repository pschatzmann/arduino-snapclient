/**
 * @brief SnapClient with Opus decoder:  Output to PortAudio on the desktop
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioLibs/PortAudioStream.h"
#include "AudioCodecs/CodecOpus.h"

PortAudioStream out;
OpusAudioDecoder opus;
SnapClient client(out, opus);

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