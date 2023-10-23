#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "SnapClient.h"

// setup I2S not necessary
AudioKitStream out;
SnapClient client(out);

void setup() {
  // login to wifk
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
  delay(100);
}