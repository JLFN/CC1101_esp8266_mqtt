/*
  "Gateway" to send and recieve asyncronus 433 MHz signals and
  communicate through mqtt.

  Tested with
  Arduino       1.8.5
  ESP8266       2.4.1
  PubSubClient  2.6.0
  NodeMCU 0.9, 80 MHz, 4M (1M SPIFFS), v2 Lower Memory
  NodeMCU 1.0, 80 MHz, 4M (1M SPIFFS), v2 Lower Memory
  
  CC1101 library and some inspiration from:
  https://github.com/incmve/Itho-WIFI-remote

  More inspiration and ideas
  http://www.silogic.com/trains/OOK_Radio_Support.html

  A lot of spaghetti code...

  TODO:
  - "Reset" the sampler limit sometimes. So it won't start lagging.
    Or at least evaluate if it might
  - Evaluate the implementation of time triggers ("limit")
    https://playground.arduino.cc/Code/TimingRollover
  - Less global variables when possible
  - CC1101 - Move register settings and radio functions 
    to own new file (write/read registers is public).
  - Signal processing etc. in own file/class
  - Listen before talk from "carrier sense" (almost done)
  - Tidy up!!!

Connections between the CC1101 and the ESP8266:
CC11xx pins    ESP pins       Description
*  1 - VCC        VCC           3v3
*  2 - GND        GND         Ground
*  3 - MOSI       13=D7     Data input to CC11xx
*  4 - SCK        14=D5     Clock pin
*  5 - MISO/GDO1  12=D6     Data output from CC11xx / serial clock from CC11xx
*  6 - GDO2       D2       Programmable output
*  7 - GDO0       D1         Asyncronus IO (Programmable output)
*  8 - CSN        15=D8     Chip select / (SPI_SS)

*/

#define DEBUG_PRINTDONTSEND 1

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "CC1101.h"
#include "EsicReceiver.h"
#include "NexaReceiver.h"
#include "NexaTx.h"
#include "BinSmooth.h"

#define GDO0  D1   // CC1101 GDO0 pin connected to...
#define GDO2  D2   // CC1101 GDO2 pin connected to...

#define RXD_PIN GDO0  // CC1101 asyncronus recieve
#define TXD_PIN GDO0  // CC1101 asyncrounus transmitt


WiFiClient espClient;
PubSubClient client(espClient);
NexaTx nexaTransmitter(TXD_PIN);
EsicReceiver esicReceiver;
NexaReceiver nexaReceiver;
CC1101 radio433;

// Dependent on sampling period and decoder timing tolerance!
// Currently 50us sampling period
BinSmooth fastSignal(3);  // 3 works fine (without hysteres in "checkRadio")
BinSmooth slowSignal(14); // 14 works fine (with single gap hysteres in "checkRadio")
// The esic thermometers have really crappy signal strength so choise of antenna,
// polarization and placement is cruical, but the sampling and radio settings could
// likely also be tweaked to improve reception.


const char* ssid =        "";
const char* password =    "";
const char* mqtt_server = "";
const uint16_t  mqtt_port = 1883;

const char* subsMess =      "Subscribe failed, retrying";
const uint16_t subsDelay =  2000;

// Publish * * * * * * * * * * * * * * * * * * * * *
const char* nexaTx =      "home/device/nexaRemote/state";
const char* ethTx =       "home/device/eth/state";
const char* rssiPub =     "home/433/rssi/state";
const char* nfPub =       "home/433/nf/state";
const char* nmcuUptPub =  "home/device/nmcu/upt/state";


// Subscribe * * * * * * * * * * * * * * * * * * * * *
const char* nexa11 =        "home/nexa/1/1/com";
const char* nexa12 =        "home/nexa/1/2/com";
const char* nexa13 =        "home/nexa/1/3/com";
// etc

// GLOBALS * * * * * * * * * * * * * * * * * * * * *
const char* mqttMess0 = "0;";
uint8_t sendRSSI = 0; // By default, off (0).

void setup_wifi() {
  uint8_t i = 0;
  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to:\t");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(50 <= ++i) {
      Serial.println();
      i=0;
    }
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
#ifdef DEBUG_PRINTDONTSEND
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif

  // Tidy up. atoi aint that great and why round?
  if (0 == strcmp(topic, nexa11)){
    doNexaSend(1, 1, round(atoi((char *)payload)));
  } else if (0 == strcmp(topic, nexa12)){
    doNexaSend(1, 2, round(atoi((char *)payload)));
  } else if (0 == strcmp(topic, nexa13)){
    doNexaSend(1, 3, round(atoi((char *)payload)));
  } else if (0 == strcmp(topic, rssiSub)){
    setSendRssi(round(atoi((char *)payload)));
  }
} 

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");
    // Attempt to connect
    if(client.connect("ESP8266 Client 2")) {
      // Once connected, (re)subscribe
      Serial.println("connected");

      while( !client.subscribe(nexa11, nexaMqttQos) )
      {Serial.println(subsMess); delay(subsDelay);}
      while( !client.subscribe(nexa12, nexaMqttQos) )
      {Serial.println(subsMess); delay(subsDelay);}
      while( !client.subscribe(nexa13, nexaMqttQos) )
      {Serial.println(subsMess); delay(subsDelay);}

      while( !client.subscribe(rssiSub, 1) )
      {Serial.println(subsMess); delay(subsDelay);}

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      Serial.print("WiFiClient state: ");
      Serial.println(espClient.status());
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


/*************************** Radio stuff *******************************/


struct registerSetting_t {  // SmartRF settings
  unsigned char Reg;
  unsigned char Value;
};

static const registerSetting_t C433AsyncAsk[]= {
  {CC1101_IOCFG2,      0x49},
  {CC1101_IOCFG1,      0x2E},
  {CC1101_IOCFG0,      0x0D},
  {CC1101_FIFOTHR,     0x47},
  {CC1101_PKTCTRL0,    0x32},
  {CC1101_CHANNR,      0x00},
  {CC1101_FSCTRL1,     0x06},
  {CC1101_FREQ2,       0x10},
  {CC1101_FREQ1,       0xB0},
  {CC1101_FREQ0,       0x50}, 
  {CC1101_MDMCFG4,     0x47},
  {CC1101_MDMCFG3,     0x43},
  {CC1101_MDMCFG2,     0x30},
  {CC1101_MDMCFG1,     0x21},
  {CC1101_MDMCFG0,     0xF8},
  {CC1101_MCSM1,       0b00011111},
  {CC1101_MCSM0,       0x08},
  {CC1101_FOCCFG,      0x00},
  {CC1101_AGCCTRL2,    0b00000111}, // Target amplitude
  {CC1101_AGCCTRL1,    0b00011000}, // Carrier Sense settings
  {CC1101_AGCCTRL0,    0b11000000}, // Gain settings
  {CC1101_WORCTRL,     0xFB},
  {CC1101_FREND0,      0x11},
  {CC1101_FSCAL3,      0xE9},
  {CC1101_FSCAL2,      0x2A},
  {CC1101_FSCAL1,      0x00},
  {CC1101_FSCAL0,      0x1F},
  {CC1101_TEST2,       0x81},
  {CC1101_TEST1,       0x35},
  {CC1101_TEST0,       0x09},
};

const byte paTable1[8] = {0x00, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void regConfigSettings() {
  #define FREQ_IN_USE C433AsyncAsk

  for (int idx = 0 ; idx < (sizeof(FREQ_IN_USE)/2) ; idx++) {
    radio433.writeRegister(FREQ_IN_USE[idx].Reg,  FREQ_IN_USE[idx].Value); 
  }
  // Set transmission power
  radio433.writeBurstRegister(CC1101_PATABLE, (byte *) paTable1, 8);
}


void radioSetTx(int channel) {
  // Wait for clear channel (or switch anyway after X ms)
  uint32_t startMillis = millis();
  while(digitalRead(GDO2) && (millis()-startMillis) < 5000)
    delay(20);
  //Serial.print(F("LBT delay: "));Serial.print(millis()-startMillis);Serial.println(F(" ms"));

  // Switch to Tx
  // Might need to turn CCA off to force?
  if ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x13) {
    radio433.writeCommand(CC1101_STX);
    while ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x13) {
      delay(10);
      // Turn CCA off when timer override instead?
      radio433.writeCommand(CC1101_STX);
    }
    pinMode(TXD_PIN, OUTPUT);  // TX mode
    digitalWrite(TXD_PIN, LOW);
  }
}

void radioSetIdle() {
  pinMode(RXD_PIN, INPUT);  // RX mode
  if ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x01) {
    radio433.writeCommand(CC1101_SIDLE); // set IDLE state and clear CS
    while ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x01)
      yield();
  }
}

void radioSetRx() {
  pinMode(RXD_PIN, INPUT);  // RX mode
  if( ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x0D)) {
    radio433.writeCommand(CC1101_SRX);
    while( ((radio433.readRegister(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1F) != 0x0D))
      yield();
  }  
}

int ReadRSSIReg() {
  unsigned char NewRSSI = radio433.readRegister(CC1101_RSSI, CC1101_STATUS_REGISTER);
  return (NewRSSI < 128) ? (NewRSSI/2 - 74) : ((NewRSSI-256)/2 - 74);
}

void upTimePub() {
  // Call every 60s, timing elsewhere!
  static uint32_t counterD;
  static uint8_t  counters[2];
  
  char upTimeMess[20];
  int8_t tempI = sprintf(upTimeMess, "%lu D, %u h, %u m%c", counterD, counters[1], counters[0], '\0');
  if(60 <= ++counters[0]) {
    counters[0] = 0;
    if(24 <= ++counters[1]) {
      counters[1] = 0;
      counterD++;
    }
  }
  if(0 < tempI) {
    if (!client.publish(nmcuUptPub, (char*)upTimeMess )) {
      Serial.println(F("nmcuUptPub:\tFailed"));
    }
  } 
}

void sendCurrentRssi(int8_t thisRead) {
  // Update if different from last
  static int8_t lastRead;

  //int thisRead = ReadRSSIReg();
  char tempMess[10];

  if(1 == sendRSSI && thisRead != lastRead && 0 < sprintf(tempMess, "%i%c", thisRead, '\0'))
    if (!client.publish(rssiPub, (char*)tempMess))
      Serial.println(F("Failed"));
  lastRead = thisRead;
}

/*************************** Signal stuff ***********************************/

void checkRadio() {
  
  const uint8_t   avgOver = 3;
  const uint8_t   minLim = 1; // floor(avgOver/2)
  static uint32_t limit;
  
  if( 0 <= (int32_t)(micros() - limit) ) {
    limit += 50;

    // Sampling
#if 1
    uint8_t   i = avgOver,
              sum = 0;
    while(0<i--) {
      sum += digitalRead(RXD_PIN);
    }
    boolean thisLvl = minLim < sum;
#else
    boolean thisLvl = digitalRead(RXD_PIN);
#endif

    fastSignal.newSignal(thisLvl);
    slowSignal.newSignalGap(thisLvl);
  }
}

void printRSSI() {
  static uint32_t nextTime;
  static int16_t sum;
  static int8_t readings[60], i;
  
  if(0 <= (int32_t)(millis()-nextTime)) {
    nextTime += 4000;
    
    sum -= readings[i];
    readings[i] = ReadRSSIReg();
    sum += readings[i];

    if(15 <= ++i) {
      i = 0;
      Serial.print(F("433MHz RSSI:\t"));
      Serial.print(round(sum/15));
      Serial.println(F(" dBm"));
    }
  }
}


void printRSSI(int8_t thisRead) {
  Serial.print(F("\t"));
  Serial.print(thisRead);
  Serial.println(F(" dBm"));
}

/****************************************************************************/


void setup() {
  pinMode(GDO2, INPUT); // CCA
  
  Serial.begin(9600);
  Serial.println();
  Serial.println(F("NodeMCU (WiFi mqtt) <-> CC1101 (433MHz)"));
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  radio433.init();
  regConfigSettings();
  radio433.writeCommand(CC1101_SCAL);
  radioSetRx();
}

int8_t thisRSSI;
uint8_t nexaResp, esicResp,tmr[3];
uint32_t tmr32, uTmr32;

void loop() {
  
  checkRadio();

  if(fastSignal.availSignal()) {
    
    if(nexaReceiver.nReceive(fastSignal.getDuration(), fastSignal.getLevel())) {
      thisRSSI = ReadRSSIReg();
      nexaResp = nexaReceiver.nDecode();
      if(2 == nexaResp) {
#ifndef DEBUG_PRINTDONTSEND
        if (!client.publish(nexaTx, mqttMess0)) {
          Serial.println(F("nexaTx:\tFailed"));
        }
#endif
        delay(100);
      }
      if(nexaResp) {
#ifndef DEBUG_PRINTDONTSEND
        if(!client.publish(nexaTx, nexaReceiver.getMessage())) {
          Serial.println(F("nexaTx:\tFailed"));
        }
        sendCurrentRssi(thisRSSI);
#else        
        Serial.print(nexaReceiver.getMessage());
        Serial.print(F("\t"));
        printRSSI(thisRSSI);
#endif        
      }
    }
  }
  
  if(slowSignal.availSignal()) {
    // Validate...

    if(esicReceiver.eReceive(slowSignal.getDuration())) {
      thisRSSI = ReadRSSIReg();
      esicResp = esicReceiver.eDecode();
      if(2 == esicResp) {
#ifndef DEBUG_PRINTDONTSEND
        if(!client.publish(ethTx, mqttMess0)) {
          Serial.println(F("ethTx:\tFailed"));
        }
#endif
        delay(100);
      }
      if(esicResp) {
#ifndef DEBUG_PRINTDONTSEND
        if (!client.publish(ethTx, esicReceiver.getMessage())) {
          Serial.println(F("ethTx:\tFailed"));
        }
        sendCurrentRssi(thisRSSI);
#else
        Serial.print(esicReceiver.getMessage());
        printRSSI(thisRSSI);
#endif
      }
    }
  }

  if(!client.connected()) reconnect();
  client.loop();
  yield();

  // "Clock"
  if(0 <= (int32_t)(millis()-tmr32)) {
    tmr32 += 1000;
    if(60 <= ++tmr[0]) {
      tmr[0] = 0;
      if(60 <= ++tmr[1]) {
        tmr[1] = 0;
        if(24 <= ++tmr[2]) {
          tmr[2] = 0;
          // Every day - - - - - - - - -
        }
        // Every hour - - - - - - - - -
      }
      // Every minute - - - - - - - - -
      if(0 == tmr[1]%5) { // Every 5 min
        radio433.writeCommand(CC1101_SCAL);
        //Serial.println(F("Recalibrated"));
      }
#ifndef DEBUG_PRINTDONTSEND
      upTimePub();
#endif
      
      //Serial.println(F("Still alive!"));
    }
    // Every second - - - - - - - - -
#ifndef DEBUG_PRINTDONTSEND
    doTimedRSSI();
#else
    printRSSI();
#endif
  }

}


/*************************** Functions ************************************/

void doNexaSend(byte remote, byte dev, byte onoff) {
  uint8_t retransmissions = 5; // > 3 or change further down
  static uint32_t lastAny;
  static boolean lastMode[4][3];
  static unsigned long lastTime[4][3];
  // Can only use mqtt QoS 0 and 1. Using 1 so need to handle duplicates
  if(onoff != lastMode[remote-1][dev-1] || 1000 < abs( millis()-lastTime[remote-1][dev-1]) ) {
    if( (millis() - lastAny) < 100) retransmissions -= 2;
    radioSetTx(0);
#ifndef DEBUG_PRINTDONTSEND
    nexaTransmitter.nexaTransmitt(1700000 + remote, onoff, dev - 1, retransmissions);
#endif // DEBUG_PRINTDONTSEND
    radioSetRx();
    lastAny = millis();
  }
  lastMode[remote-1][dev-1] = onoff;
  lastTime[remote-1][dev-1] = millis();
}

void setSendRssi(uint8_t val) {
  if(sendRSSI != val) {
    if(1 < val) val = 0;
    sendRSSI = val;
    // Report back? Not now. Set QoS = 1 instead
  }
}