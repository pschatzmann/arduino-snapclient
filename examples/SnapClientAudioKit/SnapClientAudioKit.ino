/**
 * @brief SnapClient with Opus decoder: I2S Output to an ESP32 AudioKit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioLibs/AudioKit.h" 
#include "AudioCodecs/CodecOpus.h" // https://github.com/pschatzmann/arduino-libopus
//#include "AudioCodecs/CodecFLAC.h"   // https://github.com/pschatzmann/arduino-libflac.git
//#include "AudioCodecs/CodecVorbis.h" //https://github.com/pschatzmann/arduino-libvorbis-idec

#define ARDUINO_LOOP_STACK_SIZE (10 * 1024)

AudioKitStream out;
//FLACDecoder flac;
//VorbisDecoder ogg;
//WAVDecoder pcm;
OpusAudioDecoder opus;
WiFiClient wifi;
SnapTimeSyncDynamic synch(172, 10); // optional configuratioin
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

  // use full volume of kit - volume control done by client
  out.setVolume(1.0);

  // start snap client
  client.begin(synch);
}

void loop() {
  client.doLoop();
}