#ifdef F_CPU
#undef F_CPU
#endif
#define F_CPU 1000000L

#include <Arduino.h>
#include <stdint.h>
#include "printf.h"

#include <SPI.h>
#include <Wire.h>
#include <Ports.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "SparkFunTMP102.h"
#include "Vcc.h"

#define DEVICE_ID       2
//#define DEBUG_ENABLED   1

TMP102 sensor0(0x48);

RF24 radio(10, 9); // Set up nRF24L01 radio on SPI bus (CE, CS)
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
const uint64_t pipeAddress = 0xF0F0F0F0D2LL;//0xF0F0F0F0E1LL;

const float VccCorrection = 3.16/3.215;  // Measured Vcc by multimeter divided by reported Vcc
Vcc vcc(VccCorrection);

// mcu pins
// const int pinAnalog = 0;
const int pinButton = 3;
const int pinLed = 16;
uint16_t counter = 0;

float voltage;
float temperature1;
float temperature2;

// flags
volatile bool isTimeout = false;
volatile bool isButton = false;

uint8_t counterPackets = 0;
uint32_t counterSendAttempts = 0;
uint32_t counterSendFailed = 0;
char packet[32];

void doMeasure(void);
bool doSend(void);

ISR(WDT_vect) {
  Sleepy::watchdogEvent();
  isTimeout = true;
}

void buttonInt(void){
  isButton = true;
}

void setup() {

  pinMode(pinButton, INPUT);
  pinMode(pinLed, OUTPUT);
  digitalWrite(pinLed, LOW);
  attachInterrupt(digitalPinToInterrupt(pinButton), buttonInt, RISING );

  #ifdef DEBUG_ENABLED
    Serial.begin(9600);
    printf_begin();
  #endif

  radio.begin();
  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);
  // optionally, reduce the payload size.  seems to improve reliability
  //radio.setPayloadSize(8);
  // Open 'our' pipe for writing
  radio.openWritingPipe(pipeAddress);
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)
  //radio.openReadingPipe(1,pipes[1]);
  //radio.openWritingPipe(pipes[1]);
  //radio.openReadingPipe(1,pipes[0]);
  radio.startListening();
  radio.stopListening();

  #ifdef DEBUG_ENABLED
    radio.printDetails();
  #endif

  uint8_t registerByte[2];
  #ifdef DEBUG_ENABLED

    sensor0.openPointerRegister(0x01);// config register
    registerByte[0] = sensor0.readRegister(0);
    registerByte[1] = sensor0.readRegister(1);
    // Serial.print("TMP102 config register: ");
    // Serial.print(registerByte[0]);
    // Serial.print(" ");
    // Serial.println(registerByte[1]);
  #endif

  // Initialize sensor0 settings
  // These settings are saved in the sensor, even if it loses power
  sensor0.begin();  // Join I2C bus
  // set the number of consecutive faults before triggering alarm.
  // 0-3: 0:1 fault, 1:2 faults, 2:4 faults, 3:6 faults.
  sensor0.setFault(0);  // Trigger alarm immediately
  // set the polarity of the Alarm. (0:Active LOW, 1:Active HIGH).
  sensor0.setAlertPolarity(1); // Active HIGH
  // set the sensor in Comparator Mode (0) or Interrupt Mode (1).
  sensor0.setAlertMode(1); // Comparator Mode.
  // set the Conversion Rate (how quickly the sensor gets a new reading)
  //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
  sensor0.setConversionRate(0);
  //set Extended Mode.
  //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
  sensor0.setExtendedMode(0);
  //set T_HIGH, the upper limit to trigger the alert on
  sensor0.setHighTempC(80); // set T_HIGH in C
  //set T_LOW, the lower limit to shut turn off the alert
  sensor0.setLowTempC(-40); // set T_LOW in C
  sensor0.sleep();

  #ifdef DEBUG_ENABLED
    sensor0.openPointerRegister(0x01);// config register
    registerByte[0] = sensor0.readRegister(0);
    registerByte[1] = sensor0.readRegister(1);
    // Serial.print("TMP102 config register: ");
    // Serial.print(registerByte[0]);
    // Serial.print(" ");
    // Serial.println(registerByte[1]);
  #endif

  voltage = 1.5;
  temperature1 = -1000;
  temperature2 = -1000;

  // analogReference(DEFAULT);//EXTERNAL

  isTimeout = true;
  isButton = false;
  counterPackets = 0;
}

void loop() {
// while(true){
//   digitalWrite(pinLed, HIGH);
//   Sleepy::loseSomeTime(500);
//   digitalWrite(pinLed, LOW);
//   Sleepy::loseSomeTime(500);
// }

  // make sure everything is finished before go to sleep
  delay(40);

  // got to sleep if nothing happend during last loop
  if (!isButton && !isTimeout){
    // wait for button press or measure timer
    Sleepy::loseSomeTime(60000);
  }

  // button was pressed or released
  if(isButton){
    doMeasure();
    bool sendOk = doSend();
    digitalWrite(pinLed, HIGH);
    Sleepy::loseSomeTime(30);
    digitalWrite(pinLed, LOW);
    if(!sendOk){
      Sleepy::loseSomeTime(200);
      digitalWrite(pinLed, HIGH);
      Sleepy::loseSomeTime(30);
      digitalWrite(pinLed, LOW);
    }
    isButton = false;
    isTimeout = false;
  }

  // measuring period timer
  if(isTimeout){
    doMeasure();
    doSend();
    digitalWrite(pinLed, HIGH);
    Sleepy::loseSomeTime(30);
    digitalWrite(pinLed, LOW);
    isTimeout = false;
  }

}

void doMeasure(void){
  // read battery voltage
  //voltage = analogRead(pinAnalog) * 3.3 / 1024.0;

  #ifdef DEBUG_ENABLED
    Serial.print("Temperature: ");
  #endif

  voltage = vcc.Read_Volts();

  // read temperature data
  sensor0.oneShot();
  Sleepy::loseSomeTime(50);
  temperature1 = sensor0.readTempC();

  #ifdef DEBUG_ENABLED
    Serial.print(temperature1);
    if(temperature2 > -100){
      Serial.print(", ");
      Serial.print(temperature2);
    }
    Serial.println(" ");
    Serial.print("Voltage: ");
    Serial.println(voltage);
  #endif
}

bool doSend(void){

  packet[0] = DEVICE_ID; // id
  packet[1] = counterPackets; // packet counter
  packet[2] = 0; // packet type
  packet[3] = 0; // reserved

  float *packetF = (float*) &packet; //size_of(float)=4
  packetF[1] = voltage;
  packetF[2] = temperature1;
  packetF[3] = temperature2;
  //packetF[5] = temperature4;

  uint32_t *packetUI23 = (uint32_t*) &packet;
  packetUI23[4] = counterSendFailed;

  #ifdef DEBUG_ENABLED
    Serial.print("counter = ");
    Serial.print(counterPackets);
  #endif

  bool isSendOk = radio.write( &packet, 32 );
  counterPackets++;
  if(counterPackets == 0){
    counterPackets = 1;
  }
  counterSendAttempts++;
  if (!isSendOk){
    counterSendFailed++;
    #ifdef DEBUG_ENABLED
      Serial.print(", counterFailed = ");
      Serial.print(counterSendFailed);
    #endif
  }

  radio.startListening();
  radio.stopListening();

  #ifdef DEBUG_ENABLED
    // Serial.println("\n=========================================");
  #endif

  return isSendOk;
}
