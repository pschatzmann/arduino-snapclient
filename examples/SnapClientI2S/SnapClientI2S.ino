#include "AudioTools.h"
#include "SnapClient.h"

I2SStream out;
SnapClient client(out);

void setup() {
  Serial.begin(115200);
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

  // setup I2S to define custom pins
  auto cfg = out.defaultConfig();
  config.pin_bck = 14;
  config.pin_ws = 15;
  config.pin_data = 22;
  out.begin(cfg);

  // start snap client
  client.begin();
}

void loop() {
  delay(100);
}