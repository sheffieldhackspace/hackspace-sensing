/**
 * Moisture test
 *
 * Testing reading the analogue input pin
 */
#include "Arduino.h"

const int ADC_PIN = A0;
int value = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  value = analogRead(ADC_PIN);
  Serial.print("ADC: ");
  Serial.println(value);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);
}
