// Example for EthernetClient: We just test the output by reporting the received data bytes

#include <SPI.h>
#include <Ethernet.h>
#include "AudioTools.h"
#include "SnapClient.h"

EthernetClient eth;
MeasuringStream out{10, &Serial};  // log ever 10 writes
CopyDecoder codec;
SnapClient client(eth, out, codec);
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
//IPAddress ip(192, 168, 1, 177);
//IPAddress gateway(192, 168, 1, 1);
//IPAddress dns(8, 8, 8, 8);


void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("setup");

  // example for rp2040
  SPI.setMISO(16);
  SPI.setMOSI(19);
  SPI.setSCK(18);
  SPI.setCS(17);
  SPI.begin(17); // use default pins$

  // start the Ethernet connection:
  Ethernet.init(17);
  Ethernet.begin(mac);
  //Ethernet.begin(mac, ip, dns, gateway);
    
  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true);
  }

  // Check link status
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
  Serial.print("local IP: ");
  Serial.println(Ethernet.localIP());

  // wait for link
  Serial.println("waiting for link...");
  while(Ethernet.linkStatus()!=LinkON){
    delay(10);
  }

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  client.setServerIP(IPAddress(192, 168, 1, 44));

  // start snap client
  if (!client.begin()) {
    Serial.print("Could not connect to snap server");
    while (true);
  }
  
  out.setReportBytes(true);
  Serial.println("setup success");
}

void loop() { client.doLoop(); }