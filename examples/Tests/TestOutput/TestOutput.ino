#include "AudioTools.h"
#include "SnapClient.h"

NullStream out;
SnapClient client(out);

void setup() {
  Serial.begin(115200);

  // login to wifi
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // do not start http task
  client.setStartTask(false);
  client.begin();
  // start output task
  SnapOutput::instance().begin(44100);
}

void loop() {}