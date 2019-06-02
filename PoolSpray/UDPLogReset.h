#ifndef UDPLogReset_h
#define UDPLogReset_h

#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <string.h>
#include <Arduino.h>
#include <NetEEPROM.h>
#include <SPI.h>
//#include <Ethernet2.h>
//#include <EthernetUdp2.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <Ethernet.h>
#include <EthernetUdp.h>   

#define pgm_uchar(name)   static const prog_uchar name[] PROGMEM

/**
  Class UDPLogReset

  Configure Ethernet :
  // utilise la configuration réseau se trouve dans l'EEPROM
  // ces valeurs servent pour le BootLoader et pour UDPLogReset
  // voici les valeurs mis dans l'EEPROM
  // IP  : 192.168.10.250
  // mac : { 0xDE, 0xDA, 0xBE, 0xEF, 0xFE, 0xED };
  // GW  : gateway(192, 168, 10, 1);
  // subnet(255, 255, 255, 0);
  // word port = 46970;

  Ecoute en UDP des commandes de reset sur le port listenPort

  Permet de Logguer en UDP BROADCAST sur le port logport

  Pour effectuer un reset d'une machine distante:
   echo "reset" | nc -q1 -u 192.168.10.250 listenPort
  Pour effectuer un reset en invalidant le sketch :
   echo "reprogram" | nc -q1 -u 192.168.10.250 listenPort
*/
class UDPLogReset
{
    private:

        EthernetUDP  m_udpSocket;                           // An EthernetUDP instance to let us send and receive packets over UDP
        unsigned int m_udpPortListen;                       // local udp port to listen on and to send to
        unsigned int m_udpPortSend;                         // local udp port to listen on and to send to
        IPAddress    m_broadcastIp;                         // ip log broadcast

    void stdResponce(char* msg);
    void watchdogReset();
    void stop(void);

	public:
		UDPLogReset(int listenPort, String logip, int logport);
		//~UDPLogReset();

		void begin(void);
		void check(void);
        void log(const char* s);
};

#endif
