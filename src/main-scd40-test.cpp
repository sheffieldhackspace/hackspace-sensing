#include "Arduino.h"
#include "BlinkDigits.h"
#include "SensirionI2CScd4x.h"
#include "Wire.h"

SensirionI2CScd4x scd4x;
BlinkDigits flasher1;
int ledPin = LED_BUILTIN;
uint16_t flash_co2 = 0;
bool flashing_finished = false;

void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
}

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  flasher1.config(200, 800, 200, 1500); // slower timings in milliseconds.

  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Wire.begin(4, 5);

  uint16_t error;
  char errorMessage[256];

  scd4x.begin(Wire);

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    printSerialNumber(serial0, serial1, serial2);
  }

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  Serial.println("Waiting for first measurement... (5 sec)");
}

void loop() {
  uint16_t error;
  char errorMessage[256];

  // bool fin = flasher1.blink(ledPin, LOW, flash_co2);
  if (flasher1.blink(ledPin, LOW, flash_co2)) {
    flashing_finished = true;
  }

  // Read Measurement
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  bool isDataReady = false;
  error = scd4x.getDataReadyFlag(isDataReady);
  if (error) {
    Serial.print("Error trying to execute getDataReadyFlag(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    return;
  }
  if (!isDataReady) {
    return;
  }
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (co2 == 0) {
    Serial.println("Invalid sample detected, skipping.");
  } else {
    // Serial.print("fin is ");
    // Serial.println(fin);
    // Serial.print("flash_co2 is ");
    // Serial.println(flash_co2);

    if (flashing_finished && (co2 != flash_co2)) {
      flash_co2 = co2;
      flashing_finished = false;
    }
    // flasher does not return true properly so just set it here
    // flash_co2 = co2;

    Serial.print("Co2\t");
    Serial.print(co2);
    Serial.print("\t");
    Serial.print("Temperature\t");
    Serial.print(temperature);
    Serial.print("\t");
    Serial.print("Humidity\t");
    Serial.println(humidity);
  }
}