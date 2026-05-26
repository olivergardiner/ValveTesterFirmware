#include <Arduino.h>
#include "ValveTester.h"

ValveTester *tester;

void setup()
{
  Serial.begin(115200); // Setup serial interface

  // I2C SDA is on Arduino Nano pin A4 as standard
  // I2C SCL is on Arduino Nano pin A5 as standard. These pins need no further setup.
  // By default, analog input pins also need no setup

  analogReference(EXTERNAL); // Use external voltage reference for ADC

  pinMode(LED_BUILTIN, OUTPUT); // Arduino built-in LED for debugging

  digitalWrite(LED_BUILTIN, HIGH);

  tester = new ValveTester();
}

void loop()
{
  // put your main code here, to run repeatedly:
  while (Serial.available() > 0)
  {
    tester->parseInput(Serial.read());
  }
}
