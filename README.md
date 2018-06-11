# HC-outdoor_temp

Nahrání bootloaderu do mikrokontroléru:
1. Do Arduino UNO nahrát ArduinoISP sketch.
2. Signálové a napájecí vodiče propojit, jak je uvedeno v https://www.arduino.cc/en/Tutorial/ArduinoToBreadboard. 
3. Do Arduino IDE přidat MiniCore https://github.com/MCUdude/MiniCore.
4. Nastavit MiniCore - Board:Atmega328, Bootloader:yes, Variant:328P/328PA, BOD:1,8v, Clock:1 MHz internal, Compiler LTO:Disabled. 
5. Nastavit programátor na "Arduino as ISP" a spustit Burn Bootloader.