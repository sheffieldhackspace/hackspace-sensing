/**
 * particulate monitor test
 * contains bug: if fan does not spin up immediately on reset, restart until it
 * does (gets stuck in loop trying to disable fan when it is already off)
 */

#include <SoftwareSerial.h>
#include <esp_sds011.h>

#define SDS_PIN_RX 3 // Remap to GPIO 8 (SDA)
#define SDS_PIN_TX 4 // Remap to GPIO 9 (SCL)

EspSoftwareSerial::UART serialSDS;
Sds011Async<EspSoftwareSerial::UART> sds011(serialSDS);

// The example stops the sensor for 210s, then runs it for 30s, then repeats.
// At tablesizes 20 and below, the tables get filled during duty cycle
// and then measurement completes.
// At tablesizes above 20, the tables do not get completely filled
// during the 30s total runtime, and the rampup / 4 timeout trips,
// thus completing the measurement at whatever number of data points
// were recorded in the tables.
constexpr int pm_tablesize = 20;
int pm25_table[pm_tablesize];
int pm10_table[pm_tablesize];

constexpr uint32_t down_s = 30; // waiting (fan off) time, default 210
constexpr uint32_t duty_s = 30; // measuring (fan on) time, default 30
uint32_t deadline;
uint32_t lastSerialTimerMsg = 0;
int sds_state; // 1 - waiting (fan off), 2 - measuring (fan on)
bool is_SDS_running = true;

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

// The setup() function runs once each time the micro-controller starts
void setup() {
  Serial.begin(115200);

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
      Serial.print(F("PM2.5: "));
      Serial.println(float(pm25) / 10);
    }
    Serial.println(F("End Handling SDS011 query data"));
  });

  deadline = millis() + duty_s * 1000;
  sds_state = 2;
}

// Add the main program code into the continuous loop() function
void loop() {
  // Per manufacturer specification, place the sensor in standby to prolong
  // service life. At an user-determined interval (here 210s down plus 30s duty
  // = 4m), run the sensor for 30s. Quick response time is given as 10s by the
  // manufacturer, thus the library drops the measurements obtained during the
  // first 10s of each run.

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
