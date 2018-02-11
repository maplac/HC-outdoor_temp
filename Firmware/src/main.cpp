#include <Arduino.h>
#include <stdint.h>
#include "printf.h"
#include <OneWire.h>
#include <DallasTemperature.h>

uint16_t counter = 0;

OneWire oneWire1(7);
DallasTemperature sensor1(&oneWire1);
OneWire oneWire2(8);
DallasTemperature sensor2(&oneWire2);
OneWire oneWire3(9);
DallasTemperature sensor3(&oneWire3);

void setup() {
  Serial.begin(57600);
  printf_begin();
  sensor1.begin();
  sensor2.begin();
  sensor3.begin();
}

void loop() {

    for(;;){
      Serial.print("Temperature: ");
      sensor1.requestTemperatures();
      sensor2.requestTemperatures();
      sensor3.requestTemperatures();
      Serial.print(sensor1.getTempCByIndex(0));
      Serial.print(", ");
      Serial.print(sensor2.getTempCByIndex(0));
      Serial.print(", ");
      Serial.print(sensor3.getTempCByIndex(0));
      Serial.println(" ");
      //Serial.println();
      counter++;
      delay(1000);
    }
}
