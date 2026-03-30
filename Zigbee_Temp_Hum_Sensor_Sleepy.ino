// =============================================================================
//  Zigbee Czujnik Temperatury i Wilgotności z Deep Sleep
//  Zigbee Temperature & Humidity Sensor with Deep Sleep
// =============================================================================
//
//  Wersja / Version: 1.0
//  Autor / Author:   Maciej Sikorski
//  Data / Date:      30.03.2026
//
//  Płytka / Board:   Seeed Studio XIAO ESP32C6
//
//  Czujniki / Sensors:
//    - SHT3x  — temperatura i wilgotność / temperature and humidity
//    - INA226 — napięcie i prąd baterii LiPo / LiPo battery voltage and current
//
//  Opis / Description:
//    Urządzenie Zigbee End Device z trybem głębokiego uśpienia (deep sleep).
//    Co 10 minut budzi się, mierzy temperaturę, wilgotność i napięcie baterii,
//    wysyła dane przez sieć Zigbee, po czym ponownie zasypia.
//
//    A Zigbee End Device with deep sleep mode. Every 10 minutes the device
//    wakes up, measures temperature, humidity and battery voltage, transmits
//    data over Zigbee network, then goes back to sleep.
//
// =============================================================================
//  MODYFIKACJA ORYGINALNEGO PRZYKŁADU / MODIFICATION OF ORIGINAL EXAMPLE
// =============================================================================
//
//  Ten plik powstał na podstawie przykładu dostarczonego przez
//  Espressif Systems (Shanghai) PTE LTD, objętego licencją Apache 2.0.
//  Oryginalny nagłówek licencyjny zachowano poniżej zgodnie z wymogami licencji.
//
//  This file is based on an example provided by
//  Espressif Systems (Shanghai) PTE LTD, covered by the Apache 2.0 license.
//  The original license header is preserved below as required by the license.
//
// =============================================================================

// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// =============================================================================

#ifndef ZIGBEE_MODE_ED
#error "Tryb Zigbee End Device nie jest wybrany w Tools->Zigbee mode"
// "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <Adafruit_INA219.h>
#include <nvs_flash.h>

// -----------------------------------------------------------------------------
// Konfiguracja pinów i stałych czasowych
// Pin and timing configuration
// -----------------------------------------------------------------------------
#define TEMP_SENSOR_ENDPOINT_NUMBER 10
#define LED_PIN                     15
#define BOOT_PIN_NUM                BOOT_PIN
#define uS_TO_S_FACTOR              1000000ULL
#define TIME_TO_SLEEP               600   // czas uśpienia w sekundach / sleep time in seconds (10 min)
#define REPORT_TIMEOUT              6000  // maks. czas oczekiwania na potwierdzenie / max wait for report ACK [ms]

// -----------------------------------------------------------------------------
// Granice napięcia baterii LiPo
// LiPo battery voltage thresholds
// -----------------------------------------------------------------------------
#define LIPO_MAX_VOLTAGE  4.20f   // 100% naładowania / 100% charge
#define LIPO_MIN_VOLTAGE  3.00f   //   0% naładowania /   0% charge

// -----------------------------------------------------------------------------
// Zmienne globalne
// Global variables
// -----------------------------------------------------------------------------
uint8_t button = BOOT_PIN_NUM;

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
Adafruit_SHT31   sht31        = Adafruit_SHT31();
Adafruit_INA219  ina219;

uint8_t dataToSend         = 3;     // liczba oczekiwanych potwierdzeń / expected ACK count
bool    resend             = false; // flaga ponownego wysłania / resend flag
bool    measurementStarted = false; // pomiar uruchomiony / measurement started
bool    goToSleep          = false; // gotowość do uśpienia / ready to sleep

unsigned long lastLedToggle = 0;
bool ledState = false;

// Zmienne przechowywane w pamięci RTC (przeżywają deep sleep)
// Variables stored in RTC memory (survive deep sleep)
RTC_DATA_ATTR int zigbeeFailCount = 0; // licznik błędów Zigbee / Zigbee failure counter

// Watchdog dla Zigbee.begin() — zabezpieczenie przed zawieszeniem
// Watchdog for Zigbee.begin() — protection against hang
#define ZIGBEE_BEGIN_TIMEOUT_MS  20000
volatile bool zigbeeBeginDone = false;

// -----------------------------------------------------------------------------
//  Zadanie watchdog dla Zigbee.begin()
//  Watchdog task for Zigbee.begin()
//
//  Jeśli Zigbee.begin() nie zakończy się w ciągu ZIGBEE_BEGIN_TIMEOUT_MS,
//  urządzenie zostaje zrestartowane.
//  If Zigbee.begin() does not finish within ZIGBEE_BEGIN_TIMEOUT_MS,
//  the device is restarted.
// -----------------------------------------------------------------------------
static void zigbeeWatchdogTask(void *arg) {
  unsigned long start = millis();
  while (!zigbeeBeginDone) {
    if ((millis() - start) > ZIGBEE_BEGIN_TIMEOUT_MS) {
      Serial.println("WATCHDOG: Zigbee.begin() zawieszone! Restartuję... / hung! Restarting...");
      delay(200);
      ESP.restart();
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

// -----------------------------------------------------------------------------
//  Przelicz napięcie LiPo [V] na procenty naładowania [0–100]
//  Convert LiPo voltage [V] to battery percentage [0–100]
//
//  Używa odcinkowej interpolacji liniowej dopasowanej do krzywej rozładowania.
//  Uses piecewise linear interpolation fitted to the discharge curve.
// -----------------------------------------------------------------------------
uint8_t lipoVoltageToPercent(float voltage) {
  if (voltage >= LIPO_MAX_VOLTAGE) return 100;
  if (voltage <= LIPO_MIN_VOLTAGE) return 0;

  struct { float vHigh; float vLow; uint8_t pHigh; uint8_t pLow; } segments[] = {
    { 4.20f, 4.00f, 100, 75 },
    { 4.00f, 3.80f,  75, 50 },
    { 3.80f, 3.60f,  50, 25 },
    { 3.60f, 3.00f,  25,  0 },
  };

  for (auto &seg : segments) {
    if (voltage <= seg.vHigh && voltage > seg.vLow) {
      float ratio = (voltage - seg.vLow) / (seg.vHigh - seg.vLow);
      return (uint8_t)(seg.pLow + ratio * (seg.pHigh - seg.pLow) + 0.5f);
    }
  }
  return 0;
}

// -----------------------------------------------------------------------------
//  Odczyt napięcia szyny z INA226
//  Read bus voltage from INA226
// -----------------------------------------------------------------------------
float readBatteryVoltage() {
  float busVoltage_V = ina219.getBusVoltage_V();
  Serial.printf("INA226 napięcie szyny / bus voltage: %.3f V\r\n", busVoltage_V);
  return busVoltage_V;
}

// =============================================================================
//  Callback globalnej odpowiedzi Zigbee
//  Zigbee global default response callback
// =============================================================================
void onGlobalResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status,
                      uint8_t endpoint, uint16_t cluster) {
  Serial.printf("Zigbee odpowiedź / response: %s, endpoint: %d, cluster: 0x%04X\r\n",
                esp_zb_zcl_status_to_name(status), endpoint, cluster);

  if ((command == ZB_CMD_REPORT_ATTRIBUTE) && (endpoint == TEMP_SENSOR_ENDPOINT_NUMBER)) {
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
      if (dataToSend > 0) dataToSend--;  // potwierdzenie odebrane / ACK received
    } else {
      resend = true;  // błąd — ustaw flagę ponownego wysłania / error — set resend flag
    }
  }
}

// =============================================================================
//  Zadanie FreeRTOS: pomiar i przejście do deep sleep
//  FreeRTOS task: measurement and deep sleep
// =============================================================================
static void measureAndSleep(void *arg) {

  // ── 1. Odczyt temperatury i wilgotności z SHT3x ──
  // ── 1. Read temperature and humidity from SHT3x ──
  float temperature = sht31.readTemperature();
  float humidity    = sht31.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("BŁĄD / ERROR: odczyt SHT3x nieudany! Używam wartości domyślnych. / SHT3x read failed! Using default values.");
    temperature = 0.0f;
    humidity    = 0.0f;
  }

  // ── 2. Odczyt napięcia baterii i przeliczenie na procenty ──
  // ── 2. Read battery voltage and convert to percentage ──
  float   batteryVoltage = readBatteryVoltage();
  uint8_t batteryPercent = lipoVoltageToPercent(batteryVoltage);
  Serial.printf("Bateria / Battery: %.3f V  →  %u %%\r\n", batteryVoltage, batteryPercent);

  // ── 3. Ustaw wartości w klastrach Zigbee ──
  // ── 3. Set values in Zigbee clusters ──
  zbTempSensor.setTemperature(temperature);
  zbTempSensor.setHumidity(humidity);
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, batteryPercent);

  Serial.printf("Raport Zigbee / Zigbee report: %.2f°C, %.2f%%, batt: %u%% (%.2fV)\r\n",
                temperature, humidity, batteryPercent, batteryVoltage);

  // ── 4. Wysłanie raportu Zigbee i oczekiwanie na potwierdzenie ──
  // ── 4. Send Zigbee report and wait for acknowledgement ──
  zbTempSensor.report();

  unsigned long startTime = millis();
  while (dataToSend != 0 && (millis() - startTime) < REPORT_TIMEOUT) {
    if (resend) {
      resend     = false;
      dataToSend = 3;
      zbTempSensor.report();
      startTime = millis();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  Serial.println("Wysyłanie zakończone lub timeout. Zasypiam... / Sending done or timeout. Going to sleep...");
  goToSleep = true;
  vTaskDelete(NULL);
}

// =============================================================================
//  Sprawdzenie przycisku BOOT — długie naciśnięcie (>3s) = factory reset
//  BOOT button check — long press (>3s) = factory reset
// =============================================================================
void checkFactoryReset() {
  if (digitalRead(button) == LOW) {
    delay(100);
    unsigned long startTime = millis();
    while (digitalRead(button) == LOW) {
      // Miga diodą co 200 ms podczas trzymania przycisku
      // Blink LED every 200 ms while button is held
      bool blink = ((millis() - startTime) / 200) % 2;
      digitalWrite(LED_PIN, blink ? HIGH : LOW);
      delay(50);

      if ((millis() - startTime) > 3000) {
        Serial.println("Factory reset! Czyszczę dane i restartuję... / Clearing data and restarting...");
        digitalWrite(LED_PIN, LOW);
        delay(200);
        Zigbee.factoryReset();
        esp_restart();
      }
    }
    // Przycisk zwolniony przed upływem 3 s — anuluj
    // Button released before 3 s — cancel
    ledState = false;
    lastLedToggle = millis();
    digitalWrite(LED_PIN, LOW);
  }
}

// =============================================================================
//  Nieblokująca obsługa diody LED
//  Non-blocking LED control
//
//  Miga (500 ms) gdy brak połączenia Zigbee, gaśnie po połączeniu.
//  Blinks (500 ms) when not connected to Zigbee, turns off after connection.
// =============================================================================
void updateLed() {
  unsigned long now = millis();
  if (!Zigbee.connected()) {
    if (now - lastLedToggle >= 500) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  } else {
    if (ledState) {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
    }
  }
}

// =============================================================================
//  Setup — inicjalizacja sprzętu i stosu Zigbee
//  Setup — hardware and Zigbee stack initialization
// =============================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Inicjalizacja czujnika temperatury i wilgotności SHT3x
  // Initialize SHT3x temperature and humidity sensor
  if (!sht31.begin(0x44)) {
    Serial.println("BŁĄD / ERROR: Nie znaleziono SHT3x! Sprawdź podłączenie. / SHT3x not found! Check wiring.");
  } else {
    Serial.println("SHT3x OK.");
  }

  // Inicjalizacja monitora baterii INA226
  // Initialize INA226 battery monitor
  if (!ina219.begin()) {
    Serial.println("BŁĄD / ERROR: Nie znaleziono INA226! Sprawdź podłączenie. / INA226 not found! Check wiring.");
  } else {
    ina219.setCalibration_16V_400mA();
    Serial.println("INA226 OK (zakres / range 16V/400mA).");
  }

  // Konfiguracja pinów GPIO
  // GPIO pin configuration
  pinMode(button, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Konfiguracja budzenia przez timer (deep sleep)
  // Configure timer wakeup (deep sleep)
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Konfiguracja endpointu Zigbee
  // Zigbee endpoint configuration
  zbTempSensor.setManufacturerAndModel("Espressif", "SleepyZigbeeSensor");
  zbTempSensor.setMinMaxValue(10, 50);
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100);
  zbTempSensor.addHumiditySensor(0, 100, 1, 0.0);

  Zigbee.onGlobalDefaultResponse(onGlobalResponse);
  Zigbee.addEndpoint(&zbTempSensor);

  // Konfiguracja sieci Zigbee (End Device)
  // Zigbee network configuration (End Device)
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;
  Zigbee.setTimeout(15000);

  // Odliczanie przed startem — pozwala na wgranie nowego firmware
  // Countdown before start — allows uploading new firmware
  Serial.println("\n>>> START SYSTEMU / SYSTEM START: Oczekiwanie 5 s przed Zigbee...");
  for (int i = 5; i > 0; i--) {
    Serial.printf("Start za / in: %d s\r\n", i);
    digitalWrite(LED_PIN, LOW);  delay(500);
    digitalWrite(LED_PIN, HIGH); delay(500);
  }
  digitalWrite(LED_PIN, LOW);

  Serial.printf("Uruchamianie Zigbee... / Starting Zigbee... (próba / attempt %d)\r\n", zigbeeFailCount + 1);

  // Uruchomienie watchdog i stosu Zigbee
  // Start watchdog and Zigbee stack
  zigbeeBeginDone = false;
  xTaskCreate(zigbeeWatchdogTask, "zb_watchdog", 2048, NULL, 10, NULL);

  bool zbOk = Zigbee.begin(&zigbeeConfig, false);
  zigbeeBeginDone = true;

  if (!zbOk) {
    zigbeeFailCount++;
    Serial.printf("Zigbee start NIEUDANY / FAILED! Próba / Attempt %d.\r\n", zigbeeFailCount);
    if (zigbeeFailCount >= 5) {
      // Po 5 błędach z rzędu — wyczyść NVS i restartuj
      // After 5 consecutive failures — erase NVS and restart
      Serial.println("5 błędów z rzędu — czyszczę NVS... / 5 consecutive failures — erasing NVS...");
      zigbeeFailCount = 0;
      nvs_flash_erase();
      nvs_flash_init();
    }
    delay(1000);
    ESP.restart();
  }

  zigbeeFailCount = 0;
  Serial.println("Zigbee OK. Czekam na sieć... / Waiting for network...");
}

// =============================================================================
//  Główna pętla
//  Main loop
// =============================================================================
void loop() {
  checkFactoryReset(); // sprawdź przycisk factory reset / check factory reset button
  updateLed();         // zaktualizuj stan diody LED / update LED state

  // Po uzyskaniu połączenia uruchom jednorazowo zadanie pomiaru
  // After connection is established, start the measurement task once
  if (Zigbee.connected() && !measurementStarted) {
    Serial.println("\nPołączono z siecią! / Connected to network! Uruchamiam pomiar. / Starting measurement.");
    measurementStarted = true;
    xTaskCreate(measureAndSleep, "temp_update", 4096, NULL, 5, NULL);
  }

  // Jeśli zadanie pomiaru zakończyło pracę — wejdź w deep sleep
  // If measurement task is done — enter deep sleep
  if (goToSleep) {
    Serial.println("Wchodzę w deep sleep... / Entering deep sleep...");
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW);  delay(50);
    esp_deep_sleep_start();
  }

  delay(20);
}
