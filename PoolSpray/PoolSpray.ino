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

/*
Using library Time at version 1.5 in folder: D:\Arduino\arduino_ide_croquis\libraries\Time 
Using library pubsubclient-2.6 at version 2.6 in folder: D:\Arduino\arduino_ide_croquis\libraries\pubsubclient-2.6 
Using library OneWire at version 2.3.3 in folder: D:\Arduino\arduino_ide_croquis\libraries\OneWire 
Using library DHT_sensor_library at version 1.3.0 in folder: D:\Arduino\arduino_ide_croquis\libraries\DHT_sensor_library 
Using library LiquidCrystal at version 1.0.7 in folder: D:\Arduino\arduino_ide_croquis\libraries\LiquidCrystal 
Using library NewEEPROM in folder: D:\Arduino\arduino_ide_croquis\libraries\NewEEPROM (legacy)
Using library NetEEPROM at version 1.0.0 in folder: D:\Arduino\arduino_ide_croquis\libraries\NetEEPROM 
Using library SPI at version 1.0 in folder: C:\Users\wal\AppData\Local\Arduino15\packages\arduino\hardware\avr\1.6.20\libraries\SPI 
Using library Ethernet2 at version 1.0.4 in folder: D:\Arduino\arduino_ide_croquis\libraries\Ethernet2 
Using library Adafruit_Unified_Sensor at version 1.0.2 in folder: D:\Arduino\arduino_ide_croquis\libraries\Adafruit_Unified_Sensor
*/

#include <TimeLib.h>
#include "Utils.h"
#include <avr/wdt.h>
#include <Time.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include "UDPLogReset.h"   

const char* version = "1.8";

// variables de travail
char textBuffer[256];
char floatTextBuffer[20];

// -------------------------------------------------------------------------------------
// CONFIGURATION RESEAU
// -------------------------------------------------------------------------------------
// la configuration réseau se trouve dans l'EEPROM deuis la mise en place 
// du bootloader ariadne
// ces valeurs servent pour le BootLoader et pour UDPLogReset
// voici les valeurs mis dans l'EEPROM
// IP  : 192.168.10.250
// mac : { 0xDE, 0xDA, 0xBE, 0xEF, 0xFE, 0xED };
// GW  : gateway(192, 168, 10, 1);
// subnet(255, 255, 255, 0);

/* Create the reset server. This way your reser server will be at the port you
 * have speciefied in the bootloader for remote uploading. For more information on that
 * look at the "NetEEPROM" library in the "WriteNetworkSettings" sketch.
 * If you want to use your own port, create the object as this
 * "EthernetReset reset(reset_path, port);" where port is a number, i.e. 81
 * Pour effectuer un reset :
 * echo "reset" | nc -q1 -u 192.168.10.250 2301
 * Pour effectuer un reset en invalidant le sketch :
 * echo "reprogram" | nc -q1 -u 192.168.10.250 2301
*/
UDPLogReset logreset(2301,"192.168.10.255", 2300);
// -------------------------------------------------------------------------------------
// CONFIGURATION MQTT
// -------------------------------------------------------------------------------------

IPAddress remoteJeedomIp(192,168,10,50); 
const unsigned int MQTTBrokerPort = 1883;

void LogConsole(char* s)
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

const uint8_t pin_waterTemperature = 22;

OneWire waterTemperatureSensor(pin_waterTemperature);

byte waterTemperatureSensorData[9];
byte waterTemperatureSensorAddress[8];
bool waterTemperatureSensorOK = false;

float waterTemperature = 0.0;


// Fonction d'initialisation du capteur de température de l'eau
// ------------------------------------------------------------
void InitializeWaterTemperatureSensor()
{
  LogConsole("Détection du capteur de température de l'eau\n");

  // Recherche d'un capteur 1-wire
  int  nbTries = 3;
  bool found   = false;
  while ( ( ( found = waterTemperatureSensor.search(waterTemperatureSensorAddress) ) == false ) &&
          ( nbTries > 0 ) )
  {
    sprintf(textBuffer, "Aucun capteur 1-wire présent sur la broche %d !\n", pin_waterTemperature);
    LogConsole(textBuffer);
    delay(1000);
    --nbTries;
  }

  if ( found == true )
  {
    // Un capteur a été trouvé, affichage de son adresse
    LogConsole("Capteur 1-wire trouvé avec l'adresse 64 bits :\n");
    sprintf(textBuffer, "%02X %02X %02X %02X %02X %02X %02X %02X\n", 
            waterTemperatureSensorAddress[0],
            waterTemperatureSensorAddress[1],
            waterTemperatureSensorAddress[2],
            waterTemperatureSensorAddress[3],
            waterTemperatureSensorAddress[4],
            waterTemperatureSensorAddress[5],
            waterTemperatureSensorAddress[6],
            waterTemperatureSensorAddress[7]);
    LogConsole(textBuffer);
  
    // Test du type de capteur
    // Il est donné par le 1er octet de l'adresse sur 64 bits
    // Un capteur DS18B20 a pour type de capteur la valeur 0x28
    if ( waterTemperatureSensorAddress[0] != 0x28 )
    {
      LogConsole("Le capteur présent n'est pas un capteur de température DS18B20 !\n");
    }
  
    else
    {
      LogConsole("Type du capteur 1-wire présent : capteur de température DS18B20\n");
  
      // Test du code CRC
      // Il est donné par le dernier octet de l'adresse 64 bits
      if ( waterTemperatureSensor.crc8(waterTemperatureSensorAddress, 7) != waterTemperatureSensorAddress[7] )
      {
        LogConsole("Le capteur présent n'a pas un code CRC valide !\n");
      }
  
      else
      {
        LogConsole("Le capteur présent a un code CRC valide\n");
        LogConsole("Détection du capteur de température de l'eau terminée\n");
  
        waterTemperatureSensorOK = true;
      }
    }
  }
  else
  {
    LogConsole("Aucun capteur 1-wire trouvé !\n");
  }
}


// Fonction de mesure de la température de l'eau
// ---------------------------------------------
void UpdateWaterTemperature()
{
  if ( waterTemperatureSensorOK == true )
  {

    // On demande au capteur de mémoriser la température et lui laisser 800 ms pour le faire
    waterTemperatureSensor.reset();
    waterTemperatureSensor.select(waterTemperatureSensorAddress);
    waterTemperatureSensor.write(0x44, 1);
    delay(800);

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

    dtostrf(waterTemperature, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "La température de l'eau est : %s °C\n", floatTextBuffer);
    LogConsole(textBuffer);
  }
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

  sprintf(textBuffer, "La température du local est : %s °C\n", ftoa(floatTextBuffer, roomTemperature, 2));
  LogConsole(textBuffer);
  sprintf(textBuffer, "L'humidité du local est     : %s %%\n", ftoa(floatTextBuffer, roomHumidity, 2));
  LogConsole(textBuffer);
}


// -------------------------------------------------------------------------------------
// CAPTEUR PH ET ORP
// -------------------------------------------------------------------------------------

const uint8_t pin_pH               = A9;
const uint8_t pin_pHPotentiometer  = A10;
//const uint8_t pin_ORP              = A10;
//const uint8_t pin_ORPPotentiometer = A11;

// average the analog reading
const int numReadings = 10;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average
bool setupFlag = false;

float pH  = 0.0;
//float ORP = 0.0;

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
  sprintf(textBuffer, "analogRead(pin_pH) = %d\n", average);
  LogConsole(textBuffer);
#endif
  
  // CONVERT TO VOLTAGE
  float voltage = (5.0 * average) / 1023; // voltage = 0..5V;  we do the math in millivolts!!

#ifdef DEBUG_PH_ORP
  sprintf(textBuffer, "voltage = %f\n", voltage);
  LogConsole(textBuffer);
#endif
  
  // Valeur du pH
  pH = (0.0178 * voltage * 200.0) - 1.889;

#ifdef DEBUG_PH_ORP
  sprintf(textBuffer, "pH (avant calibration) =  %f\n", pH);
  LogConsole(textBuffer);
#endif
  
  // Lecture de la valeur brute du potentiomètre de calibration sur le port analogique
  float tempPotentiometerValue = analogRead(pin_pHPotentiometer);
  
#ifdef DEBUG_PH_ORP
  sprintf(textBuffer, "analogRead(pin_pHPotentiometer) = %f\n", tempPotentiometerValue);
  LogConsole(textBuffer);
#endif

  // On utilise la calibration entre -1.0 et +1.0
  tempPotentiometerValue = (tempPotentiometerValue - 512.0) / 500.0;
  
#ifdef DEBUG_PH_ORP
  sprintf(textBuffer, "tempPotentiometerValue = %f\n", tempPotentiometerValue);
  LogConsole(textBuffer);
#endif

  // Adaptation du pH
  pH = pH + tempPotentiometerValue;
}


//// Fonction de mesure de l'ORP (potentiel d'oxydo réduction)
//// ---------------------------------------------------------
//void UpdateORP()
//{
//  // Lecture de la valeur brute de l'ORP sur le port analogique
//  float tempValue = analogRead(pin_ORP);
//#ifdef DEBUG_PH_ORP
//  Serial.print("analogRead(pin_ORP) = ");
//  Serial.println(tempValue);
//#endif
//  // Valeur entre 0.0 et 5.0 V
//  tempValue = tempValue * 5000.0 / 1023.0 / 1000.0;
//#ifdef DEBUG_PH_ORP
//  Serial.print("tempValue = ");
//  Serial.println(tempValue);
//#endif
//  // Valeur de l'ORP (entre -2000 et 2000 mV)
//  ORP = ((2.5 - tempValue) / 1.037) * 1000.0;
//#ifdef DEBUG_PH_ORP
//  Serial.print("ORP (avant calibration) = ");
//  Serial.println(ORP);
//#endif
//
//  // Lecture de la valeur brute du potentiomètre de calibration sur le port analogique
//  float tempPotentiometerValue = analogRead(pin_ORPPotentiometer);
//#ifdef DEBUG_PH_ORP
//  Serial.print("analogRead(pin_ORPPotentiometer) = ");
//  Serial.println(tempPotentiometerValue);
//#endif
//  // On utilise la calibration entre -1.0 et +1.0
//  tempPotentiometerValue = (tempPotentiometerValue - 512.0) / 500.0;
//#ifdef DEBUG_PH_ORP
//  Serial.print("tempPotentiometerValue = ");
//  Serial.println(tempPotentiometerValue);
//#endif
//  // Adaptation de l'ORP
////  ORP = ORP + tempPotentiometerValue;
//}

// -------------------------------------------------------------------------------------
// NIVEAU DU LIQUIDE
// -------------------------------------------------------------------------------------

#define FLOATLEVELPIN  3
int  liquidLevel = 0;


void UpdateLiquidLevel()
{
  int raw = digitalRead(FLOATLEVELPIN); 

  sprintf(textBuffer, "liquidLevelSensor sensor = %d\n", raw);
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

  sprintf(textBuffer, "raw pressure sensor = %d\n" , raw);
  LogConsole(textBuffer);

  // CONVERT TO VOLTAGE
  float voltage = 5.0 * raw / 1023; // voltage = 0..5V;  we do the math in millivolts!!

  // INTERPRET VOLTAGES
  if (voltage < 0.5)
  {
    filterPressure = 0.0;
  }
  else if (voltage < 4.5)  // between 0.5 and 4.5 now...
  {
    filterPressure = mapFloat(voltage, 0.5, 4.5, 0.0, 12.0);    // variation on the Arduino map() function
    
    sprintf(textBuffer, "filterPressure = %d, %f\n" , millis(), filterPressure);
    LogConsole(textBuffer);
  }
  else
  {
    filterPressure = 5.0; // pression hors limite
  }
}


// -------------------------------------------------------------------------------------
// CAPTEUR DE PLUIE
// -------------------------------------------------------------------------------------

const int rainSensorMinValue = 0;
const int rainSensorMaxValue = 1024;
const uint8_t pin_rainDropSensor = A12;
int rainSensorValue = 0;


void UpdateRainDropDetection()
{
  int rainSensorReading = analogRead(pin_rainDropSensor);

  sprintf(textBuffer, "rainSensorReading = %d\n" , rainSensorReading);
  LogConsole(textBuffer);
  
  rainSensorValue = map( rainSensorReading, rainSensorMinValue, rainSensorMaxValue, 0, 3);

  sprintf(textBuffer, "rainSensorValue = %d\n" , rainSensorValue);
  LogConsole(textBuffer);
}

// -------------------------------------------------------------------------------------
// CAPTEURS DE CONSOMMATION ELECTRIQUE
// -------------------------------------------------------------------------------------



// -------------------------------------------------------------------------------------
// GESTION DES RELAIS
// -------------------------------------------------------------------------------------

#define RELAY_1  14
#define RELAY_2  15
#define RELAY_3  16
#define RELAY_4  17
#define RELAY_5  18
#define RELAY_6  19
#define RELAY_7  20
#define RELAY_8  21

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
// GESTION ECRAN / CLAVIER
// -------------------------------------------------------------------------------------

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define  VALUES_SCREEN  0

int currentScreen = -1;


// Fonction de mise à jour de l'affichage
// --------------------------------------
void Display(int screen)
{
  bool cleared = false;
  
  // Si on demande un affichage différent
  if ( currentScreen != screen )
  {
    lcd.clear();
    cleared = true;
  }

  // Affichage des valeurs
  // ---------------------
  if ( screen == VALUES_SCREEN )
  {
    // S'il a été effacé
    if ( cleared == true )
    {
      // On remet le masque de fond
      lcd.setCursor(0, 0);
      lcd.print("E     A     H  %");
      lcd.setCursor(0, 1);
      lcd.print("pH    P    R   ");
    }

    // Affichage des valeur
    lcd.setCursor(1, 0);
    dtostrf(waterTemperature, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    lcd.print(textBuffer);

    lcd.setCursor(7, 0);
    dtostrf(roomTemperature, 4, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    lcd.print(textBuffer);
    
    lcd.setCursor(13, 0);
    dtostrf(roomHumidity, 2, 0, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    lcd.print(textBuffer);

    lcd.setCursor(2, 1);
    dtostrf(pH, 3, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    lcd.print(textBuffer);
    
    lcd.setCursor(7, 1);
    dtostrf(filterPressure, 3, 1, floatTextBuffer);
    sprintf(textBuffer, "%s", floatTextBuffer);
    lcd.print(textBuffer);
    
    lcd.setCursor(12, 1);    
    switch (rainSensorValue) 
    {
    case 0:    // Sensor getting wet
      lcd.print("O");
      break;
    case 1:    // Sensor getting wet
      lcd.print("W");
      break;
    case 2:    // Sensor dry - To shut this up delete the " Serial.println("Not Raining"); " below.
      lcd.print("N");
      break;

    lcd.print(textBuffer);
   }

   lcd.setCursor(14, 1);    
   if ( !mqttClient.connected() )
   {
     lcd.print("T?");
   }
   else
   {
     lcd.print("T=");
   }
  }


  currentScreen = screen;
}

// Fonction de mise à jour des Topic MQTT
// --------------------------------------
void UpdateMQTT()
{

  dtostrf(roomTemperature, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Local/Temperature", textBuffer);

  dtostrf(roomHumidity, 2, 0, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Local/Humidite", textBuffer);

  dtostrf(waterTemperature, 4, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Pool/Eau/Temperature", textBuffer);
  
  dtostrf(pH, 3, 1, floatTextBuffer);
  sprintf(textBuffer, "%s", floatTextBuffer);
  mqttClient.publish("PoolAndSpray/Pool/Eau/PH", textBuffer);

  sprintf(textBuffer, "%d", liquidLevel);
  mqttClient.publish("PoolAndSpray/Pool/Filtration/NiveauLiquide", textBuffer);

  mqttClient.publish("PoolAndSpray/Pool/Filtration/Ampere", "0");

  dtostrf(filterPressure, 3, 1, floatTextBuffer);
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

  switch (rainSensorValue) 
  {
  case 0:    // Sensor getting wet
    mqttClient.publish("PoolAndSpray/Arrosage/Pluie/EtatSenseur", "1");
    break;
  case 1:    // Sensor getting wet
    //mqttClient.publish("PoolAndSpray/Arrosagey/Pluie/EtatSenseur", "QUELQUES GOUTTES");
    break;
  case 2:    
    mqttClient.publish("PoolAndSpray/Arrosage/Pluie/EtatSenseur", "0");
    break;

  }
}

// Mise à jour de l'affichage courant
void UpdateCurrentDisplay()
{
  Display(currentScreen);
}


const uint8_t pin_keyboard = A0;

#define  RIGHT   0
#define  UP      1
#define  DOWN    2
#define  LEFT    3
#define  SELECT  4
#define  NONE    5


// Fonction de lecture de la touche appuyée (polling)
// --------------------------------------------------
int ReadKeyboardButton()
{
  int res = NONE;

  // On lit une valeur analogique pour connaitre l'état du clavier
  // car en fait on mesure une résistance et en fonction de la valeur,
  // on sait si une touche est appuyée
  int tempKeyValue = analogRead(pin_keyboard);

  // Les valeurs des résistances associées aux touches sont centrées sur :
  // 0, 144, 329, 504, 741
  // On ajoute 50 à ces valeurs et on vérifie si on est proche

  // On teste ça en 1er pour une raison de rapidité car c'est le cas le plus probable
  if ( tempKeyValue > 1000 )  res = NONE;
  
  else if ( tempKeyValue <  50 ) res = RIGHT;
  else if ( tempKeyValue < 195 ) res = UP;
  else if ( tempKeyValue < 380 ) res = DOWN;
  else if ( tempKeyValue < 555 ) res = LEFT;
  else if ( tempKeyValue < 850 ) res = SELECT;
  
  return res;
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
  sprintf(textBuffer, "Message arrived [%s]=\"%s\"", topic, payloadStr);
  
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
  
  // l'objet reset initialise Ethernet avec les valeur de l'EEPROM
  // current value:
  // IP  : 192.168.10.250
  // mac : { 0xDE, 0xDA, 0xBE, 0xEF, 0xFE, 0xED };
  // GW  : gateway(192, 168, 10, 1);
  // subnet(255, 255, 255, 0);
  // path (password) : ariadne (http://192.168.10.250:8080/ariadne/reset|program
   /* For now the Arduino EthShield and the server are being configured using the
   * settings already stored in the EEPROM and are the same with the ones for Ariadne bootloader.
   * This means that you *MUST* have updated the network settings on your Arduino with the
   * "WriteNetworkSettings" sketch.
   * The "begin()" command takes care of everything, from initializing the EthShield to
   * starting the web server for resetting. This is why you should always start it before any other
   * server you might want to have */
  logreset.begin();
 
  pinMode(FLOATLEVELPIN, INPUT);  

  mqttClient.setCallback(CallbackMQTT);
  mqttClient.setServer(remoteJeedomIp, MQTTBrokerPort);

  // Initialisation du LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Modul Pool&Spray");
  lcd.setCursor(0, 1);
  sprintf(textBuffer, "Version : %s", version);
  lcd.print(textBuffer);

  // pour prmettre de voir la version
  delay(500);
}


// -------------------------------------------------------------------------------------
// BOUCLE PRINCIPALE
// -------------------------------------------------------------------------------------

// Variables utilisées dans la boucle de traitement
unsigned long lastReadingTime = 0;
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
    
    // Initialisation du capteur de température de l'eau
    InitializeWaterTemperatureSensor();
  
    InitializeRelays();

    firstLoop = false;
  }
  else
  {
    time_t        nowSec      = now();
    unsigned long nowMillisec = millis();
  
    // Gestion de la connexion MQTT
    if ( !mqttClient.connected() )
    {
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
    else
    {
      // Client connected
      mqttClient.loop();
    }
  
    // Test du clavier à chaque passage
    int button = ReadKeyboardButton();
    if ( button != NONE )
    {
      if ( button == SELECT )
      {
        if (etatRelay1 == LOW) etatRelay1 = HIGH;
        else if (etatRelay1 == HIGH) etatRelay1 = LOW;
      }
      else if ( button == LEFT )
      {
        if (etatRelay2 == LOW) etatRelay2 = HIGH;
        else if (etatRelay2 == HIGH) etatRelay2 = LOW;
      }
      else if ( button == DOWN )
      {
        if (etatRelay3 == LOW) etatRelay3 = HIGH;
        else if (etatRelay3 == HIGH) etatRelay3 = LOW;
      }
      else if ( button == UP )
      {
        if (etatRelay4 == LOW) etatRelay4 = HIGH;
        else if (etatRelay4 == HIGH) etatRelay4 = LOW;
      }
      else if ( button == RIGHT )
      {
        if (etatRelay5 == LOW) etatRelay5 = HIGH;
        else if (etatRelay5 == HIGH) etatRelay5 = LOW;
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
  
    // Toutes les 10 secondes
    if ( nowMillisec - lastReadingTime >= 10000 )
    {
      if ( button != NONE )
      {
        sprintf(textBuffer, "Touche Appuye = %d\n", button);
        LogConsole(textBuffer);
      }
  
      // Mise à jour de la température de l'eau
      UpdateWaterTemperature();
  
      // Mise à jour de la température et de l'humidité du local
      UpdateRoomTemperatureAndHumidity();
  
      // Mise à jour du pH
      UpdatePH();
  
  //    // Mise à jour de l'ORP
  //    UpdateORP();
  
      // Mise à jour de la pression du filtre à sable
      UpdateFilterPressure();
  
      // Mise à jour de la détection de pluie 
      UpdateRainDropDetection();
  
      // Mise à jour du niveau liquide
      UpdateLiquidLevel();
      
      // Affichage des valeurs si on est sur l'écran d'affichage
      if ( currentScreen == VALUES_SCREEN )
      {
        UpdateCurrentDisplay();
      }
      else
      {
        Display(VALUES_SCREEN);
      }
  
      // Mis à jour des topic MQTT
      UpdateMQTT();
  
      lastReadingTime = nowMillisec;
    }
  }

   /* After the reset server is running the only thing needed is this command at the start of
   * the "loop()" function in your sketch and it will take care of checking for any reset or
   * reprogram requests */
   logreset.check();
}


