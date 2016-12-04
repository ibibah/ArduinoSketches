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
#include <EthernetBonjour.h>    // https://github.com/TrippyLighting/EthernetBonjour

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] =
{
  0xED, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 10, 251);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned int localPort = 2200;      // local port to listen on

IPAddress broadcastIp(192, 168, 10, 255);
IPAddress remoteJeedomIp(0, 0, 0, 0);
// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char  ReplyBuffer[] = "acknowledged";       // a string to send back

SoftwareSerial teleinfo(2, 3);

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

const char* ip_to_str(const uint8_t*);
void nameFound(const char* name, const byte ipAddr[4]);

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(1200);
  teleinfo.begin(1200);
  
  // give the Ethernet shield a second to initialize:
  Serial.println("waiting ethernet shield initialisation...");
  delay(2000);
  Serial.println("OK.");
  
  // start the Ethernet connection:
  Ethernet.begin(mac, ip, gateway, subnet);
  Udp.begin(localPort);

  // Initialize the Bonjour/MDNS library. You can now reach or ping this
  // Arduino via the host name "arduino.local", provided that your operating
  // system is Bonjour-enabled (such as MacOS X).
  // Always call this before any other method!
  EthernetBonjour.begin("ibibahteleinfo");

  // We specify the function that the Bonjour library will call when it
  // resolves a host name. In this case, we will call the function named
  // "nameFound".
  EthernetBonjour.setNameResolvedCallback(nameFound);
}

void loop()
{
  if (remoteJeedomIp == IPAddress(0,0,0,0) )
  {
    if (EthernetBonjour.isResolvingName()== false) 
    {
      Serial.print("Resolving '");
      Serial.print("ibibahjeedom");
      Serial.println("' via Multicast DNS (Bonjour)...");

      // Now we tell the Bonjour library to resolve the host name. We give it a
      // timeout of 5 seconds (e.g. 5000 milliseconds) to find an answer. The
      // library will automatically resend the query every second until it
      // either receives an answer or your timeout is reached - In either case,
      // the callback function you specified in setup() will be called.

      EthernetBonjour.resolveName("ibibahjeedom", 5000);
    }    
  }
  else
  {
    // as long as there are bytes in the serial queue,
    // read them and send them out the socket if it's open:
    while (teleinfo.available() > 0)
    {
      char inChar = teleinfo.read() & 0x7F;
      // send a reply, to the IP address and port that sent us the packet we received
      Udp.beginPacket(remoteJeedomIp, localPort);
      Udp.write(inChar);
      Udp.endPacket();
      Serial.print(inChar);
    }
  }
  
  // This actually runs the Bonjour module. YOU HAVE TO CALL THIS PERIODICALLY,
  // OR NOTHING WILL WORK! Preferably, call it once per loop().
  EthernetBonjour.run();

  delay(1);
}

// This function is called when a name is resolved via MDNS/Bonjour. We set
// this up in the setup() function above. The name you give to this callback
// function does not matter at all, but it must take exactly these arguments
// (a const char*, which is the hostName you wanted resolved, and a const
// byte[4], which contains the IP address of the host on success, or NULL if
// the name resolution timed out).
void nameFound(const char* name, const byte ipAddr[4])
{
  if (NULL != ipAddr) {
    Serial.print("The IP address for '");
    Serial.print(name);
    Serial.print("' is ");
    Serial.println(ip_to_str(ipAddr));
    remoteJeedomIp = IPAddress(ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  } else {
    Serial.print("Resolving '");
    Serial.print(name);
    Serial.println("' timed out.");
  }
}

// This is just a little utility function to format an IP address as a string.
const char* ip_to_str(const uint8_t* ipAddr)
{
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}
