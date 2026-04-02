# Zigbee Temperature and Humidity Sensor with Deep Sleep

[Polski](#polski) | [English](#english)

---

## Polski

Czujnik temperatury i wilgotności oparty na module Seeed Studio XIAO ESP32C6 z obsługą protokołu Zigbee i trybem głębokiego uśpienia. Urządzenie mierzy temperaturę, wilgotność oraz poziom naładowania baterii LiPo, a następnie wysyła dane do koordynatora Zigbee i usypia na 10 minut.

![PCB](images/PCB.jpg)

---

## Funkcje

- Pomiar temperatury i wilgotności względnej (SHT3x)
- Pomiar napięcia baterii LiPo (INA226) z przeliczaniem na procenty
- Tryb głębokiego uśpienia — 10 minut między pomiarami
- Wybudzanie przyciskiem (GPIO0) — factory reset po przytrzymaniu 3 sekund
- Watchdog dla Zigbee.begin() — automatyczny restart przy zawieszeniu stosu
- Timeout połączenia Zigbee (5 minut) — zasypianie gdy sieć niedostępna
- Dwujęzyczne logowanie (PL/EN) z poziomami DEBUG / INFO / WARN / ERROR

---

## Sprzęt

| Komponent | Model | Uwagi |
|---|---|---|
| Mikrokontroler | Seeed Studio XIAO ESP32C6 | Zigbee 3.0, wbudowana ładowarka LiPo |
| Czujnik T/H | SHT3x | I2C, adres 0x44 |
| Pomiar baterii | INA226 | I2C, adres 0x40, rezystor 0.1 Ω |
| Bateria | LiPo 1S | np. 2000 mAh |
| Przycisk | dowolny NO | podłączony do GPIO0 (BOOT) |

---

## Schemat połączeń

![Schemat](images/Schemat.jpg)

Oba czujniki (SHT3x i INA226) podłączone są do wspólnej magistrali I2C (SDA / SCL). Bateria LiPo podłączona jest bezpośrednio do złącza BAT modułu XIAO oraz do wejść IN+/IN- czujnika INA226. Przycisk wpięty między GPIO0 a GND.

---

## Struktura repozytorium

```
├── README.md
├── LICENSE
├── .gitignore
├── Zigbee_Temp_Hum_Sensor_Sleepy.ino
└── images/
    ├── PCB.jpg
    └── Schemat.jpg
```

---

## Konfiguracja

Wszystkie parametry zebrane są na górze pliku `.ino`:

| Stała | Domyślna wartość | Opis |
|---|---|---|
| `TIME_TO_SLEEP` | 600 s | Czas głębokiego uśpienia |
| `REPORT_TIMEOUT` | 6000 ms | Maksymalny czas oczekiwania na ACK |
| `ZIGBEE_CONNECT_TIMEOUT_MS` | 300 000 ms | Timeout połączenia z siecią |
| `SHUNT_RESISTANCE` | 0.1 Ω | Rezystor bocznikowy INA226 |
| `SHT3X_I2C_ADDR` | 0x44 | Adres I2C czujnika SHT3x |
| `INA226_ADDR` | 0x40 | Adres I2C czujnika INA226 |
| `DEBUG_ENABLED` | 0 | Ustaw 1 aby włączyć pełne logi |

---

## Wymagania

- Arduino IDE z obsługą ESP32 (płytka esp32 by Espressif, wersja z obsługą Zigbee)
- Tryb Zigbee ustawiony na **Zigbee ED (End Device)** w menu Tools
- Biblioteki:
  - `Zigbee.h` (wbudowana w core ESP32 Espressif)
  - `Adafruit SHT31`
  - `INA226_WE`
  - `Wire`

---

## Pierwsze uruchomienie

1. Wgraj szkic na XIAO ESP32C6 przy ustawionym trybie Zigbee End Device.
2. Urządzenie odczeka 5 sekund (sygnalizacja LED), po czym spróbuje połączyć się z koordynatorem Zigbee.
3. Po połączeniu wykona pomiar i wyśle dane, następnie uśnie na 10 minut.
4. Aby sparować urządzenie ponownie lub wyczyścić sieć — przytrzymaj przycisk przez 3 sekundy. LED miga 3 razy i urządzenie wykonuje factory reset.

---

## Działanie po deep sleep

Po wybudzeniu przez timer lub przycisk urządzenie przechodzi przez skrócony boot (bez odliczania 5 s). Magistrala I2C jest resetowana po każdym wybudzeniu, aby uniknąć problemów ze stabilnością czujników. Ostatnia prawidłowa wartość napięcia baterii przechowywana jest w pamięci RTC i używana jako fallback gdy INA226 jest niedostępny.

---

## Licencja

Projekt oparty na przykładzie Espressif Systems (Shanghai) PTE LTD, objętym licencją Apache 2.0.  
Modyfikacje: Maciej Sikorski, 2026.  
Szczegóły w pliku [LICENSE](LICENSE).

---

### Autor

**Wersja:** 1.0  
**Autor:** Maciej Sikorski  
**Data:** 01.04.2026  
**Licencja:** Apache 2.0

---
---

## English

A temperature and humidity sensor based on the Seeed Studio XIAO ESP32C6 module with Zigbee protocol support and deep sleep mode. The device measures temperature, humidity, and LiPo battery level, sends the data to a Zigbee coordinator, and then sleeps for 10 minutes.

![PCB](images/PCB.jpg)

---

## Features

- Temperature and relative humidity measurement (SHT3x)
- LiPo battery voltage measurement (INA226) with percentage conversion
- Deep sleep mode — 10 minutes between measurements
- Button wakeup (GPIO0) — factory reset after holding for 3 seconds
- Watchdog for Zigbee.begin() — automatic restart if the stack hangs
- Zigbee connection timeout (5 minutes) — sleep if network unavailable
- Bilingual logging (PL/EN) with DEBUG / INFO / WARN / ERROR levels

---

## Hardware

| Component | Model | Notes |
|---|---|---|
| Microcontroller | Seeed Studio XIAO ESP32C6 | Zigbee 3.0, built-in LiPo charger |
| T/H Sensor | SHT3x | I2C, address 0x44 |
| Battery monitor | INA226 | I2C, address 0x40, 0.1 Ω shunt resistor |
| Battery | LiPo 1S | e.g. 2000 mAh |
| Button | any NO type | connected to GPIO0 (BOOT) |

---

## Wiring Diagram

![Schemat](images/Schemat.jpg)

Both sensors (SHT3x and INA226) share the same I2C bus (SDA / SCL). The LiPo battery connects directly to the XIAO BAT connector and to the IN+/IN- pins of the INA226. The button is wired between GPIO0 and GND.

---

## Repository Structure

```
├── README.md
├── LICENSE
├── .gitignore
├── Zigbee_Temp_Hum_Sensor_Sleepy.ino
└── images/
    ├── PCB.jpg
    └── Schemat.jpg
```

---

## Configuration

All parameters are defined at the top of the `.ino` file:

| Constant | Default value | Description |
|---|---|---|
| `TIME_TO_SLEEP` | 600 s | Deep sleep duration |
| `REPORT_TIMEOUT` | 6000 ms | Maximum wait time for ACK |
| `ZIGBEE_CONNECT_TIMEOUT_MS` | 300 000 ms | Network connection timeout |
| `SHUNT_RESISTANCE` | 0.1 Ω | INA226 shunt resistor value |
| `SHT3X_I2C_ADDR` | 0x44 | SHT3x I2C address |
| `INA226_ADDR` | 0x40 | INA226 I2C address |
| `DEBUG_ENABLED` | 0 | Set to 1 to enable full debug logs |

---

## Requirements

- Arduino IDE with ESP32 support (esp32 by Espressif board package, Zigbee-capable version)
- Zigbee mode set to **Zigbee ED (End Device)** in the Tools menu
- Libraries:
  - `Zigbee.h` (included in the Espressif ESP32 core)
  - `Adafruit SHT31`
  - `INA226_WE`
  - `Wire`

---

## First Boot

1. Upload the sketch to the XIAO ESP32C6 with Zigbee End Device mode selected.
2. The device waits 5 seconds (LED blinks), then attempts to join a Zigbee coordinator.
3. After connecting, it takes a measurement, sends the data, and goes to sleep for 10 minutes.
4. To re-pair or clear network settings — hold the button for 3 seconds. The LED blinks 3 times and the device performs a factory reset.

---

## Behavior After Deep Sleep

After waking up via timer or button, the device goes through a shortened boot sequence (no 5-second countdown). The I2C bus is reset after every wakeup to ensure sensor stability. The last valid battery voltage is stored in RTC memory and used as a fallback if the INA226 is unavailable.

---

## License

Based on an example by Espressif Systems (Shanghai) PTE LTD, licensed under Apache 2.0.  
Modifications by Maciej Sikorski, 2026.  
See [LICENSE](LICENSE) for details.

### Author

**Version:** 1.0  
**Author:** Maciej Sikorski  
**Date:** 01.04.2026  
**License:** Apache 2.0
