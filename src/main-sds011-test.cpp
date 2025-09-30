/**
 * sds011 test
 * testing particulate sensor, sending measurements via serial
 */

// secrets
#include <secrets.h>

// arduino
#include "Arduino.h"

// sds011
#include <SoftwareSerial.h>
#include <esp_sds011.h>
#define SDS_PIN_RX 3 // Remap to GPIO 8 (SDA)
#define SDS_PIN_TX 4 // Remap to GPIO 9 (SCL)
EspSoftwareSerial::UART serialSDS;
Sds011Async<EspSoftwareSerial::UART> sds011(serialSDS);
constexpr int pm_tablesize = 20;
int pm25_table[pm_tablesize];
int pm10_table[pm_tablesize];
bool is_SDS_running = true;

void setup_SDS();
void start_SDS();
void stop_SDS();

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // setup sds011
  setup_SDS();
}

void loop() {
  // Start by stopping the sensor
  Serial.println("stopping sensor");
  constexpr uint32_t down_s = 90;

  stop_SDS();
  Serial.print(F("stopped SDS011 (is running = "));
  Serial.print(is_SDS_running);
  Serial.println(')');

  // wait a bit ?
  uint32_t deadline = millis() + down_s * 1000;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
    delay(1000);
    Serial.println(static_cast<int32_t>(deadline - millis()) / 1000);
    sds011.perform_work();
  }

  // start ?
  constexpr uint32_t duty_s = 30;
  start_SDS();
  Serial.print(F("started SDS011 (is running = "));
  Serial.print(is_SDS_running);
  Serial.println(')');

  // callback on query data
  sds011.on_query_data_auto_completed([](int n) {
    Serial.println(F("Begin Handling SDS011 query data"));
    int pm25;
    int pm10;
    Serial.print(F("n = "));
    Serial.println(n);
    if (sds011.filter_data(n, pm25_table, pm10_table, pm25, pm10) &&
        !isnan(pm10) && !isnan(pm25)) {
      Serial.print(F("PM10: "));
      Serial.println(float(pm10) / 10);
      float PM25 = 0;
      float PM10 = 0;
      // PM10 = float(pm10) / 10;
      // PM25 = float(pm25) / 10;
      Serial.print(F("PM2.5: "));
      Serial.println(float(pm25) / 10);

      // delay(5000);
    }
  });
}

void setup_SDS() {
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
}

void start_SDS() {
  // Serial.println(F("Start wakeup SDS011"));

  if (sds011.set_sleep(false)) {
    is_SDS_running = true;
  }

  // Serial.println(F("End wakeup SDS011"));
}

void stop_SDS() {
  // Serial.println(F("Start sleep SDS011"));

  if (sds011.set_sleep(true)) {
    is_SDS_running = false;
  }

  // Serial.println(F("End sleep SDS011"));
}