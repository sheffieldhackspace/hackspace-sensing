/**
 * main
 *
 * get environment information
 * send it over MQTT
 */

// secrets
#include <secrets.h>
// arduino
#include "Arduino.h"
// timer
#define DELAY_BETWEEN_MEASUREMENTS 30000
unsigned long lastExecutedMillis = 0; // last executed time
// blink LED
#include "BlinkDigits.h"
BlinkDigits flasher1;
int ledPin = LED_BUILTIN;
uint16_t flash_co2 = 0;
bool flashing_finished = false;
// SCD40 sensor
#include "SensirionI2CScd4x.h"
#include "Wire.h"
SensirionI2CScd4x scd4x;
uint16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
char co2_str[15];
char temperature_str[15];
char humidity_str[15];
// wifi
#include <ESP8266WiFi.h>
WiFiClient espClient;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
float rssi;
char rssi_str[15];
// mqtt
#include <PubSubClient.h>
PubSubClient client(espClient);
#define MQTT_TOPIC_STATUS MQTT_TOPIC_ROOT "status"
#define MQTT_TOPIC_RSSI MQTT_TOPIC_ROOT "rssi"
#define MQTT_TOPIC_CO2 MQTT_TOPIC_ROOT "co2"
#define MQTT_TOPIC_TEMPERATURE MQTT_TOPIC_ROOT "temperature"
#define MQTT_TOPIC_HUMIDITY MQTT_TOPIC_ROOT "humidity"

void printUint16Hex(uint16_t);
void printSerialNumber(uint16_t, uint16_t, uint16_t);
void mqtt_reconnect();

void setup() {
  // set up flasher
  flasher1.config(200, 800, 200, 1500); // slower timings in milliseconds.
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // set up serial
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // set up wifi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // mqtt
  client.setServer(MQTT_SERVER, MQTT_SERVERPORT);

  // set up SCD40
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
  digitalWrite(ledPin, HIGH);
}

void loop() {
  // WiFi - reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  // mqtt - check connection, reset if needed, and execute client loop
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  if (flasher1.blink(ledPin, LOW, flash_co2)) {
    flashing_finished = true;
  }

  //   execute only every DELAY_BETWEEN_MEASUREMENTS milliseconds
  if (currentMillis - lastExecutedMillis >= DELAY_BETWEEN_MEASUREMENTS) {
    lastExecutedMillis = currentMillis; // save the last executed time

    // do sensor stuff
    uint16_t error;
    char errorMessage[256];

    // read sensor
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
      if (flashing_finished && (co2 != flash_co2)) {
        flash_co2 = co2;
        flashing_finished = false;
      }

      rssi = WiFi.RSSI();

      Serial.print("Co2\t");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print("Temperature\t");
      Serial.print(temperature);
      Serial.print("\t");
      Serial.print("Humidity\t");
      Serial.println(humidity);

      Serial.print("RSSI\t");
      Serial.println(rssi);

      Serial.println("sending to MQTT...");
      dtostrf(rssi, 4, 0, rssi_str);
      dtostrf(co2, 4, 0, co2_str);
      dtostrf(temperature, 4, 3, temperature_str);
      dtostrf(humidity, 4, 3, humidity_str);

      client.publish(MQTT_TOPIC_RSSI, rssi_str, true);
      client.publish(MQTT_TOPIC_CO2, co2_str, true);
      client.publish(MQTT_TOPIC_TEMPERATURE, temperature_str, true);
      client.publish(MQTT_TOPIC_HUMIDITY, humidity_str, true);
    }
  }
}

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

void mqtt_reconnect() {
  Serial.print("Attempting MQTT connection...");
  while (!client.connected()) {
    if (client.connect(HOSTNAME)) {
      Serial.println("connected");
      client.publish(MQTT_TOPIC_STATUS, "on", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
