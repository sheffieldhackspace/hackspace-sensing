/**
 * mosquittopub
 *
 * send a message to an MQTT network periodically
 * requires secrets set in secrets.h
 */

// secrets
#include <secrets.h>
// arduino
#include "Arduino.h"
// wifi
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266-mosquittoer-alfie"
// define these in secrets.h
// #define WIFI_SSID (from secrets)
// #define WIFI_PASSWORD (from secrets)
// mqtt
#include <PubSubClient.h>
const char *mqtt_server = "broker.mqtt-dashboard.com";
PubSubClient client(wifiMulti);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // SETUP wifi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // mqtt
  client.setServer(mqtt_server, 1883);
}

void loop() {
  Serial.println("LED OFF");
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);

  Serial.println("LED ON");
  // client.publish("SHHNoT/test/alfie", "hi:]");
  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
}
