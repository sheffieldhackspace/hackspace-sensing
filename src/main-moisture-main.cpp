/**
 * Moisture test
 *
 * Testing reading the analogue input pin
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
uint16_t flash_val = 0;
bool flashing_finished = false;
// moisture sensor
const int ADC_PIN = A0;
int moisture = 0;
char moisture_str[15];
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
#define MQTT_TOPIC_MOISTURE MQTT_TOPIC_ROOT "moisture"

void mqtt_reconnect();

void setup() {
  // set up flasher
  flasher1.config(200, 800, 200, 1500);
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

  // wait
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

  if (flasher1.blink(ledPin, LOW, flash_val)) {
    flashing_finished = true;
  }

  // sensor
  //   execute only every DELAY_BETWEEN_MEASUREMENTS milliseconds
  if (currentMillis - lastExecutedMillis >= DELAY_BETWEEN_MEASUREMENTS) {
    lastExecutedMillis = currentMillis;
    // wifi
    rssi = WiFi.RSSI();
    // moisture
    moisture = analogRead(ADC_PIN);

    if (flashing_finished && (moisture != flash_val)) {
      flash_val = moisture;
      flashing_finished = false;
    }

    Serial.print("ADC\t");
    Serial.print(moisture);
    Serial.print("\t");
    Serial.print("RSSI\t");
    Serial.println(rssi);

    Serial.println("sending to MQTT...");
    dtostrf(rssi, 4, 0, rssi_str);
    dtostrf(moisture, 4, 0, moisture_str);

    client.publish(MQTT_TOPIC_RSSI, rssi_str, true);
    client.publish(MQTT_TOPIC_MOISTURE, moisture_str, true);
  }
}

void mqtt_reconnect() {
  Serial.print("Attempting MQTT connection to ");
  Serial.println(MQTT_SERVER);
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
