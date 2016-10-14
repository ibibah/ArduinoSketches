/*
  Telnet client

 This sketch connects to a a telnet server (http://www.google.com)
 using an Arduino Wiznet Ethernet shield.  You'll need a telnet server
 to test this with.
 Processing's ChatServer example (part of the network library) works well,
 running on port 10002. It can be found as part of the examples
 in the Processing application, available at
 http://processing.org/

 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13

 created 14 Sep 2010
 modified 9 Apr 2012
 by Tom Igoe

 */

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <SoftwareSerial.h>


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] =
{
  0xED, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 10, 251);
IPAddress broadcastIp(192, 168, 10, 255);
unsigned int localPort = 2200;      // local port to listen on

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char  ReplyBuffer[] = "acknowledged";       // a string to send back

SoftwareSerial teleinfo(2, 3);

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;


void setup()
{

  // start the Ethernet connection:
  Ethernet.begin(mac, ip);
    Udp.begin(localPort);

  // Open serial communications and wait for port to open:
  Serial.begin(1200);
  teleinfo.begin(1200);

  // give the Ethernet shield a second to initialize:
  Serial.println("waiting ethernet shield initialisation...");
  delay(2000);
  Serial.println("OK.");
}

void loop()
{
  // as long as there are bytes in the serial queue,
  // read them and send them out the socket if it's open:
  while (teleinfo.available() > 0)
  {
    char inChar = teleinfo.read() & 0x7F;
    // send a reply, to the IP address and port that sent us the packet we received
      Udp.beginPacket(broadcastIp, localPort);
      Udp.write(inChar);
      Udp.endPacket();
      Serial.print(inChar);
  }

    delay(10);
}
