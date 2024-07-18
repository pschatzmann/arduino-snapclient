// Example for EthernetClient: We just test the output by providing a hex dump of the received data

#include <SPI.h>
#include <Ethernet.h>
#include "AudioTools.h"
#include "SnapClient.h"

// #define ETH_MISO  23
// #define ETH_MOSI  19
// #define ETH_SCLK  18
// #define ETH_CS    5

EthernetClient eth;
HexDumpOutput out;  // final output
CopyDecoder codec;
SnapClient client(eth, out, codec);
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 177);

void setup() {
  Serial.begin(115200);

  // The ESP32 supports a flexible definition of the SPI pins
  //SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI, ETH_CS);

  SPI.begin(); // use default pins

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip);
  }  

  // wait for link
  while(Ethernet.linkStatus()!=LinkON){
    delay(10);
  }

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  client.setServerIP(IPAddress(192, 168, 1, 38));

  // start snap client
  if (!client.begin()) {
    Serial.print("Could not connect to snap server");
    while (true);
  }
}

void loop() { client.doLoop(); }