# Zigbee Czujnik Temperatury i Wilgotności z Deep Sleep
# Zigbee Temperature & Humidity Sensor with Deep Sleep

**Wersja / Version:** 1.0  
**Autor / Author:** Maciej Sikorski  
**Data / Date:** 30.03.2026  
**Licencja / License:** Apache 2.0

---

## Opis / Description

Czujnik Zigbee End Device oparty na płytce **Seeed Studio XIAO ESP32C6**.  
Co 10 minut budzi się z deep sleep, mierzy temperaturę i wilgotność (SHT3x) oraz napięcie baterii LiPo (INA226), wysyła dane do sieci Zigbee i ponownie zasypia.

A Zigbee End Device based on the **Seeed Studio XIAO ESP32C6**.  
Every 10 minutes it wakes from deep sleep, measures temperature & humidity (SHT3x) and LiPo battery voltage (INA226), reports to Zigbee network, then sleeps again.

---

## Zdjęcie / Photo

![PCB](hardware/PCB.jpg)

*XIAO ESP32C6 (góra / top) + INA226 (lewy dół / bottom left) + SHT3x (prawy dół / bottom right)*

---

## Sprzęt / Hardware

| Komponent / Component | Model |
|---|---|
| Płytka / Board | Seeed Studio XIAO ESP32C6 |
| Czujnik T/H / T/H Sensor | SHT30 / SHT31 / SHT35 (SHT3x) |
| Monitor baterii / Battery monitor | INA226 |
| Bateria / Battery | Li-Pol / LiPo 3.7V |

---

## Schemat podłączenia / Wiring diagram

### XIAO ESP32C6 → SHT3x

| XIAO ESP32C6 | SHT3x |
|---|---|
| GPIO22 (SDA) | SDA |
| GPIO23 (SCL) | SCL |
| 3.3V | VCC |
| GND | GND |

> Adres I2C: `0x44` (domyślny / default)

---

### XIAO ESP32C6 → INA226

| XIAO ESP32C6 | INA226 |
|---|---|
| GPIO22 (SDA) | SDA |
| GPIO23 (SCL) | SCL |
| 3.3V | VCC |
| GND | GND |

### INA226 → Bateria LiPo / LiPo battery

| INA226 | Podłączenie / Connection |
|---|---|
| VIN+ | (+) baterii LiPo / LiPo (+) |
| VIN- | zwarte z V_BUS / shorted to V_BUS |
| V_BUS | zwarte z VIN- / shorted to VIN- |

> INA226 mierzy napięcie szyny (bus voltage) jako napięcie baterii.  
> INA226 measures bus voltage as battery voltage.

---

### Przycisk BOOT / BOOT button

| XIAO ESP32C6 | |
|---|---|
| GPIO0 | Przycisk → GND / Button → GND |

> Przytrzymaj > 3 sekundy → Factory Reset (miga LED)  
> Hold > 3 seconds → Factory Reset (LED blinks)

---

## Konfiguracja Arduino IDE / Arduino IDE setup

### 1. Płytka / Board

Zainstaluj pakiet ESP32 Arduino (Espressif) w wersji **3.x** lub nowszej.  
Install ESP32 Arduino package (Espressif) version **3.x** or newer.

Wybierz / Select: `Tools → Board → ESP32 Arduino → XIAO_ESP32C6`

### 2. ⚠️ Tryb Zigbee / Zigbee mode — WYMAGANE / REQUIRED

```
Tools → Zigbee mode → Zigbee ED (end device)
```

Bez tego ustawienia kompilacja zakończy się błędem.  
Without this setting compilation will fail with an error.

### 3. Partycja / Partition scheme

```
Tools → Partition Scheme → Zigbee 4MB with spiffs
```

---

## Biblioteki / Libraries

Zainstaluj przez Arduino Library Manager / Install via Arduino Library Manager:

| Biblioteka / Library | Autor / Author |
|---|---|
| `Adafruit SHT31 Library` | Adafruit |
| `Adafruit INA219` | Adafruit |
| `Adafruit BusIO` | Adafruit (zależność / dependency) |

> **Uwaga / Note:** Biblioteka `Adafruit INA219` jest używana z układem INA226.  
> Działa poprawnie dla odczytu napięcia szyny (bus voltage).  
> The `Adafruit INA219` library is used with the INA226 chip.  
> It works correctly for bus voltage reading.

---

## Działanie / Operation

```
[Budzenie / Wake up]
        ↓
[Odczyt SHT3x: T + RH / Read SHT3x: T + RH]
        ↓
[Odczyt INA226: napięcie baterii / Read INA226: battery voltage]
        ↓
[Połączenie Zigbee / Zigbee connect]
        ↓
[Raport Zigbee: T, RH, % baterii / Zigbee report: T, RH, battery %]
        ↓
[Deep sleep 10 min]
```

---

## Kompatybilność Zigbee / Zigbee compatibility

Czujnik był projektowany i budowany z myślą o integracji z **Samsung SmartThings**.  
The sensor was designed and built for integration with **Samsung SmartThings**.

Powinien działać również z innymi koordynatorami Zigbee, jednak nie były one testowane.  
Should also work with other Zigbee coordinators, but these have not been tested.

| Hub / Coordinator | Status |
|---|---|
| **Samsung SmartThings** | ✅ Docelowy / Target platform |
| Home Assistant + ZHA | ⚠️ Nie testowano / Not tested |
| Zigbee2MQTT | ⚠️ Nie testowano / Not tested |

---

## Licencja / License

Ten projekt oparty jest na przykładzie Espressif Systems objętym licencją Apache 2.0.  
This project is based on an Espressif Systems example covered by the Apache 2.0 license.

Copyright 2026 Maciej Sikorski  
Copyright 2024 Espressif Systems (Shanghai) PTE LTD  

Zobacz plik [LICENSE](LICENSE) po pełny tekst licencji.  
See [LICENSE](LICENSE) file for full license text.
