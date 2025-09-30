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
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
// do one thing
#elif defined(ESP32)
#include <WiFi.h>
// do another
#else
#error Architecture unrecognized by this code.
#endif
WiFiClient espClient;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// mqtt
#include <PubSubClient.h>
PubSubClient client(espClient);
#define MQTT_TOPIC_STATUS MQTT_TOPIC_ROOT "status"
#define MQTT_TOPIC_TEMPERATURE MQTT_TOPIC_ROOT "temperature"

void mqtt_reconnect();

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // SETUP wifi
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
}

void loop() {
  Serial.println("LED OFF");
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);

  Serial.println("LED ON (MQTT)");
  client.publish(MQTT_TOPIC_TEMPERATURE, "18.294", true);
  digitalWrite(LED_BUILTIN, LOW);

  delay(5000);

  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  // mqtt - check connection and reset if needed
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();
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
