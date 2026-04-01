// =============================================================================
//  Zigbee Czujnik Temperatury i Wilgotności z Deep Sleep
//  Zigbee Temperature & Humidity Sensor with Deep Sleep
// =============================================================================
//
//  Wersja / Version: 2.5
//  Autor / Author:   Maciej Sikorski
//  Data / Date:      01.04.2026
//
//  Płytka / Board:   Seeed Studio XIAO ESP32C6
//
//  Czujniki / Sensors:
//    - SHT3x  — temperatura i wilgotność / temperature and humidity
//    - INA226 — napięcie i prąd baterii LiPo / LiPo battery voltage and current
//
//  Zmiany v2.5 / Changes v2.5:
//    - Przywrócono aktywną obsługę przycisku w loop() – factory reset działa też,
//      gdy urządzenie nie śpi (np. podczas oczekiwania na połączenie).
//    - Zachowano wybudzanie GPIO z deep sleep – przycisk działa w każdym stanie.
//    - Dodano antyodbicie (debounce) dla przycisku.
//    - Ujednolicono logowanie factory reset.
//    - Zastosowano bibliotekę INA226_WE zamiast Adafruit_INA219 dla poprawy stabilności po deep sleep.
//    - Dodano reset I2C i opóźnienie po wybudzeniu, aby uniknąć crash.
//    - Poprawiono inicjalizację INA226 – użyto sprawdzonych metod z oryginalnego kodu.
//
//    - Restored active button handling in loop() – factory reset works even
//      when device is awake (e.g., waiting for connection).
//    - Kept GPIO wakeup from deep sleep – button works in any state.
//    - Added debounce for button.
//    - Unified factory reset logging.
//    - Replaced Adafruit_INA219 with INA226_WE library for better stability after deep sleep.
//    - Added I2C reset and delay after wake-up to prevent crash.
//    - Fixed INA226 initialization – using proven methods from original code.
//
// =============================================================================
//  MODYFIKACJA ORYGINALNEGO PRZYKŁADU / MODIFICATION OF ORIGINAL EXAMPLE
// =============================================================================
//
//  Ten plik powstał na podstawie przykładu dostarczonego przez
//  Espressif Systems (Shanghai) PTE LTD, objętego licencją Apache 2.0.
//  This file is based on an example by Espressif Systems (Shanghai) PTE LTD,
//  covered by the Apache 2.0 license.
//
// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
// Licensed under the Apache License, Version 2.0
// http://www.apache.org/licenses/LICENSE-2.0
// =============================================================================

#ifndef ZIGBEE_MODE_ED
#error "Tryb Zigbee End Device nie jest wybrany w Tools->Zigbee mode"
// "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <INA226_WE.h>           // ZMIANA: użyto dedykowanej biblioteki dla INA226
#include <nvs_flash.h>
#include <esp_wifi.h>      // esp_wifi_stop() przed sleep / before sleep
#include <esp_bt.h>        // esp_bt_controller_disable() przed sleep / before sleep

// =============================================================================
//  Konfiguracja logowania / Logging configuration
//
//  DEBUG_ENABLED 1 → wszystkie logi (development)
//  DEBUG_ENABLED 0 → tylko WARN i ERROR (production, zero overhead)
// =============================================================================
#define DEBUG_ENABLED 0

#if DEBUG_ENABLED
  #define LOG_DEBUG(fmt, ...)  Serial.printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
  #define LOG_INFO(fmt, ...)   Serial.printf("[INFO]  " fmt "\r\n", ##__VA_ARGS__)
  #define LOG_PRINTLN(msg)     Serial.println(msg)
  #define LOG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...)  ((void)0)
  #define LOG_INFO(fmt, ...)   ((void)0)
  #define LOG_PRINTLN(msg)     ((void)0)
  #define LOG_PRINTF(fmt, ...) ((void)0)
#endif

// WARN i ERROR logują zawsze — zdarzenia krytyczne produkcyjne
// WARN and ERROR always log — production-critical events
#define LOG_WARN(fmt, ...)   Serial.printf("[WARN]  " fmt "\r\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  Serial.printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)

// -----------------------------------------------------------------------------
// Konfiguracja pinów i stałych czasowych
// Pin and timing configuration
// -----------------------------------------------------------------------------
#define TEMP_SENSOR_ENDPOINT_NUMBER  10
#define LED_PIN                      15
#define BOOT_PIN_NUM                 0
#define uS_TO_S_FACTOR               1000000ULL
#define TIME_TO_SLEEP                600    // s (10 minut / 10 minutes)
#define REPORT_TIMEOUT               6000   // maks. czas na ACK / max wait for ACK [ms]

// -----------------------------------------------------------------------------
// Konfiguracja czujników
// Sensor configuration
// -----------------------------------------------------------------------------
#define SHT3X_I2C_ADDR          0x44
#define SHT3X_READ_TIMEOUT_MS   500    // maks. czas na odczyt / max read time [ms]
#define SHT3X_MAX_RETRIES       3      // liczba prób przed poddaniem się / attempts before giving up
#define SHT3X_RETRY_DELAY_MS    50     // opóźnienie między próbami / delay between retries [ms]
#define SHT3X_TEMP_MIN         -40.0f  // dolna granica zakresu / lower range limit [°C]
#define SHT3X_TEMP_MAX          85.0f  // górna granica zakresu / upper range limit [°C]
#define SHT3X_HUM_MIN           0.0f   // dolna granica wilgotności / lower humidity limit [%]
#define SHT3X_HUM_MAX           100.0f // górna granica wilgotności / upper humidity limit [%]

#define INA226_ADDR            0x40    // adres I2C czujnika INA226
#define SHUNT_RESISTANCE       0.1f    // rezystor bocznikowy [Ω]
#define INA226_MAX_RETRIES      3      // próby odczytu INA226 / INA226 read attempts
#define INA226_RETRY_DELAY_MS   50

// -----------------------------------------------------------------------------
// Granice napięcia baterii LiPo
// LiPo battery voltage thresholds
// -----------------------------------------------------------------------------
#define LIPO_MAX_VOLTAGE  4.20f   // 100%
#define LIPO_MIN_VOLTAGE  3.00f   //   0%

// -----------------------------------------------------------------------------
// Watchdog dla Zigbee.begin()
// Watchdog for Zigbee.begin()
// -----------------------------------------------------------------------------
#define ZIGBEE_BEGIN_TIMEOUT_MS    20000
#define ZIGBEE_CONNECT_TIMEOUT_MS  300000  // maks. czas oczekiwania na sieć / max wait for network [ms] (5 min)
#define ZIGBEE_REPORT_COUNT        3       // liczba atrybutów do potwierdzenia / number of attributes to ACK

// Flaga w pamięci RTC — przeżywa deep sleep, kasuje się przy twardym resecie
// Flag in RTC memory — survives deep sleep, cleared on hard reset
RTC_DATA_ATTR static bool factoryResetRequested = false;

// =============================================================================
//  Struktura stanu urządzenia (zastępuje luźne zmienne globalne)
//  Device state structure (replaces loose global variables)
// =============================================================================
struct DeviceState {
  uint8_t          dataToSend;         // oczekiwane ACK / expected ACKs
  bool             resend;             // flaga ponownego wysłania / resend flag
  bool             measurementStarted; // pomiar uruchomiony / measurement started
  volatile bool    goToSleep;          // gotowość do deep sleep / ready for deep sleep
  bool             shtAvailable;       // SHT3x inicjalizowany poprawnie / SHT3x initialized
  bool             inaAvailable;       // INA226 inicjalizowany poprawnie / INA226 initialized
  unsigned long    lastLedToggle;
  bool             ledState;
  unsigned long    zigbeeConnectStart; // czas startu oczekiwania na sieć / network wait start time
};

static DeviceState dev = {
  .dataToSend         = ZIGBEE_REPORT_COUNT,
  .resend             = false,
  .measurementStarted = false,
  .goToSleep          = false,
  .shtAvailable       = false,
  .inaAvailable       = false,
  .lastLedToggle      = 0,
  .ledState           = false,
  .zigbeeConnectStart = 0,
};

// Mutex chroniący goToSleep przed race condition między taskiem a loop()
// Mutex protecting goToSleep from race condition between task and loop()
static portMUX_TYPE sleepMux = portMUX_INITIALIZER_UNLOCKED;

// -----------------------------------------------------------------------------
// Obiekty sprzętu / Hardware objects
// -----------------------------------------------------------------------------
ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
Adafruit_SHT31   sht31        = Adafruit_SHT31();
INA226_WE        ina226       = INA226_WE(INA226_ADDR);   // ZMIANA: podajemy adres

uint8_t button = BOOT_PIN_NUM;

// Zmienne w pamięci RTC (przeżywają deep sleep)
// Variables in RTC memory (survive deep sleep)
RTC_DATA_ATTR int   zigbeeFailCount        = 0;
RTC_DATA_ATTR float lastGoodBatteryVoltage = 3.7f;
// Wartość 3.7V = nominalne napięcie środka zakresu LiPo (między 3.0V a 4.2V).
// Używana jako fallback gdy INA226 niedostępny lub wszystkie próby odczytu nieudane,
// zamiast 0.0V które koordynator Zigbee mógłby zinterpretować jako awarię zasilania.
// 3.7V = nominal mid-range LiPo voltage (between 3.0V and 4.2V).
// Used as fallback when INA226 is unavailable or all read attempts fail,
// instead of 0.0V which the Zigbee coordinator might interpret as a power failure.

volatile bool zigbeeBeginDone = false;

// =============================================================================
//  Pomocnik: bezpieczne ustawienie flagi goToSleep
//  Helper: thread-safe set of goToSleep flag
// =============================================================================
static void IRAM_ATTR setGoToSleep() {
  portENTER_CRITICAL(&sleepMux);
  dev.goToSleep = true;
  portEXIT_CRITICAL(&sleepMux);
}

static bool IRAM_ATTR checkGoToSleep() {
  bool val;
  portENTER_CRITICAL(&sleepMux);
  val = dev.goToSleep;
  portEXIT_CRITICAL(&sleepMux);
  return val;
}

// =============================================================================
//  Zadanie watchdog dla Zigbee.begin()
//  Watchdog task for Zigbee.begin()
// =============================================================================
static void zigbeeWatchdogTask(void *arg) {
  unsigned long start = millis();
  while (!zigbeeBeginDone) {
    if ((millis() - start) > ZIGBEE_BEGIN_TIMEOUT_MS) {
      LOG_WARN("WATCHDOG: Zigbee.begin() zawieszone! Restartuję... / hung! Restarting...");
      delay(200);
      ESP.restart();
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

// =============================================================================
//  Przelicz napięcie LiPo [V] na procenty [0–100]
//  Convert LiPo voltage [V] to battery percentage [0–100]
// =============================================================================
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

// =============================================================================
//  Odczyt napięcia baterii z INA226 (z retry i sprawdzeniem dostępności)
//  Read battery voltage from INA226 (with retry and availability check)
// =============================================================================
float readBatteryVoltage() {
  if (!dev.inaAvailable) {
    // INA226 nie zainicjalizowany — użyj ostatniej dobrej wartości z RTC
    // INA226 not initialized — use last known good value from RTC
    LOG_WARN("INA226 niedostępny / unavailable. Używam ostatniej dobrej wartości / last good value: %.3fV.", lastGoodBatteryVoltage);
    return lastGoodBatteryVoltage;
  }

  for (int attempt = 1; attempt <= INA226_MAX_RETRIES; attempt++) {
    float busVoltage_V = ina226.getBusVoltage_V();   // ZMIANA: odczyt z INA226_WE

    // Zakres 0.5–5.5V: dolna granica odrzuca zwarcia i błędy I2C (zwracają 0V),
    // górna granica z marginesem na przepięcia pomiarowe przy 1S LiPo (max 4.2V).
    // Celowo NIE obsługuje 2S LiPo (max 8.4V) — układ zasilany jest z 1S.
    // Range 0.5–5.5V: lower bound rejects shorts and I2C errors (return 0V),
    // upper bound with margin for measurement spikes on 1S LiPo (max 4.2V).
    // Intentionally does NOT support 2S LiPo (max 8.4V) — circuit runs on 1S.
    if (busVoltage_V > 0.5f && busVoltage_V < 5.5f) {
      LOG_DEBUG("INA226 napięcie szyny / bus voltage: %.3f V (próba / attempt %d)",
                    busVoltage_V, attempt);
      lastGoodBatteryVoltage = busVoltage_V;  // zapisz w RTC na wypadek awarii / save to RTC for fallback
      return busVoltage_V;
    }

    LOG_WARN("INA226 podejrzany odczyt / suspicious read: %.3f V (próba / attempt %d/%d)",
                  busVoltage_V, attempt, INA226_MAX_RETRIES);

    if (attempt < INA226_MAX_RETRIES) {
      vTaskDelay(INA226_RETRY_DELAY_MS / portTICK_PERIOD_MS);
    }
  }

  // Wszystkie próby nieudane — użyj ostatniej dobrej wartości z RTC
  // All attempts failed — use last known good value from RTC
  LOG_ERROR("INA226 — wszystkie próby nieudane / all attempts failed. Używam ostatniej dobrej wartości / last good value: %.3fV.", lastGoodBatteryVoltage);
  return lastGoodBatteryVoltage;
}

// =============================================================================
//  Odczyt SHT3x z retry i timeoutem
//  SHT3x read with retry and timeout
//
//  Zwraca true gdy odczyt udany.
//  Returns true when read was successful.
// =============================================================================
bool readSHT3x(float &temperature, float &humidity) {
  if (!dev.shtAvailable) {
    LOG_WARN("SHT3x niedostępny / unavailable.");
    temperature = 0.0f;
    humidity    = 0.0f;
    return false;
  }

  for (int attempt = 1; attempt <= SHT3X_MAX_RETRIES; attempt++) {
    unsigned long tStart = millis();

    // Oba odczyty w jednym bloku czasowym — T i H z tej samej próbki
    // Both reads in one timed block — T and H from the same sample
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    unsigned long elapsed = millis() - tStart;

    // Sprawdź timeout odczytu
    // Check read timeout
    if (elapsed > SHT3X_READ_TIMEOUT_MS) {
      LOG_WARN("SHT3x timeout odczytu / read timeout: %lu ms (próba / attempt %d/%d)",
                    elapsed, attempt, SHT3X_MAX_RETRIES);
      if (attempt < SHT3X_MAX_RETRIES) {
        vTaskDelay(SHT3X_RETRY_DELAY_MS / portTICK_PERIOD_MS);
        continue;
      }
      break;
    }

    // Sprawdź czy wartości są poprawne (nie NaN)
    // Check if values are valid (not NaN)
    if (isnan(t) || isnan(h)) {
      LOG_WARN("SHT3x NaN (próba / attempt %d/%d)", attempt, SHT3X_MAX_RETRIES);
      if (attempt < SHT3X_MAX_RETRIES) {
        vTaskDelay(SHT3X_RETRY_DELAY_MS / portTICK_PERIOD_MS);
        continue;
      }
      break;
    }

    // Walidacja zakresu fizycznego czujnika
    // Physical sensor range validation
    if (t < SHT3X_TEMP_MIN || t > SHT3X_TEMP_MAX) {
      LOG_WARN("Temperatura poza zakresem / out of range: %.2f°C (zakres / range: %.0f–%.0f°C). Próba %d/%d",
                    t, SHT3X_TEMP_MIN, SHT3X_TEMP_MAX, attempt, SHT3X_MAX_RETRIES);
      if (attempt < SHT3X_MAX_RETRIES) {
        vTaskDelay(SHT3X_RETRY_DELAY_MS / portTICK_PERIOD_MS);
        continue;
      }
      break;
    }

    if (h < SHT3X_HUM_MIN || h > SHT3X_HUM_MAX) {
      LOG_WARN("Wilgotność poza zakresem / out of range: %.2f%% (próba %d/%d)",
                    h, attempt, SHT3X_MAX_RETRIES);
      if (attempt < SHT3X_MAX_RETRIES) {
        vTaskDelay(SHT3X_RETRY_DELAY_MS / portTICK_PERIOD_MS);
        continue;
      }
      break;
    }

    // Odczyt udany / Successful read
    temperature = t;
    humidity    = h;
    LOG_DEBUG("SHT3x OK: %.2f°C, %.2f%% (próba / attempt %d)", t, h, attempt);
    return true;
  }

  // Wszystkie próby nieudane
  // All attempts failed
  LOG_ERROR("SHT3x — wszystkie próby nieudane / all attempts failed. Używam 0.0.");
  temperature = 0.0f;
  humidity    = 0.0f;
  return false;
}

// =============================================================================
//  Wyłącz zbędne radia przed deep sleep (WiFi i BLE)
//  Disable unused radios before deep sleep (WiFi and BLE)
//
//  Na ESP32C6 z aktywnym Zigbee — WiFi/BLE mogą pobierać prąd jeśli
//  nie są explicite wyłączone.
//  On ESP32C6 with active Zigbee — WiFi/BLE may draw current unless
//  explicitly disabled.
// =============================================================================
static void disableUnusedRadios() {
  // Zatrzymaj WiFi jeśli był uruchomiony
  // Stop WiFi if it was running
  esp_err_t ret = esp_wifi_stop();
  if (ret == ESP_OK) {
    LOG_INFO("WiFi zatrzymany / stopped.");
  } else if (ret != ESP_ERR_WIFI_NOT_INIT) {
    // ESP_ERR_WIFI_NOT_INIT oznacza "nigdy nie był uruchomiony" — to OK
    // ESP_ERR_WIFI_NOT_INIT means "never started" — that's fine
    LOG_WARN("esp_wifi_stop() błąd / error: 0x%x", ret);
  }

  // Zatrzymaj BT controller jeśli aktywny
  // Stop BT controller if active
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
    ret = esp_bt_controller_disable();
    if (ret == ESP_OK) {
      LOG_INFO("BT controller wyłączony / disabled.");
    } else {
      LOG_WARN("esp_bt_controller_disable() błąd / error: 0x%x", ret);
    }
  }
}

// =============================================================================
//  Callback globalnej odpowiedzi Zigbee
//  Zigbee global default response callback
// =============================================================================
void onGlobalResponse(zb_cmd_type_t command, esp_zb_zcl_status_t status,
                      uint8_t endpoint, uint16_t cluster) {
  LOG_DEBUG("Zigbee odpowiedź / response: %s, ep: %d, cluster: 0x%04X",
                esp_zb_zcl_status_to_name(status), endpoint, cluster);

  if ((command == ZB_CMD_REPORT_ATTRIBUTE) && (endpoint == TEMP_SENSOR_ENDPOINT_NUMBER)) {
    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
      if (dev.dataToSend > 0) dev.dataToSend--;
    } else {
      dev.resend = true;
    }
  }
}

// =============================================================================
//  Zadanie FreeRTOS: pomiar i przejście do deep sleep
//  FreeRTOS task: measurement and deep sleep
// =============================================================================
static void measureAndSleep(void *arg) {

  // ── DODANE: krótkie opóźnienie po wybudzeniu z deep sleep, aby I2C i czujniki zdążyły się ustabilizować ──
  // ── ADDED: short delay after deep sleep wake-up to allow I2C and sensors to stabilize ──
  vTaskDelay(100 / portTICK_PERIOD_MS);

  // ── 1. Odczyt temperatury i wilgotności z SHT3x (retry + timeout + walidacja) ──
  // ── 1. Read temperature & humidity from SHT3x (retry + timeout + validation) ──
  float temperature = 0.0f;
  float humidity    = 0.0f;
  bool  shtOk       = readSHT3x(temperature, humidity);

  if (!shtOk) {
    // Kontynuujemy — wyślemy dane z wartościami domyślnymi, zapis błędu już nastąpił
    // Continue — send with default values, error was already logged
    LOG_WARN("Wysyłam dane z domyślnymi wartościami SHT3x / Sending with SHT3x defaults.");
  }

  // ── 2. Odczyt napięcia baterii (z retry i sprawdzeniem dostępności INA226) ──
  // ── 2. Read battery voltage (with retry and INA226 availability check) ──
  float   batteryVoltage = readBatteryVoltage();
  uint8_t batteryPercent = lipoVoltageToPercent(batteryVoltage);
  LOG_DEBUG("Bateria / Battery: %.3f V  →  %u %%", batteryVoltage, batteryPercent);

  // ── 3. Ustaw wartości w klastrach Zigbee ──
  // ── 3. Set values in Zigbee clusters ──
  zbTempSensor.setTemperature(temperature);
  zbTempSensor.setHumidity(humidity);
  zbTempSensor.setBatteryPercentage(batteryPercent);
  zbTempSensor.reportBatteryPercentage(); 

  LOG_INFO("Raport Zigbee / Zigbee report: %.2f°C, %.2f%%, batt: %u%% (%.2fV)",
                temperature, humidity, batteryPercent, batteryVoltage);

  // ── 4. Wysłanie raportu Zigbee i oczekiwanie na potwierdzenie ──
  // ── 4. Send Zigbee report and wait for acknowledgement ──
  zbTempSensor.report();

  unsigned long startTime = millis();
  while (dev.dataToSend != 0 && (millis() - startTime) < REPORT_TIMEOUT) {
    if (dev.resend) {
      dev.resend     = false;
      dev.dataToSend = ZIGBEE_REPORT_COUNT;
      zbTempSensor.report();
      startTime = millis();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  if (dev.dataToSend != 0) {
    LOG_WARN("Timeout ACK — nie otrzymano %d potwierdzeń / %d ACKs not received.",
                  dev.dataToSend, dev.dataToSend);
  } else {
    LOG_INFO("Wszystkie potwierdzenia odebrane / All ACKs received.");
  }

  LOG_INFO("Wysyłanie zakończone. Zasypiam... / Sending done. Going to sleep...");

  // Thread-safe ustawienie flagi / Thread-safe flag set
  setGoToSleep();
  vTaskDelete(NULL);
}

// =============================================================================
//  Nieblokująca obsługa diody LED
//  Non-blocking LED control
// =============================================================================
void updateLed() {
  unsigned long now = millis();
  if (!Zigbee.connected()) {
    if (now - dev.lastLedToggle >= 500) {
      dev.lastLedToggle = now;
      dev.ledState      = !dev.ledState;
      digitalWrite(LED_PIN, dev.ledState ? HIGH : LOW);
    }
  } else {
    if (dev.ledState) {
      dev.ledState = false;
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

  // Inicjalizacja SHT3x
  // Initialize SHT3x
  if (!sht31.begin(SHT3X_I2C_ADDR)) {
    LOG_ERROR("SHT3x nie znaleziony! / SHT3x not found! Sprawdź podłączenie / Check wiring.");
    dev.shtAvailable = false;
  } else {
    LOG_INFO("SHT3x OK.");
    dev.shtAvailable = true;
  }

  // Inicjalizacja INA226
  // Initialize INA226
  if (!ina226.init()) {
    LOG_ERROR("INA226 nie znaleziony! / INA226 not found! Sprawdź podłączenie / Check wiring.");
    dev.inaAvailable = false;
  } else {
    // Ustawienia kalibracji i konwersji – zgodne z oryginalnym kodem
    ina226.setResistorRange(SHUNT_RESISTANCE, 500.0f);
    ina226.setAverage(INA226_AVERAGE_16);
    ina226.setConversionTime(INA226_CONV_TIME_1100, INA226_CONV_TIME_1100);
    ina226.setMeasureMode(INA226_CONTINUOUS);
    LOG_INFO("INA226 OK (zakres / range 16V/400mA).");
    dev.inaAvailable = true;
  }

  // Zatrzymaj WiFi i BLE — nie są potrzebne dla Zigbee ED
  // Disable WiFi and BLE — not needed for Zigbee ED
  disableUnusedRadios();

  // GPIO
  pinMode(button, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Timer deep sleep
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Wybudzenie przez przycisk (GPIO0) — działa również podczas deep sleep
  // Button wakeup (GPIO0) — works even during deep sleep
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BOOT_PIN_NUM, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Konfiguracja endpointu Zigbee
  // Zigbee endpoint configuration
  zbTempSensor.setManufacturerAndModel("Espressif", "SleepyZigbeeTempSensor");
  zbTempSensor.setMinMaxValue(SHT3X_TEMP_MIN, SHT3X_TEMP_MAX);
  zbTempSensor.setTolerance(0.3f);                        // ← przywrócone
  zbTempSensor.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100); // ← trzeci argument (napięcie nominalne baterii)
  zbTempSensor.addHumiditySensor(SHT3X_HUM_MIN, SHT3X_HUM_MAX, 1);

  Zigbee.onGlobalDefaultResponse(onGlobalResponse);
  Zigbee.addEndpoint(&zbTempSensor);

  // Konfiguracja sieci Zigbee (End Device)
  // Zigbee network configuration (End Device)
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;
  Zigbee.setTimeout(15000);

  // Odliczanie przed startem tylko przy pierwszym uruchomieniu (nie po deep sleep)
  // Countdown only on first boot, skip after deep sleep wake-up
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

  // Wybudzenie przyciskiem — ustaw flagę w RTC (przeżyje restart po Zigbee.begin)
  // Button wakeup — set RTC flag (survives restart after Zigbee.begin)
  if (wakeupCause == ESP_SLEEP_WAKEUP_GPIO) {
    factoryResetRequested = true;
    LOG_WARN("Wybudzenie przez przycisk GPIO0 — factory reset zlecony / Button GPIO0 wakeup — factory reset requested.");
  }

  // DODANE: po wybudzeniu z deep sleep (GPIO lub timer) resetuję I2C, aby uniknąć problemów z czujnikami
  // ADDED: after deep sleep wake-up (GPIO or timer) reset I2C to prevent sensor issues
  if (wakeupCause != ESP_SLEEP_WAKEUP_UNDEFINED) {
    Wire.end();
    Wire.begin();
    delay(10);
    LOG_INFO("I2C zresetowany po deep sleep / I2C reset after deep sleep.");
  }

  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    LOG_INFO("\n>>> START SYSTEMU / SYSTEM START: Oczekiwanie 5 s przed Zigbee...");
    for (int i = 5; i > 0; i--) {
      LOG_INFO("Start za / in: %d s", i);
      digitalWrite(LED_PIN, LOW);  delay(500);
      digitalWrite(LED_PIN, HIGH); delay(500);
    }
    digitalWrite(LED_PIN, LOW);
  } else {
    LOG_INFO("Wake-up z deep sleep / from deep sleep. Pomijam countdown.");
  }

  LOG_INFO("Uruchamianie Zigbee... / Starting Zigbee... (próba / attempt %d)",
                zigbeeFailCount + 1);

  // Watchdog + start stosu Zigbee
  // Watchdog + Zigbee stack start
  zigbeeBeginDone = false;
  xTaskCreate(zigbeeWatchdogTask, "zb_watchdog", 2048, NULL, 10, NULL);

  bool zbOk = Zigbee.begin(&zigbeeConfig, false);
  zigbeeBeginDone = true;

  if (!zbOk) {
    zigbeeFailCount++;
    LOG_WARN("Zigbee start NIEUDANY / FAILED! Próba / Attempt %d.", zigbeeFailCount);
    if (zigbeeFailCount >= 5) {
      LOG_WARN("5 błędów z rzędu — czyszczę NVS... / 5 consecutive failures — erasing NVS...");
      zigbeeFailCount = 0;
      nvs_flash_erase();
      nvs_flash_init();
    }
    delay(1000);
    ESP.restart();
  }

  zigbeeFailCount = 0;
  dev.zigbeeConnectStart = millis();
  LOG_INFO("Zigbee OK. Czekam na sieć... / Waiting for network...");

  // Factory reset zlecony przez przycisk podczas deep sleep
  // Factory reset requested by button press during deep sleep
  if (factoryResetRequested) {
    factoryResetRequested = false;
    LOG_WARN(">>> FACTORY RESET: Czyszczę dane i restartuję... / Clearing data and restarting...");
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, LOW);  delay(100);
      digitalWrite(LED_PIN, HIGH); delay(100);
    }
    Zigbee.factoryReset();
    esp_restart();
  }
}

// =============================================================================
//  Główna pętla
//  Main loop
// =============================================================================
void loop() {
  // ---- Obsługa przycisku - wymagane przytrzymanie 3 s ----
  static unsigned long buttonPressStart = 0;
  static bool buttonWasPressed = false;

  int btnState = digitalRead(button);
  if (btnState == LOW) {
    if (!buttonWasPressed) {
      buttonWasPressed = true;
      buttonPressStart = millis();
    } else {
      // jeśli przycisk trzymany dłużej niż 3 sekundy – wykonaj factory reset
      if (millis() - buttonPressStart >= 3000) {
        LOG_WARN("Przycisk trzymany 3 s – factory reset");
        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_PIN, LOW);  delay(100);
          digitalWrite(LED_PIN, HIGH); delay(100);
        }
        digitalWrite(LED_PIN, LOW);
        Zigbee.factoryReset();
        esp_restart();
      }
    }
  } else {
    buttonWasPressed = false; // przycisk puszczony
  }

  // Po połączeniu — jednorazowo uruchom pomiar
  // After connection — start measurement once
  if (Zigbee.connected() && !dev.measurementStarted) {
    LOG_INFO("Połączono z siecią! / Connected to network! Uruchamiam pomiar. / Starting measurement.");
    dev.measurementStarted = true;
    xTaskCreate(measureAndSleep, "temp_update", 4096, NULL, 5, NULL);
  }

  // Timeout połączenia Zigbee — jeśli nie połączono w 5 minut, zasypiam
  // Zigbee connect timeout — if not connected within 5 minutes, go to sleep
  if (!dev.measurementStarted &&
      dev.zigbeeConnectStart > 0 &&
      (millis() - dev.zigbeeConnectStart) > ZIGBEE_CONNECT_TIMEOUT_MS) {
    LOG_WARN("Timeout połączenia Zigbee. Zasypiam. / Zigbee connect timeout. Sleeping.");
    Serial.flush();
    delay(10); // gwarancja opróżnienia bufora UART / ensure UART buffer is drained
    esp_deep_sleep_start();
  }

  // Jeśli zadanie pomiaru skończyło — wejdź w deep sleep
  // If measurement task is done — enter deep sleep
  if (checkGoToSleep()) {
    LOG_INFO("Wchodzę w deep sleep... / Entering deep sleep...");
    Serial.flush();
    delay(10); // gwarancja opróżnienia bufora UART / ensure UART buffer is drained
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW);  delay(50);
    esp_deep_sleep_start();
  }

  delay(20);
}
