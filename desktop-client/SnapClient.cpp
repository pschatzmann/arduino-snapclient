/**
 * @brief SnapClient with Opus decoder:  Output to PortAudio on the desktop
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioTools/AudioLibs/MiniAudioStream.h"
//#include "AudioTools/AudioCodecs/CodecOpus.h"
//#include "AudioTools/AudioCodecs/CodecFLAC.h"   // https://github.com/pschatzmann/arduino-libflac.git
#include "AudioTools/AudioCodecs/CodecVorbis.h" //https://github.com/pschatzmann/arduino-libvorbis-idec
#include "AudioTools/AudioLibs/StdioStream.h"

//CsvOutput<int16_t> out;
//StdioStream out;
MiniAudioStream out;
//OpusAudioDecoder opus;
VorbisDecoder ogg;
//FLACDecoder flac;
WiFiClient wifi;
SnapClient client(wifi, out, ogg);

void setup() {
  Serial.begin(115200);
  //AudioLogger::instance().begin(Serial, AudioLogger::Info);  
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