/**
 * Temperature sensor test
 *
 * Testing reading Serial from pin 14 / D5
 */
#include "Arduino.h"
#include <DallasTemperature.h>
#include <OneWire.h>

#define ONE_WIRE_BUS 14
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  pinMode(LED_BUILTIN, OUTPUT);

  sensors.begin();
}

void loop() {
  sensors.requestTemperatures();

  Serial.print("temperature: ");
  Serial.println(sensors.getTempCByIndex(0));

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
}
