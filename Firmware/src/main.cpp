#include <Arduino.h>
#include <stdint.h>
#include "printf.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#include <SPI.h>
#include <Wire.h>
#include <Ports.h>
#include <nRF24L01.h>
#include <RF24.h>


#define DEVICE_ID       2
#define DEBUG_ENABLED

#define I2C_ADDRESS 0x3C

RF24 radio(15, 16); // Set up nRF24L01 radio on SPI bus (CE, CS)
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
const uint64_t pipeAddress = 0xF0F0F0F0D2LL;//0xF0F0F0F0E1LL;

OneWire oneWire1(7);
DallasTemperature sensor1(&oneWire1);
OneWire oneWire2(8);
DallasTemperature sensor2(&oneWire2);
OneWire oneWire3(9);
DallasTemperature sensor3(&oneWire3);

// mcu pins
int pinAnalog = 0;
int pinButton = 2;
int pinLed = 3;
uint16_t counter = 0;

float temperature1 = 0;
float temperature2 = 0;
float temperature3 = 0;

// flags
volatile bool isTimeout = false;
volatile bool isButton = false;

uint8_t counterPackets = 0;
uint32_t counterSendAttempts = 0;
uint32_t counterSendFailed = 0;
char packet[32];

void doMeasure(void);
void doSend(void);

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
    Serial.begin(57600);
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

  sensor1.begin();
  sensor2.begin();
  sensor3.begin();

  analogReference(DEFAULT);//EXTERNAL

  isTimeout = true;
  isButton = false;
  counterPackets = 0;
}

void loop() {

  // make sure everything is finished before go to sleep
  delay(20);

  // got to sleep if nothing happend during last loop
  if (!isButton && !isTimeout){
    // wait for button press or measure timer
    Sleepy::loseSomeTime(60000);
  }

  // button was pressed or released
  if(isButton){
    // digitalWrite(pinLed, HIGH);
    // delay(1000);
    // digitalWrite(pinLed, LOW);
    digitalWrite(pinLed, HIGH);
    doMeasure();
    doSend();
    digitalWrite(pinLed, LOW);
    isButton = false;
    isTimeout = false;
  }

  // measuring period timer
  if(isTimeout){
    digitalWrite(pinLed, HIGH);
    doMeasure();
    doSend();
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

  sensor1.requestTemperatures();
  sensor2.requestTemperatures();
  sensor3.requestTemperatures();
  Sleepy::loseSomeTime(1000);
  temperature1 = sensor1.getTempCByIndex(0);
  temperature2 = sensor2.getTempCByIndex(0);
  temperature3 = sensor3.getTempCByIndex(0);


  #ifdef DEBUG_ENABLED
    Serial.print(temperature1);
    Serial.print(", ");
    Serial.print(temperature2);
    Serial.print(", ");
    Serial.print(temperature3);
    Serial.println(" ");
  #endif

}

void doSend(void){

  packet[0] = DEVICE_ID; // id
  packet[1] = counterPackets; // packet counter
  packet[2] = 0; // packet type
  packet[3] = 0; // reserved

  float *packetF = (float*) &packet; //size_of(float)=4
  packetF[1] = temperature1;
  packetF[2] = temperature2;
  packetF[3] = temperature3;
  //packetF[4] = temperature4;

  uint32_t *packetUI23 = (uint32_t*) &packet;
  packetUI23[5] = counterSendFailed;

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
    Serial.println("\n=========================================");
  #endif

}
