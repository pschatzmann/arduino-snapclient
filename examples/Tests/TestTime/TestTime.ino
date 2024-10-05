#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioTools/AudioCodecs/CodecOpus.h"

OpusAudioDecoder opus;
NullStream out;
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

  client.setStartTask(false);
  client.begin();
}

void loop() {}