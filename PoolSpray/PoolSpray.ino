//
// Module de gestion de piscine
// Modifié le 14/10/2016 Version 1.2 : suppression synchro NTP affichage T? T= pour connexion MQTT
// Modifié le 12/12/2016 Version 1.3 : suppression du code NTP
// Modifié le 05/03/2017 Version 1.4 : création Utils.h + bascule relais sur appuie bouton ( rester sur DHT 1.2.3, pas 1.3.0(compile pas))
// Modifié le 05/03/2017 Version 1.5 : transmission UDP BROADCAST 2300 du log + commande UDP "reset"
// Modifié le 05/03/2017 Version 1.6 : softwareReboot si ibibahjeedom non trouvé
// Modifié le 05/03/2017 Version 1.7 : mis à jour toutes les 10 secondes au lieu de 2
// Modifié le 30/11/2017 Version 1.8 : ajout bootloader ariadne (https://github.com/loathingKernel/ariadne-bootloader compatible w5500)
//                                     suppression watchdog
//                                     suppression EthernetBonjour
//                                     ajout UDPLogReset ( Ethernet est initialisé par lui avec les infos EEPROM )
// Modifié le 2/06/2019 Version 1.9 : changement bootloader ariadne (http://codebendercc.github.com/Ariadne-Bootloader/ compatible w5100)
//                                    changement display ( ecran oled )
//                                    ajout reponse à la seconde sur ordre MQTT ( forceMQTTUpdate )
/*
Utilisation de la bibliothèque Time version 1.5 dans le dossier: D:\wal\Documents\Arduino\libraries\Time 
Utilisation de la bibliothèque PubSubClient version 2.7 dans le dossier: D:\wal\Documents\Arduino\libraries\PubSubClient 
Utilisation de la bibliothèque OneWire version 2.3.4 dans le dossier: D:\wal\Documents\Arduino\libraries\OneWire 
Utilisation de la bibliothèque DHT_sensor_library version 1.3.4 dans le dossier: D:\wal\Documents\Arduino\libraries\DHT_sensor_library 
Utilisation de la bibliothèque Adafruit_SSD1306 version 1.2.9 dans le dossier: D:\wal\Documents\Arduino\libraries\Adafruit_SSD1306 
Utilisation de la bibliothèque Wire version 1.0 dans le dossier: C:\Program Files (x86)\Arduino\hardware\arduino\avr\libraries\Wire 
Utilisation de la bibliothèque SPI version 1.0 dans le dossier: C:\Program Files (x86)\Arduino\hardware\arduino\avr\libraries\SPI 
Utilisation de la bibliothèque Adafruit_GFX_Library version 1.5.3 dans le dossier: D:\wal\Documents\Arduino\libraries\Adafruit_GFX_Library 
Utilisation de la bibliothèque NetEEPROM version 1.0.0 dans le dossier: D:\wal\Documents\Arduino\libraries\NetEEPROM 
Utilisation de la bibliothèque NewEEPROM prise dans le dossier : D:\wal\Documents\Arduino\libraries\NewEEPROM (legacy)
Utilisation de la bibliothèque Ethernet version 2.0.0 dans le dossier: C:\Program Files (x86)\Arduino\libraries\Ethernet 
Utilisation de la bibliothèque Adafruit_Unified_Sensor version 1.0.3 dans le dossier: D:\wal\Documents\Arduino\libraries\Adafruit_Unified_Sensor 
*/

#include <TimeLib.h>
#include "Utils.h"
#include <avr/wdt.h>
#include <Time.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <avr/pgmspace.h>
#include "UDPLogReset.h"   

const char* version = "1.9";

// variables de travail
char textBuffer[256];
char floatTextBuffer[20];

// -------------------------------------------------------------------------------------
// CONFIGURATION RESEAU
// -------------------------------------------------------------------------------------
// la configuration réseau se trouve dans l'EEPROM deuis la mise en place 
// du bootloader ariadne
// il faut uploader le sketch Example/NetEEPROM (Ariadne Bootloader)/WriteNetWorkSettings
// Et verifier l'écriture avec Example/NetEEPROM (Ariadne Bootloader)/ReadNetworkSettings
// ces valeurs servent pour le BootLoader et pour UDPLogReset
// voici les valeurs mis dans l'EEPROM
// IP  : 192.168.10.250
// mac : { 0xDE, 0xDA, 0xBE, 0xEF, 0xFE, 0xED };
// GW  : gateway(192, 168, 10, 1);
// subnet(255, 255, 255, 0);
// word port = 46970;
// String password = "ibibah";
// int ledpin = 13;

/* Create the reset server. This way your reser server will be at the port you
 * have speciefied in the bootloader for remote uploading. For more information on that
 * look at the "NetEEPROM" library in the "WriteNetworkSettings" sketch.
 * If you want to use your own port, create the object as this
 * "EthernetReset reset(reset_path, port);" where port is a number, i.e. 81
 * Pour effectuer un reset :
 * echo "reset" | nc -q1 -u 192.168.10.250 2301
 * Pour effectuer un reset en invalidant le sketch :
 * echo "reprogram" | nc -q1 -u 192.168.10.250 2301
 * pour voir les log :
 * socat -u udp4-recv:2300,reuseaddr -
*/
UDPLogReset logreset(2301,"192.168.10.255", 2300);

unsigned char wifi1_icon16x16[] =
{
	0b00000000, 0b00000000, //                 
	0b00000111, 0b11100000, //      ######     
	0b00011111, 0b11111000, //    ##########   
	0b00111111, 0b11111100, //   ############  
	0b01110000, 0b00001110, //  ###        ### 
	0b01100111, 0b11100110, //  ##  ######  ## 
	0b00001111, 0b11110000, //     ########    
	0b00011000, 0b00011000, //    ##      ##   
	0b00000011, 0b11000000, //       ####      
	0b00000111, 0b11100000, //      ######     
	0b00000100, 0b00100000, //      #    #     
	0b00000001, 0b10000000, //        ##       
	0b00000001, 0b10000000, //        ##       
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
};

unsigned char cancel_icon16x16[] =
{
	0b01000000, 0b00000001, //  #             # 
	0b00100000, 0b00000010, //   #           #   
	0b00010000, 0b00000100, //    #         #  
	0b00001000, 0b00001000, //     #       # 
	0b00000100, 0b00100000, //      #    # 
	0b00000010, 0b01000000, //       #  #  
	0b00000001, 0b10000000, //        ##   
	0b00000001, 0b10000000, //        ##    
	0b00000010, 0b01000000, //       #  #     
	0b00000100, 0b00100000, //      #    #    
	0b00001000, 0b00010000, //     #      #  
	0b00010000, 0b00001000, //    #        #  
	0b00100000, 0b00000100, //   #          # 
	0b01000000, 0b00000010, //  #            #
	0b10000000, 0b00000001, // #              #
	0b00000000, 0b00000000, //                 
};

unsigned char cancel2_icon16x16[] =
{
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
	0b00111000, 0b00001110, //   ###       ### 
	0b00111100, 0b00011110, //   ####     #### 
	0b00111110, 0b00111110, //   #####   ##### 
	0b00011111, 0b01111100, //    ##### #####  
	0b00001111, 0b11111000, //     #########   
	0b00000111, 0b11110000, //      #######    
	0b00000011, 0b11100000, //       #####     
	0b00000111, 0b11110000, //      #######    
	0b00001111, 0b11111000, //     #########   
	0b00011111, 0b01111100, //    ##### #####  
	0b00111110, 0b00111110, //   #####   ##### 
	0b00111100, 0b00011110, //   ####     #### 
	0b00111000, 0b00001110, //   ###       ### 
	0b00000000, 0b00000000, //                 
};

 unsigned char check_icon16x16[] =
{
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000111, //              ###
	0b00000000, 0b00001111, //             ####
	0b00000000, 0b00011111, //            #####
	0b01110000, 0b00111110, //  ###      ##### 
	0b01111000, 0b01111100, //  ####    #####  
	0b01111100, 0b11111000, //  #####  #####   
	0b00011111, 0b11110000, //    #########    
	0b00001111, 0b11100000, //     #######     
	0b00000111, 0b11000000, //      #####      
	0b00000011, 0b10000000, //       ###       
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000000, //                 
};

// OLED display TWI address
#define OLED_ADDR   0x3C

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------------------------------------------------------------------
// CONFIGURATION MQTT
// -------------------------------------------------------------------------------------

IPAddress remoteJeedomIp(192,168,10,50); 
const unsigned int MQTTBrokerPort = 1883;

void LogConsole(const char* s)
{
  utf8ascii(s);

  logreset.log(s);
}


// -------------------------------------------------------------------------------------
// GESTION MQTT
// -------------------------------------------------------------------------------------
EthernetClient ethClient;
PubSubClient   mqttClient(ethClient);


// Fonction de reconnexion au broker MQTT
// --------------------------------------
boolean ReconnectMQTT()
{
  // Connexion au broker MQTT
  LogConsole("Connexion au broker MQTT\n");
  if ( mqttClient.connect("PoolAndSpray") )
  {
    LogConsole("Connecté au broker MQTT\n");
    
    mqttClient.publish("PoolAndSpray/Message","Connecte au broker MQTT");
    
    // ... and resubscribe
    mqttClient.subscribe("PoolAndSpray/Relais/Activer");
  }
  else
  {
    LogConsole("Erreur de connection au broker MQTT\n");    
  }

  return mqttClient.connected();
}


// -------------------------------------------------------------------------------------
// CAPTEUR TEMPERATURE PISCINE
// -------------------------------------------------------------------------------------

const uint8_t pin_waterTemperature = 31;

OneWire waterTemperatureSensor(pin_waterTemperature);

byte waterTemperatureSensorData[9];
byte waterTemperatureSensorAddress[8];
float waterTemperature = 0.0;
unsigned long lastWaterRequestTime = 0;

// Fonction de mesure de la température de l'eau
// ---------------------------------------------
void UpdateWaterTemperature()
{  
  bool updated = false;

  // reset de la recherche des capteurs
  waterTemperatureSensor.reset_search();

  // Recherche d'un capteur 1-wire
  if ( waterTemperatureSensor.search(waterTemperatureSensorAddress) == false )
  {
    sprintf(textBuffer, "UpdateWaterTemperature::Aucun capteur 1-wire présent sur la broche %d !\n", pin_waterTemperature);
    LogConsole(textBuffer);
  }
  else
  {
    // Test du type de capteur
    // Il est donné par le 1er octet de l'adresse sur 64 bits
    // Un capteur DS18B20 a pour type de capteur la valeur 0x28
    if ( waterTemperatureSensorAddress[0] != 0x28 )
    {
      LogConsole("UpdateWaterTemperature::Le capteur présent n'est pas un capteur de température DS18B20 !\n");
    }
  
    else
    {
      // Test du code CRC
      // Il est donné par le dernier octet de l'adresse 64 bits
      if ( waterTemperatureSensor.crc8(waterTemperatureSensorAddress, 7) != waterTemperatureSensorAddress[7] )
      {
        LogConsole("UpdateWaterTemperature::Le capteur présent n'a pas un code CRC valide !\n");
      }
      else
      {
        unsigned long nowMillisec = millis();
        
        if ( lastWaterRequestTime == 0 )
        {
          // On memorise l'heure de la demande
          lastWaterRequestTime = nowMillisec;

          // On demande au capteur de mémoriser la température et lui laisser 1000 ms pour le faire
          waterTemperatureSensor.reset();
          waterTemperatureSensor.select(waterTemperatureSensorAddress);
          waterTemperatureSensor.write(0x44, 1);

          LogConsole("UpdateWaterTemperature::Envoie d'une demande de calcul au capteur de la température de l'eau!\n");
        }
        else if ( (nowMillisec - lastWaterRequestTime) > 750 )
        {

          // On demande au capteur de nous envoyer la mesure mémorisée
          waterTemperatureSensor.reset();
          waterTemperatureSensor.select(waterTemperatureSensorAddress);
          waterTemperatureSensor.write(0xBE);

          // Le mot reçu du capteur fait 9 octets, on les charge un par un dans le tableau de stockage
          for ( byte i = 0; i < 9; ++i )
          {
            waterTemperatureSensorData[i] = waterTemperatureSensor.read();
          }

          // Puis on converti la valeur reçue en température
          waterTemperature = ( ( ( waterTemperatureSensorData[1] << 8 ) | waterTemperatureSensorData[0] )* 0.0625 );  
 
          // On réinitialise l'heure pour reprovoquer une demande
          lastWaterRequestTime = 0;

          LogConsole("UpdateWaterTemperature::Lecture de la température de l'eau!\n");
        }
        else
        {
          LogConsole("UpdateWaterTemperature::Attente du calcul de la température de l'eau...\n");
        }

        updated = true;        
      }
    }
  }
  
  if ( updated == false )
  {
    if ( waterTemperature != 0 )
    {
      waterTemperature = 0.0;
    }
    else
    {
      waterTemperature = -1.0;
    }
  }

  dtostrf(waterTemperature, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "UpdateWaterTemperature::La température de l'eau est : %s °C\n", floatTextBuffer);
  LogConsole(textBuffer);  
}


// -------------------------------------------------------------------------------------
// CAPTEUR TEMPERATURE / HUMIDITE DU LOCAL
// -------------------------------------------------------------------------------------

const uint8_t pin_roomTemperature = A8;

DHT dht(pin_roomTemperature, DHT21);

float roomTemperature = 0.0;
float roomHumidity    = 0.0;


// Fonction de mesure de la température et de l'humidité du local
// --------------------------------------------------------------
void UpdateRoomTemperatureAndHumidity()
{
  roomTemperature = dht.readTemperature();
  roomHumidity    = dht.readHumidity();

  sprintf(textBuffer, "UpdateRoomTemperatureAndHumidity::La température du local est : %s °C\n", ftoa(floatTextBuffer, roomTemperature, 2));
  LogConsole(textBuffer);
  sprintf(textBuffer, "UpdateRoomTemperatureAndHumidity::L'humidité du local est     : %s %%\n", ftoa(floatTextBuffer, roomHumidity, 2));
  LogConsole(textBuffer);
}


// -------------------------------------------------------------------------------------
// CAPTEUR PH ET ORP
// -------------------------------------------------------------------------------------

const uint8_t pin_pH               = A9;
const uint8_t pin_pHPotentiometer  = A10;

// average the analog reading
const int numReadings = 10;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average
bool setupFlag = false;
float potentiometerValue = 0.0;
float pH  = 0.0;

#define DEBUG_PH_ORP 1


// Fonction de mesure du pH
// ------------------------
void UpdatePH()
{
  if ( setupFlag == false )
  {
    for( int i=0; i<numReadings; i++)
    {
      readings[i]=0;
    }
    setupFlag = true;
  }
  
  // subtract the last reading:
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = analogRead(pin_pH);
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }
  // calculate the average:
  average = total / numReadings;
 

#ifdef DEBUG_PH_ORP
  sprintf(textBuffer, "UpdatePH::analogRead(pin_pH) = %d\n", average);
  LogConsole(textBuffer);
#endif
  
  // CONVERT TO VOLTAGE
  float voltage = (5.0 * average) / 1023; // voltage = 0..5V;  we do the math in millivolts!!

#ifdef DEBUG_PH_ORP
  dtostrf(voltage, 3, 1, floatTextBuffer);
  sprintf(textBuffer, "UpdatePH::voltage = %s\n", floatTextBuffer);
  LogConsole(textBuffer);
#endif
  
  // Valeur du pH
  pH = (0.0178 * voltage * 200.0) - 1.889;

#ifdef DEBUG_PH_ORP
  dtostrf(pH, 3, 1, floatTextBuffer);
  sprintf(textBuffer, "UpdatePH::pH (avant calibration) =  %s\n", floatTextBuffer);
  LogConsole(textBuffer);
#endif
  
  // Lecture de la valeur brute du potentiomètre de calibration sur le port analogique
  potentiometerValue = analogRead(pin_pHPotentiometer);
  
#ifdef DEBUG_PH_ORP
  dtostrf(potentiometerValue, 3, 1, floatTextBuffer);
  sprintf(textBuffer, "UpdatePH::analogRead(pin_pHPotentiometer) = %s\n", floatTextBuffer);
  LogConsole(textBuffer);
#endif

  // On utilise la calibration entre -3.0 et +3.0
  potentiometerValue = (potentiometerValue - 512.0) / 170.0;
  
#ifdef DEBUG_PH_ORP
  dtostrf(potentiometerValue, 3, 1, floatTextBuffer);
  sprintf(textBuffer, "UpdatePH::potentiometerValue = %s\n", floatTextBuffer);
  LogConsole(textBuffer);
#endif

  // Adaptation du pH
  pH = pH + potentiometerValue;
}

// -------------------------------------------------------------------------------------
// NIVEAU DU LIQUIDE
// -------------------------------------------------------------------------------------

#define FLOATLEVELPIN 30
int  liquidLevel = 0;


void UpdateLiquidLevel()
{
  int raw = digitalRead(FLOATLEVELPIN); 

  sprintf(textBuffer, "UpdateLiquidLevel::liquidLevelSensor sensor = %d\n", raw);
  LogConsole(textBuffer);

  liquidLevel = (raw == HIGH)? 0: 1;
}

// -------------------------------------------------------------------------------------
// PRESSION DU FILTRE A SABLE
// -------------------------------------------------------------------------------------

float filterPressure = 0.0;
const uint8_t pin_filterPressure = A11;

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void UpdateFilterPressure()
{
  // make 16 readings and average them (reduces some noise) you can tune the number 16 of course
  int count = 16;
  int raw = 0;
  for (int i=0; i< count; i++) raw += analogRead(pin_filterPressure);  // return 0..1023 
  raw = raw / count;

  sprintf(textBuffer, "UpdateFilterPressure::raw pressure sensor = %d\n" , raw);
  LogConsole(textBuffer);

  // CONVERT TO VOLTAGE
  float voltage = 5.0 * raw / 1024; // voltage = 0..5V;  we do the math in millivolts!!

  dtostrf(voltage, 3, 3, floatTextBuffer);
  sprintf(textBuffer, "UpdateFilterPressure::voltage = %s\n", floatTextBuffer);
  LogConsole(textBuffer);

  // INTERPRET VOLTAGES
  if (voltage < 0.5)
  {
    filterPressure = 0.0;
    dtostrf(filterPressure, 3, 1, floatTextBuffer);
    sprintf(textBuffer, "UpdateFilterPressure::filterPressure ( voltage < 0.5 ) = %s\n", floatTextBuffer);
    LogConsole(textBuffer);
  }
  else if (voltage <= 4.5)  // between 0.5 and 4.5 now...
  {
    filterPressure = mapFloat(voltage, 0.5, 4.5, 0.0, 12.0);    // variation on the Arduino map() function
    dtostrf(filterPressure, 3, 1, floatTextBuffer);
    sprintf(textBuffer, "UpdateFilterPressure::filterPressure = %s\n", floatTextBuffer);
    LogConsole(textBuffer);
 }
  else
  {
    filterPressure = 5.0; // pression hors limite
    dtostrf(filterPressure, 3, 1, floatTextBuffer);
    sprintf(textBuffer, "UpdateFilterPressure::filterPressure ( voltage > 4.5 ) = %s\n", floatTextBuffer);
    LogConsole(textBuffer);
  }

}

// -------------------------------------------------------------------------------------
// GESTION DES RELAIS
// -------------------------------------------------------------------------------------

#define RELAY_1  22
#define RELAY_2  23
#define RELAY_3  24
#define RELAY_4  25
#define RELAY_5  26
#define RELAY_6  27
#define RELAY_7  28
#define RELAY_8  29

void InitializeRelays()
{
  pinMode(RELAY_1, OUTPUT);  
  digitalWrite(RELAY_1, HIGH);
  pinMode(RELAY_2, OUTPUT);  
  digitalWrite(RELAY_2, HIGH);
  pinMode(RELAY_3, OUTPUT);  
  digitalWrite(RELAY_3, HIGH);
  pinMode(RELAY_4, OUTPUT);    
  digitalWrite(RELAY_4, HIGH);
  pinMode(RELAY_5, OUTPUT);  
  digitalWrite(RELAY_5, HIGH);
  pinMode(RELAY_6, OUTPUT);  
  digitalWrite(RELAY_6, HIGH);
  pinMode(RELAY_7, OUTPUT);  
  digitalWrite(RELAY_7, HIGH);
  pinMode(RELAY_8, OUTPUT);    
  digitalWrite(RELAY_8, HIGH);
}


// -------------------------------------------------------------------------------------
// GESTION ECRAN 
// -------------------------------------------------------------------------------------
#define  VALUES_SCREEN  0
#define  INIT_SCREEN  1

int currentScreen = -1;


// Fonction de mise à jour de l'affichage
// --------------------------------------
void Display(int screen)
{
  // Affichage des valeurs
  // ---------------------
  if ( screen == VALUES_SCREEN )
  {
    display.clearDisplay();

    // display a line of text
    // Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc
    display.setTextColor(WHITE);

    // Affichage des valeur
    // waterTemperature
    dtostrf(waterTemperature, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.print(textBuffer);
    display.drawCircle(50, 2, 2, WHITE);

    // filterPressure
    dtostrf(filterPressure, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    display.setCursor(0, 19);
    display.setTextSize(2);
    display.print(textBuffer);
    display.setCursor(50, 20);
    display.setTextSize(1);
    display.print("Bar");

    // pH
    dtostrf(pH, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    display.setCursor(0, 37);
    display.setTextSize(2);
    display.print(textBuffer);
    display.setCursor(50, 46);
    display.setTextSize(1);
    display.print("Ph");
    dtostrf(potentiometerValue, 1, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    display.setCursor(50, 37);
    display.setTextSize(1);
    display.print(textBuffer);

    // roomTemperature
    dtostrf(roomTemperature, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    display.setCursor(0, 57);
    display.setTextSize(1);
    display.print(textBuffer);
    display.drawCircle(27, 55, 1, WHITE);

    // roomHumidity
    dtostrf(roomHumidity, 3, 0, floatTextBuffer);
    sprintf(textBuffer, "%s%%", floatTextBuffer);
    display.setCursor(30, 57);
    display.print(textBuffer);
    display.setTextSize(1);

    // mqttClient.connected
    display.drawBitmap( 110, 0, wifi1_icon16x16, 16, 16, WHITE);
    if ( !mqttClient.connected() )
    {
      display.drawBitmap( 110, 0, cancel_icon16x16, 16, 16, WHITE);
    }
    display.setCursor(110, 20);
    display.setTextSize(1);
    display.print("MQT");


    // niveau liquide
    if ( liquidLevel == 0 )
    {
      display.drawBitmap( 90, 0, cancel2_icon16x16, 16, 16, WHITE);
    }
    else
    {
      display.drawBitmap( 90, 0, check_icon16x16, 16, 16, WHITE);
    }
    
    display.setCursor(90, 20);
    display.setTextSize(1);
    display.print("LVL");
  
    display.display();
  }
  else if ( screen == INIT_SCREEN )
  {
    display.clearDisplay();

    // display a line of text
    // Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc
    display.setTextColor(WHITE);

    strcpy(textBuffer, "Initialization...");
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print(textBuffer);
    display.display();
  }

  currentScreen = screen;
}

// Fonction de mise à jour des Topic MQTT
// --------------------------------------
int forceMQTTUpdate = false;

void UpdateMQTT()
{

  dtostrf(roomTemperature, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Local/Temperature", textBuffer);

  dtostrf(roomHumidity, 3, 0, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Local/Humidite", textBuffer);

  dtostrf(waterTemperature, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Pool/Eau/Temperature", textBuffer);
  
  dtostrf(pH, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Pool/Eau/PH", textBuffer);

  sprintf(textBuffer, "%d", liquidLevel);
  mqttClient.publish("PoolAndSpray/Pool/Filtration/NiveauLiquide", textBuffer);

  mqttClient.publish("PoolAndSpray/Pool/Filtration/Ampere", "0");

  dtostrf(filterPressure, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Pool/FiltreASable/Pression", textBuffer);
  

  int val = digitalRead(RELAY_1);
  mqttClient.publish("PoolAndSpray/Pool/Filtration/Pompe", (val==LOW)?"1":"0");
  val = digitalRead(RELAY_2);
  mqttClient.publish("PoolAndSpray/Pool/ProjecteurLED/Actif", (val==LOW)?"1":"0");
  val = digitalRead(RELAY_3);
  mqttClient.publish("PoolAndSpray/Arrosage/Zone1/Actif", (val==LOW)?"1":"0");
  val = digitalRead(RELAY_4);
  mqttClient.publish("PoolAndSpray/Arrosage/Zone2/Actif", (val==LOW)?"1":"0");
  val = digitalRead(RELAY_5);
  mqttClient.publish("PoolAndSpray/Arrosage/Zone3/Actif", (val==LOW)?"1":"0");
  val = digitalRead(RELAY_6);
  mqttClient.publish("PoolAndSpray/Arrosage/Zone4/Actif", (val==LOW)?"1":"0");

  forceMQTTUpdate = false;
}

// Mise à jour de l'affichage courant
void UpdateCurrentDisplay()
{
  Display(currentScreen);
}

int etatRelay1 = HIGH;
int etatRelay2 = HIGH;
int etatRelay3 = HIGH;
int etatRelay4 = HIGH;
int etatRelay5 = HIGH;
int etatRelay6 = HIGH;
int etatRelay7 = HIGH;
int etatRelay8 = HIGH;

// Callback de traitement des messages MQTT
// ----------------------------------------
void CallbackMQTT(char*        topic,
                  byte*        payload,
                  unsigned int length)
{
  char payloadStr[length+1];
  for (int i=0;i<length;i++) {
    payloadStr[i]=(char)payload[i];
  }
  payloadStr[length]='\0';
  sprintf(textBuffer, "Message arrived [%s]=\"%s\"\n", topic, payloadStr);
  LogConsole(textBuffer);

  // interpretation des commandes
  if ( strcmp(topic , "PoolAndSpray/Relais/Activer") == 0 )
  {
    switch(payloadStr[0])
    {
      case '1': etatRelay1 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '2': etatRelay2 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '3': etatRelay3 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '4': etatRelay4 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '5': etatRelay5 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '6': etatRelay6 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '7': etatRelay7 = (payloadStr[1]=='1')?HIGH:LOW; break;
      case '8': etatRelay8 = (payloadStr[1]=='1')?HIGH:LOW; break;
    }

    forceMQTTUpdate = true;

    sprintf(textBuffer, "Relay %c mis à %c\n", payloadStr[0], payloadStr[1]);
    LogConsole(textBuffer);
  }
  else
  {
    sprintf(textBuffer, "Message %s rejected\n", topic);
    LogConsole(textBuffer);
  }
}

// -------------------------------------------------------------------------------------
// FONCTION D'INITIALISATION
// -------------------------------------------------------------------------------------

void setup()
{
  //Configuration Serie
  Serial.begin(9600);
  
  // Configuration Ethernet
  //Ethernet.begin(mac, ip, gateway, subnet);
  
  // l'objet logreset initialise Ethernet avec les valeur de l'EEPROM
  // current value:
  // IP  : 192.168.10.250
  // mac : { 0xDE, 0xDA, 0xBE, 0xEF, 0xFE, 0xED };
  // GW  : gateway(192, 168, 10, 1);
  // subnet(255, 255, 255, 0);
  // path (password) : ibibah6998 (http://192.168.10.250:8080/ibibah6998/reset|program
   /* For now the Arduino EthShield and the server are being configured using the
   * settings already stored in the EEPROM and are the same with the ones for Ariadne bootloader.
   * This means that you *MUST* have updated the network settings on your Arduino with the
   * "WriteNetworkSettings" sketch.
   * The "begin()" command takes care of everything, from initializing the EthShield to
   * starting the web server for resetting. This is why you should always start it before any other
   * server you might want to have */
  logreset.begin();
  
  dht.begin();
  
  pinMode(FLOATLEVELPIN, INPUT);  

  mqttClient.setCallback(CallbackMQTT);
  mqttClient.setServer(remoteJeedomIp, MQTTBrokerPort);

  // initialize and clear display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);

  // Clear the buffer
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Pool&Spray");
  display.setCursor(10, 40);
  sprintf(textBuffer, "v. %s", version);
  display.print(textBuffer);
  display.display();
  // pour permettre de voir la version
  delay(2000);
  }


// -------------------------------------------------------------------------------------
// BOUCLE PRINCIPALE
// -------------------------------------------------------------------------------------

// Variables utilisées dans la boucle de traitement
unsigned long lastReadingTime = 0;
unsigned long lastMqttTime = 0;
time_t prevDisplay = 0;
unsigned long lastReconnectAttempt = 0;
bool firstLoop = true;

void loop()
{
  if ( firstLoop == true )
  {
    sprintf(textBuffer, "Version : %s\n", version);
    LogConsole("\n[Module_piscine]\n");
    LogConsole(textBuffer);
    LogConsole("\n");
    
    Display(INIT_SCREEN);
  
    InitializeRelays();

    firstLoop = false;

    Display(VALUES_SCREEN);
  }
  else
  {
    time_t        nowSec      = now();
    unsigned long nowMillisec = millis();
  
    // Gestion de la connexion MQTT
    if (mqttClient.loop() == false)
    {
        // client disconnected

      if ( nowMillisec - lastReconnectAttempt > 5000 )
      {
        lastReconnectAttempt = nowMillisec;
        // Attempt to reconnect
        if ( ReconnectMQTT() )
        {
          lastReconnectAttempt = 0;
        }
      }
    }  

    // Etat des relays
    digitalWrite(RELAY_1, etatRelay1);
    digitalWrite(RELAY_2, etatRelay2);
    digitalWrite(RELAY_3, etatRelay3);
    digitalWrite(RELAY_4, etatRelay4);
    digitalWrite(RELAY_5, etatRelay5);
    digitalWrite(RELAY_6, etatRelay6);
    digitalWrite(RELAY_7, etatRelay7);
    digitalWrite(RELAY_8, etatRelay8);
  
    // Toutes les 1 secondes
    if ( nowMillisec - lastReadingTime >= 1000 )
    {
      // Mise à jour de la température de l'eau
      UpdateWaterTemperature();
  
      // Mise à jour de la température et de l'humidité du local
      UpdateRoomTemperatureAndHumidity();
  
      // Mise à jour du pH
      UpdatePH();
    
      // Mise à jour de la pression du filtre à sable
      UpdateFilterPressure();
    
      // Mise à jour du niveau liquide
      UpdateLiquidLevel();
      
      // Affichage des valeurs si on est sur l'écran d'affichage
      UpdateCurrentDisplay();
      
      lastReadingTime = nowMillisec;

      LogConsole("---------End Update------------\n");
    }
    

    // Toutes les 10 secondes
    if ( (nowMillisec - lastMqttTime >= 10000) ||
         (forceMQTTUpdate == true) )
    {  
      // Mis à jour des topic MQTT
      UpdateMQTT();
  
      lastMqttTime = nowMillisec;
    }
  }

   /* After the reset server is running the only thing needed is this command at the start of
   * the "loop()" function in your sketch and it will take care of checking for any reset or
   * reprogram requests */
   logreset.check();
}