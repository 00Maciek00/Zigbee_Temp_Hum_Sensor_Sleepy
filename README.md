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

![PCB](images/PCB.jpg)

*XIAO ESP32C6 (góra / top) + INA226 (lewy dół / bottom left) + SHT3x (prawy dół / bottom right)*

![PCB](images/Schemat.jpg)
---

## Cechy / Features

- 🔋 **Efektywny energetycznie / Power efficient:** Deep sleep 10 minut, ~5–6 lat na baterii LiPo 2000 mAh
- 🛡️ **Niezawodny / Reliable:** Retry logic dla czujników, RTC fallback, thread-safe
- 📊 **Monitorowanie baterii / Battery monitoring:** Napięcie + procent rozładowania
- 🌡️ **Dokładne pomiary / Accurate measurements:** SHT3x (±0.3°C, ±2% RH), timeout + walidacja zakresu
- 🔧 **Factory Reset:** Przycisk BOOT (> 3 s) resetuje Zigbee binding
- 📝 **Bilingual docs / Dokumentacja dwujęzyczna:** PL + EN

---

## Sprzęt / Hardware

| Komponent / Component | Model |
|---|---|
| Płytka / Board | Seeed Studio XIAO ESP32C6 |
| Czujnik T/H / T/H Sensor | SHT30 / SHT31 / SHT35 (SHT3x) |
| Monitor baterii / Battery monitor | INA226 |
| Bateria / Battery | Li-Pol / LiPo 3.7V |

---

## BOM (Lista części / Parts list)

| Lp. / No. | Komponent / Component | Model | Ilość / Qty | Źródło / Source |
|---|---|---|---|---|
| 1 | Płytka / Board | XIAO ESP32C6 | 1 | Seeed Studio |
| 2 | Czujnik T/H | SHT31 | 1 | Adafruit / AliExpress |
| 3 | Monitor baterii / Battery monitor | INA226 | 1 | AliExpress |
| 4 | Bateria LiPo | 503450 (500 mAh) / 704050 (2000 mAh) | 1 | AliExpress |
| 5 | Kondensator ceramiczny | 100 nF (dla SHT3x, INA226) | 2 | AliExpress |
| 6 | Rezystor pull-up | 10 kΩ (opcjonalnie / optional) | 2 | AliExpress |

> 💡 Całkowity koszt (~2026): ~25–40 PLN (bez baterii / without battery)

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
| BAT+ | VIN- |
| BAT- | GND |

### INA226 → Bateria LiPo / LiPo battery

| INA226 | Podłączenie / Connection |
|---|---|
| VIN+ | (+) baterii LiPo / LiPo (+) |
| VIN- | zwarte z V_BUS / shorted to V_BUS |
| V_BUS | zwarte z VIN- / shorted to VIN- |
| GND | (-) baterii LiPo / LiPo (-) |

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

## Instalacja / Installation

### Krok 1: Arduino IDE + pakiet ESP32 / Step 1: Arduino IDE + ESP32 package

1. `File → Preferences → Additional Boards Manager URLs` — dodaj / add:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. `Tools → Board Manager` — wyszukaj / search `esp32` → zainstaluj **Espressif Systems** (wersja / version **3.x+**)
3. `Tools → Board → ESP32 Arduino → XIAO_ESP32C6`

### Krok 2: Biblioteki / Step 2: Libraries

`Sketch → Include Library → Manage Libraries`

| Biblioteka / Library | Autor / Author |
|---|---|
| `Adafruit SHT31 Library` | Adafruit |
| `Adafruit INA219` | Adafruit |
| `Adafruit BusIO` | Adafruit (zależność / dependency) |

> **Uwaga / Note:** Biblioteka `Adafruit INA219` jest używana z układem INA226.  
> Działa poprawnie dla odczytu napięcia szyny (bus voltage).  
> The `Adafruit INA219` library is used with the INA226 chip.  
> It works correctly for bus voltage reading.

### Krok 3: Wgrywanie / Step 3: Upload

1. `Tools → Zigbee mode → Zigbee ED (end device)` ⚠️ **WYMAGANE / REQUIRED**
2. `Tools → Partition Scheme → Zigbee 4MB with spiffs`
3. `Sketch → Upload`
4. Po wgraniu przytrzymaj BOOT > 3 s → Factory Reset  
   After upload hold BOOT > 3 s → Factory Reset

---

## Konfiguracja zaawansowana / Advanced configuration

Edytuj w pliku `.ino` / Edit in `.ino` file:

```c
#define TIME_TO_SLEEP  600       // Deep sleep (sekundy / seconds). 300 = 5 min, 1800 = 30 min.
#define REPORT_TIMEOUT 6000      // Czekaj na ACK (ms) / Wait for ACK (ms)
#define DEBUG_ENABLED  1         // Zmień na 0 dla produkcji / Set to 0 for production

// INA226 — zakres napięcia baterii / battery voltage range
#define LIPO_MAX_VOLTAGE  4.20f  // 100% (pełna / fully charged)
#define LIPO_MIN_VOLTAGE  3.00f  // 0%  (rozładowana / empty)
```

> 💡 **Dobór TIME_TO_SLEEP / Choosing TIME_TO_SLEEP:**
> - **300 s (5 min):** Częstsze odczyty, krótszy czas pracy baterii / More frequent readings, shorter battery life
> - **600 s (10 min):** Domyślnie — dobry balans / Default — good balance
> - **1800 s (30 min):** Minimalne zużycie energii, spóźnione alarmy / Minimal energy use, delayed alerts

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

## Pobór prądu / Power consumption

| Stan / State | Prąd / Current | Czas / Duration | Energia / Energy |
|---|---|---|---|
| Deep sleep | 5–10 µA | 599 s | ~50–100 µJ |
| Wake + Zigbee | 50–100 mA | 1 s | ~50–100 mJ |
| Czytanie czujników / Sensor read | 10 mA | 0.5 s | ~5 mJ |
| Wysyłanie / Transmission | 100–150 mA | 0.5 s | ~50–75 mJ |
| **Cykl / Cycle** | — | 600 s | **~125 mJ** |

### Estymacja czasu pracy / Battery lifetime estimate

- **Bateria / Battery:** 2000 mAh LiPo (3.7 V, ~7.4 Wh)
- **Pobór / Consumption:** ~125 mJ/cykl (10 min)
- **Cykli dziennie / Cycles per day:** 144
- **Szacowany czas pracy / Estimated runtime:** ~5–6 lat *(teoretycznie / theoretically)*

> ⚠️ Rzeczywisty czas pracy zależy od / Actual runtime depends on:
> - Stabilności sieci Zigbee (słaba sieć = częstsze retry / weak network = more retries)
> - Temperatury otoczenia (zimna bateria = niższe napięcie / cold battery = lower voltage)
> - Wilgotności i drgań (mogą wpłynąć na SHT3x / may affect SHT3x)

---

## Integracja SmartThings / SmartThings Integration

Czujnik był projektowany i budowany z myślą o integracji z **Samsung SmartThings**.  
The sensor was designed and built for integration with **Samsung SmartThings**.

Po sparowaniu czujnik pojawia się jako urządzenie **Temperature and Humidity** z atrybutem Battery.  
After pairing, the sensor appears as a **Temperature and Humidity** device with a Battery attribute.

| Atrybut / Attribute | Zakres / Range | Dokładność / Accuracy |
|---|---|---|
| Temperatura / Temperature | −40°C … 85°C | ±0.3°C |
| Wilgotność / Humidity | 0% … 100% | ±2% RH |
| Bateria / Battery | 0% … 100% | zakres 3.0–4.2 V / range 3.0–4.2 V |

Po sparowaniu czujnik będzie / After pairing the sensor will:
- ✅ Wysyłać dane co 10 minut / Send data every 10 minutes
- ✅ Wysyłać alert jeśli bateria < 10% / Send alert if battery < 10%
- ✅ Pokazywać status "Connected" / "Offline"

> 💡 SmartThings uznaje urządzenie za offline jeśli nie komunikuje się przez > 1 godzinę.  
> To zachowanie jest normalne dla czujnika z deep sleep.  
> SmartThings marks the device as offline if it doesn't communicate for > 1 hour.  
> This is expected behavior for a deep sleep sensor.

---

## Kompatybilność Zigbee / Zigbee compatibility

| Hub / Coordinator | Status |
|---|---|
| **Samsung SmartThings** | ✅ Docelowy / Target platform |
| Home Assistant + ZHA | ⬜ Nie testowano / Not tested |
| Zigbee2MQTT | ⬜ Nie testowano / Not tested |

---

## Rozwiązywanie problemów / Troubleshooting

### Czujnik nie pojawia się w SmartThings / Sensor not appearing in SmartThings

- ✅ Czy wybrano `Tools → Zigbee mode → Zigbee ED`? ⚠️ WYMAGANE / REQUIRED
- ✅ Czy Hub SmartThings jest w trybie parowania? / Is the SmartThings Hub in pairing mode?
- ✅ Czy bateria ma co najmniej 3.5 V? / Does the battery have at least 3.5 V?
- ⏳ Czekaj do 2 minut — pierwsze budzenie może być wolne / Wait up to 2 minutes — first boot may be slow

### Czujnik wysyła dane rzadziej niż co 10 minut / Sensor reports less often than every 10 minutes

- ⚠️ Sieć Zigbee mogła zmienić polling interval / Zigbee network may have changed polling interval
- 🔧 Usuń urządzenie ze SmartThings → dodaj ponownie → obserwuj logi  
  Remove device from SmartThings → re-add → observe logs

### SHT3x zwraca 0.0°C / INA226 nie odpowiada / SHT3x returns 0.0°C / INA226 not responding

Sprawdź wyjście Serial Monitor / Check Serial Monitor output:
```
[DEBUG] SHT3x OK.              ← dobrze / good
[ERROR] SHT3x nie znaleziony!  ← problem z podłączeniem / wiring issue
```
- Sprawdź połączenia SDA/SCL / Check SDA/SCL connections
- Zweryfikuj adresy I2C: `0x44` (SHT3x), `0x40` (INA226)
- Rozważ dodanie rezystorów pull-up 10 kΩ na SDA/SCL / Consider adding 10 kΩ pull-up resistors on SDA/SCL

### LED nie miga po budzeniu / LED not blinking after wake

- ✅ To normalne! LED miga tylko gdy czujnik **nie jest** połączony z siecią Zigbee.  
  This is normal! LED blinks only when the sensor is **not** connected to a Zigbee network.
- Po połączeniu LED pozostaje ciemny (oszczędzanie baterii) / After connecting LED stays dark (battery saving)

### Serial Monitor pokazuje same błędy I2C / Serial Monitor shows only I2C errors

- Sprawdź pull-upy na SDA/SCL / Check pull-ups on SDA/SCL
- Sprawdź zasilanie — spróbuj innego LDO lub baterii o wyższej pojemności  
  Check power supply — try a different LDO or higher capacity battery

---

## Licencja / License

Ten projekt oparty jest na przykładzie Espressif Systems objętym licencją Apache 2.0.  
This project is based on an Espressif Systems example covered by the Apache 2.0 license.

Copyright 2026 Maciej Sikorski  
Copyright 2024 Espressif Systems (Shanghai) PTE LTD

Zobacz plik [LICENSE](LICENSE) po pełny tekst licencji.  
See [LICENSE](LICENSE) file for full license text.
