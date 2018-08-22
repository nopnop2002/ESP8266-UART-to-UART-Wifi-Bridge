/*
 * ESP8266 UART <-> WiFi(UDP) <-> UART Bridge
 * ESP8266 TX of Serial1 is GPIO2 
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>

const char* ssid = "**********";
const char* password = "**********";

WiFiUDP Udp;
unsigned int localUdpPort = 4210;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets

#define bufferSize 8192
uint8_t buf[bufferSize];
uint8_t iofs=0;
IPAddress remoteIp;
unsigned int remotePort;
char myHostName[16] = {0};
char remoteHostName[16] = {0};

void setup()
{
  delay(1000);
  Serial1.begin(115200);
  Serial.begin(115200); // You can change

  sprintf(myHostName, "ESP_%06X", ESP.getChipId());
  Serial1.print("Hostname: ");
  Serial1.println(myHostName);
  WiFi.hostname(myHostName);

  Serial1.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial1.print(".");
  }
  Serial1.println(" connected");

  Udp.begin(localUdpPort);
  Serial1.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  // Start the mDNS responder for esp8266.local
  if (!MDNS.begin(myHostName)) {
    Serial1.println("Error setting up MDNS responder!");
  }
  Serial1.println("mDNS responder started");
  // Announce esp tcp service on port 8080
  MDNS.addService("esp8266_wifi", "udp", localUdpPort);
}


void loop()
{
  static int validRemoteIp = 0;
 
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    Serial1.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    Serial1.printf("UDP packet contents: %s\n", incomingPacket);
    Serial.print(incomingPacket);
  }

  if(Serial.available()) {
   
    while(1) {
      if(Serial.available()) {
        buf[iofs] = (char)Serial.read(); // read char from UART
        if(iofs<bufferSize-1) {
          iofs++;
        }
      } else {
        break;
      }
    }

    if (validRemoteIp == 0) {
      // Send out query for esp tcp services
      Serial1.println("Sending mDNS query");
      int n = MDNS.queryService("esp8266_wifi", "udp");
      Serial1.println("mDNS query done");
      if (n == 0) {
        Serial1.println("no services found");
      } else {
        Serial1.print(n);
        Serial1.println(" service(s) found");
        for (int i = 0; i < n; ++i) {
          // Print details for each service found
          Serial1.print(i + 1);
          Serial1.print(": ");
          Serial1.print(MDNS.hostname(i));
          Serial1.print(" (");
          Serial1.print(MDNS.IP(i));
          Serial1.print(":");
          Serial1.print(MDNS.port(i));
          Serial1.println(")");
    
          MDNS.hostname(i).toCharArray(remoteHostName, sizeof(remoteHostName)); 
          if (strcmp(remoteHostName, myHostName) != 0) {
            remoteIp = MDNS.IP(i);
            remotePort = MDNS.port(i);
            validRemoteIp = 1;
          }
        }
      }
      Serial1.println();
      Serial1.print("remoteIp=");
      Serial1.println(remoteIp);
      Serial1.print("remotePort=");
      Serial1.println(remotePort);
    }
    
    Serial1.print("validRemoteIp=");
    Serial1.println(validRemoteIp);
    if (validRemoteIp) {
      Udp.beginPacket(remoteIp, remotePort); // remote IP and port
      Udp.write(buf, iofs);
      Udp.endPacket();
    }
    iofs = 0;
  }
}