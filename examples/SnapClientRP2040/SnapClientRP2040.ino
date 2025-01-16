/**
 * @brief SnapClient with Opus decoder: Wifi processing is on core 0, decoding and
 * i2s output is on core 1.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecOpus.h"
#include "SnapClient.h"
#include "api/SnapProcessorRP2040.h" 

//#define ARDUINO_LOOP_STACK_SIZE (10 * 1024)

I2SStream out;
OpusAudioDecoder opus;
WiFiClient wifi;
//SnapTimeSyncDynamic synch(172, 10); // optional configuratioin
SnapClient client(wifi, out, opus);
SnapProcessorRP2040 processor_rp2040(1024*8); // define OPUS queue with 8 kbytes

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while(!Serial);
  Serial.println("starting...");

  // login to wifi -> Define values in SnapConfig.h or replace them here
  WiFi.begin("ssid", "password");
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // print ip address
  Serial.println();
  Serial.println(WiFi.localIP());

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  client.setServerIP(IPAddress(192,168,1,44));
  client.setSnapProcessor(processor_rp2040);

  // start snap client
  client.begin();
  Serial.println("started");
}

void loop() {
  // fill queue  
  client.doLoop();
}

void setup1(){
  // setup I2S to define custom pins
  auto cfg = out.defaultConfig();
  //cfg.pin_bck = 14;
  //cfg.pin_ws = 15;
  //cfg.pin_data = 22;
  cfg.sample_rate = 48000;
  cfg.buffer_size = 1024;
  cfg.buffer_count = 4;
  out.begin(cfg);
}

void loop1(){
  // trigger output from queue  
  processor_rp2040.doLoop1();
}