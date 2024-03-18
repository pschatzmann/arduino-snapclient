/**
 * @brief SnapClient where the decoding is done by a VS1053 module.
 * This module supports: Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI / WAV so
 * we can copy the received stream as is to the module.
 * DRAFT: not tested!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioLibs/VS1053Stream.h"


VS1053Stream out; // final output
WiFiClient wifi;
SnapTimeSyncDynamic synch(172, 10); // optional configuratioin
CopyDecoder codec;
SnapClient client(wifi, out, codec);

void setup() {
  Serial.begin(115200);

  // login to wifi -> Define values in SnapConfig.h or replace them here
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // print ip address
  Serial.println();
  Serial.println(WiFi.localIP());

  // use full volume of VS1053
  out.setVolume(1.0);

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  // client.setServerIP(IPAddress(192,168,1,38));

  // start snap client
  client.begin(synch);
}

void loop() {
  client.doLoop();
}