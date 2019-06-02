#include "UDPLogReset.h"
#include "Utils.h"

/******************************************************************************
 * Definitions
 ******************************************************************************/

void UDPLogReset::watchdogReset()
{
	delay(10);
	m_udpSocket.stop();
	wdt_disable();
	wdt_enable(WDTO_2S);
	while(1);
}

/******************************************************************************
 * Constructors / Destructors
 ******************************************************************************/
UDPLogReset::UDPLogReset(int listenport, String logip, int logport)
{
  m_udpPortListen = listenport;
  m_udpPortSend   = logport;
  m_broadcastIp.fromString(logip);
}

/******************************************************************************
 * User API
 ******************************************************************************/

void UDPLogReset::begin()
{
	if(NetEEPROM.netSigIsSet()) {
		Ethernet.begin(NetEEPROM.readMAC(), NetEEPROM.readIP(), NetEEPROM.readGW(),
					   NetEEPROM.readGW(), NetEEPROM.readSN());

		// Configuration UDP
    m_udpSocket.begin(m_udpPortListen);

    char textBuffer[UDP_TX_PACKET_MAX_SIZE];
    sprintf(textBuffer, "Server is at : %d.%d.%d.%d\n",
                        Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
    log(textBuffer);
    sprintf(textBuffer, "Gw at : %d.%d.%d.%d\n",
                        NetEEPROM.readGW()[0], NetEEPROM.readGW()[1],NetEEPROM.readGW()[2], NetEEPROM.readGW()[3]);
    log(textBuffer);
  }
}

void UDPLogReset::check()
{
  // if there's data available, read a packet
  int packetSize = m_udpSocket.parsePacket();
  if (packetSize)
  {
    // buffers for receiving  data
    char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
    // read the packet into packetBufffer
    m_udpSocket.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

    char textBuffer[UDP_TX_PACKET_MAX_SIZE];
    sprintf(textBuffer, "UDPLogReset::check : Recu udp : '%s'\n", packetBuffer);
    log(textBuffer);

    // On compare les 5 premieres caractères pour ne pas se préoccuper du '\n'
    if ( strncmp( textBuffer , "reset", 5 ) == 0 )
    {
      log("UDPLogReset::check : Arduino will be doing a normal reset in 2 seconds\n");
      watchdogReset();
    }
    else if ( strncmp( textBuffer , "reprogram", 5 ) == 0 )
    {
      log("UDPLogReset::check : Arduino will reset for reprogramming in 2 seconds\n");
      NetEEPROM.writeImgBad();
      watchdogReset();
    }
    else
    {
      log("UDPLogReset::check : Wrong command\n");
    }
  }
}

void UDPLogReset::log(const char* s)
{
  utf8ascii(s);

  if( m_udpSocket.beginPacket(m_broadcastIp, m_udpPortSend) )
  {
    m_udpSocket.write(s);
    m_udpSocket.endPacket();
  }

  Serial.print(s);
}
