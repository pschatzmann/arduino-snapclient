/**
 * @brief SnapClient with Opus decoder: I2S OUtput to an AudioKit.
 * Arduino only implementation: No dependceny to FreeRTOS!
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioCodecs/CodecOpus.h"
#include "AudioLibs/AudioKit.h"
#include "AudioTools.h"
#include "SnapClient.h"
#include "api/SnapOutputTasks.h"
#include "api/SnapProcessorTasks.h"

AudioKitStream out;
OpusAudioDecoder opus;
SnapClient client(out, opus);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

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

  // start snap client with SnapProcessor usingSnap OutputSimple
  static SnapOutputTasks snap_out;
  static SnapProcessorTasks snap_processor(snap_out);
  client.setSnapProcessor(snap_processor);
  client.begin();
}

void loop() { 
  delay(100)
}