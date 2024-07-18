// We just test the output by providing a hex dump of the received data

#include <Ethernet.h>
#include <SPI.h>
#include "AudioTools.h"
#include "SnapClient.h"

// Example settings for RP2040
#define ETH_MISO  0
#define ETH_MOSI  3
#define ETH_SCLK  2

EthernetClient eth;
HexDumpOutput out;                   // final output
CopyDecoder codec;
SnapClient client(eth, out, codec);
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 177);

void setup() {
  Serial.begin(115200);

  SPI.setRX(ETH_MISO);
  SPI.setTX(ETH_MOSI);
  SPI.setSCK(ETH_SCLK);
  SPI.begin();

  // start the Ethernet connection:
  if (!eth.begin(mac, ip)) {
    Serial.print("Ethernet error");
    while (true);
  }

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  client.setServerIP(IPAddress(192, 168, 1, 38));

  // start snap client
  if (!client.begin()) {
    Serial.print("Connection error");
    while (true);
  }
}

void loop() { client.doLoop(); }