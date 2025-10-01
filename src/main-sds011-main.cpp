/**
 * particulate monitor
 *
 * currently it sends data to MQTT "sometimes"
 * the other times I don't know what happens
 * seemingly it doesn't ever complete properly, or something?
 */
// secrets
#include <secrets.h>

// arduino
#include <Arduino.h>

// blink LED
#include "BlinkDigits.h"
BlinkDigits flasher1;
int ledPin = 8;
int flash_val = 0;
bool flashing_finished = false;

// particulate sensor (sds011)
#include <SoftwareSerial.h>
#include <esp_sds011.h>
#define SDS_PIN_RX 3 // Remap to GPIO 8 (SDA)
#define SDS_PIN_TX 4 // Remap to GPIO 9 (SCL)
EspSoftwareSerial::UART serialSDS;
Sds011Async<EspSoftwareSerial::UART> sds011(serialSDS);
constexpr int pm_tablesize = 20;
int pm25_table[pm_tablesize];
int pm10_table[pm_tablesize];
constexpr uint32_t down_s = 210; // waiting (fan off) time, default 210
constexpr uint32_t duty_s = 30;  // measuring (fan on) time, default 30
uint32_t deadline;
uint32_t lastSerialTimerMsg = 0;
int sds_state; // 1 - waiting (fan off), 2 - measuring (fan on)
bool is_SDS_running = true;
char pm25_str[15];
char pm10_str[15];
void start_SDS();
void stop_SDS();

// wifi
#include <WiFi.h>
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
#define MQTT_TOPIC_PM10 MQTT_TOPIC_ROOT "PM10"
#define MQTT_TOPIC_PM25 MQTT_TOPIC_ROOT "PM2.5"
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

  // sds sensor
  serialSDS.begin(9600, SWSERIAL_8N1, SDS_PIN_RX, SDS_PIN_TX, false, 192);
  Serial.println(F("SDS011 start/stop and reporting sample"));
  Sds011::Report_mode report_mode;
  constexpr int GETDATAREPORTINMODE_MAXRETRIES = 2;
  for (auto retry = 1; retry <= GETDATAREPORTINMODE_MAXRETRIES; ++retry) {
    if (!sds011.get_data_reporting_mode(report_mode)) {
      if (retry == GETDATAREPORTINMODE_MAXRETRIES) {
        Serial.println(F("Sds011::get_data_reporting_mode() failed"));
      }
    } else {
      break;
    }
  }
  if (Sds011::REPORT_ACTIVE != report_mode) {
    Serial.println(F("Turning on Sds011::REPORT_ACTIVE reporting mode"));
    if (!sds011.set_data_reporting_mode(Sds011::REPORT_ACTIVE)) {
      Serial.println(
          F("Sds011::set_data_reporting_mode(Sds011::REPORT_ACTIVE) failed"));
    }
  }

  deadline = millis() + duty_s * 1000;
  sds_state = 2;
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

  // flasher
  if (flasher1.blink(ledPin, LOW, flash_val)) {
    flashing_finished = true;
  }

  if (millis() - lastSerialTimerMsg > 1000) {
    Serial.println(static_cast<int32_t>(deadline - millis()) / 1000);
    lastSerialTimerMsg = millis();
    sds011.perform_work();
  }

  // waiting
  if (sds_state == 1) {
    // enter measuring state
    if (static_cast<int32_t>(deadline - millis()) < 0) {
      deadline = millis() + duty_s * 1000;
      sds_state = 2;
      while (is_SDS_running != true) {
        start_SDS();
        delay(100);
      }
      Serial.print(F("started SDS011, is running = "));
      Serial.println(is_SDS_running);

      sds011.on_query_data_auto_completed([](int n) {
        Serial.println(F("Begin Handling SDS011 query data"));
        Serial.print(F("n = "));
        Serial.println(n);
        int pm25;
        int pm10;
        if (sds011.filter_data(n, pm25_table, pm10_table, pm25, pm10) &&
            !isnan(pm10) && !isnan(pm25)) {

          rssi = WiFi.RSSI();

          if (flashing_finished && (pm10 != flash_val)) {
            flash_val = pm10;
            flashing_finished = false;
          }

          Serial.print(F("PM10: "));
          Serial.println(float(pm10) / 10);
          Serial.print(F("PM2.5: "));
          Serial.println(float(pm25) / 10);
          Serial.print("RSSI\t");
          Serial.println(rssi);

          Serial.println("sending to MQTT...");
          dtostrf(rssi, 4, 0, rssi_str);
          dtostrf(float(pm10) / 10, 4, 0, pm10_str);
          dtostrf(float(pm25) / 10, 4, 0, pm25_str);

          client.publish(MQTT_TOPIC_RSSI, rssi_str, true);
          client.publish(MQTT_TOPIC_PM10, pm10_str, true);
          client.publish(MQTT_TOPIC_PM25, pm25_str, true);
        }
        Serial.println(F("End Handling SDS011 query data"));
      });

      if (!sds011.query_data_auto_async(pm_tablesize, pm25_table, pm10_table)) {
        Serial.println(F("measurement capture start failed"));
      }
      Serial.print("wait s ");
      Serial.println(duty_s);
    }

  } else if (sds_state == 2) {
    // enter stopped state
    if (static_cast<int32_t>(deadline - millis()) < 0) {
      deadline = millis() + down_s * 1000;
      while (is_SDS_running != false) {
        stop_SDS();
        delay(100);
      }
      sds_state = 1;
      Serial.print(F("stopped SDS011, is running = "));
      Serial.println(is_SDS_running);
      Serial.print("wait s ");
      Serial.println(down_s);
    }
  } else {
    Serial.println("stuck in a very strange state");
  }
}

void start_SDS() {
  Serial.println(F("Start wakeup SDS011"));
  if (sds011.set_sleep(false)) {
    is_SDS_running = true;
  }
  Serial.println(F("End wakeup SDS011"));
}
void stop_SDS() {
  Serial.println(F("Start sleep SDS011"));
  if (sds011.set_sleep(true)) {
    is_SDS_running = false;
  }
  Serial.println(F("End sleep SDS011"));
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
