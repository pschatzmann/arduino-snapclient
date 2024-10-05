#include "AudioTools.h"
#include "SnapClient.h"
#include "api/SnapProcessorRTOS.h" // install https://github.com/pschatzmann/arduino-freertos-addons
#include "AudioTools/AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver
#include "AudioTools/AudioCodecs/CodecOpus.h" // https://github.com/pschatzmann/arduino-libopus


AudioBoardStream out(AudioKitEs8388V1);  // or replace with e.g. I2SStream out;
//WAVDecoder pcm;
OpusAudioDecoder opus;
WiFiClient wifi;
SnapProcessorRTOS rtos(1024*8); // define queue with 8 kbytes
SnapTimeSyncDynamic synch(172, 10); // optional configuratioin
SnapClient client(wifi, out, opus);

void setup() {
  Serial.begin(115200);

  // login to wifi -> Define values in SnapConfig.h or replace them here
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // initialize output
  auto cfg = out.defaultConfig();
  //cfg.buffer_size = 512;
  //cfg.buffer_count = 6;
  out.begin(cfg);

  // print ip address
  Serial.println();
  Serial.println(WiFi.localIP());

  // use full volume of kit - volume control done by client
  out.setVolume(1.0);

  // Use FreeRTOS
  client.setSnapProcessor(rtos);

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  //client.setServerIP(IPAddress(192,168,1,33));

  // start snap client
  client.begin(synch);
}


void loop() {
  client.doLoop();
}