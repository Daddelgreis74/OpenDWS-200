# Phase-2 Reverse-Engineering-Analyse (`APP_DWS_200_V1.5_250922.bin`)

Diese Spezifikation dokumentiert die aus der offiziellen Stock-Firmware `APP_DWS_200_V1.5_250922.bin` (Artery `AT32F415RCT7`, App-Basis `0x08006000`) verifizierte Heizungs-, Mess-, Griff-Umschalt-, PID- und Sicherheitsarchitektur als direkte Basis für OpenDWS-200 Phase 2.

---

## 1. Timer- & Blanking-Architektur (`TMR1` + `TMR2`)

Die DWS-200 nutzt in der Stock-Firmware **kein AC-Zero-Crossing**, sondern eine **DC-gepulste `10 ms` (`100 Hz`) Heiz- und Mess-Zeitscheibe** über zwei Hardware-Timer:

### 1.1 `TMR1` – `1,000 ms` Scheduler (`0x08014ab4`, `TMR1_OVF_IRQHandler` @ `0x080098f8`)

- **Konfiguration:** `144 MHz / (143 + 1) = 1 MHz` Zähltakt, Periode `PR = 999` → **`1,000 ms` (`1 kHz`)** Überlauf-Interrupt (IRQ 25).
- **10-Slot Round-Robin (`slot = counter % 10`, Gesamtzyklus `10 ms` / `100 Hz`):**
  - **Slot 0 (`t = 0 ms`, `0x080123b0`):** Tastenabfrage (`0x080114dc`), Griff-Erkennung (`PB6` → `PC13..PC15`, `0x08011898`), Schlafständer (`PB5`, `0x08011310`), Soft-Power-Überwachung (`PB0`/`PB1`, `0x08011834`).
  - **Slot 1 (`t = 1 ms`, `0x08009960`):** Ruft `Heater_Ctrl(0)` (`0x08012cfc`) auf → **schaltet `PA2` zwingend `LOW`**. Damit beginnt das **Blanking-Fenster** (Entkopplung von Heizstrom und Thermospannung).
  - **Slot 2 (`t = 2 ms`, `0x080113a4`):** Exakt **`1,0 ms` nach `PA2 = LOW`** → Thermoelement `PC0` einlesen (`0x0800fd84`), Open-Tip-Prüfung (`tip_adc > 4000`), inkrementellen PID berechnen (`0x080129f0`), Heizpuls für diesen `10 ms`-Zyklus starten (`0x08012cfc`).
  - **Slot 3 (`t = 3 ms`, `0x080123c8`):** Sleep-/Standby-Countdown (`6000 × 10 ms` Faktor, `0x1770`) und Buzzer-Abschalt-Timer.
  - **Slot 4 (`t = 4 ms`, `0x0801196c`):** Alle 50 Zyklen (`500 ms`) `PA3` (`ADC1_CH3`) messen → `V_rail = (adc * 3.3 / 4095.0) * 16.0` (24-V-Versorgungsschiene; auf echter Station verifiziert: `1,434 V × 16 = 22,94 V` am Netz, `0,145 V × 16 = 2,32 V` bei reinem USB-Betrieb).
  - **Slot 5 (`t = 5 ms`, `0x08009988`):** `PC1` (`ADC1_CH11`) Hall-Stromsensor einlesen → `I = |(adc * 3.3 / 4095.0 - 1.65) / 0.066|`. Wenn `I > 5,0 A`, wird ein Integrator hochgezählt (`3000` Zyklen = `30 s` → Dauerüberlast-Fault `[0x20000340 + 0x22] = 1`).

### 1.2 `TMR2` – `91 µs` Pulsbreiten- & Überstrom-Timer (`0x08014b18`, `TMR2_IRQHandler` @ `0x08009a68`)

- **Konfiguration:** `144 MHz / (143 + 1) = 1 MHz` Zähltakt, Periode `PR = 90` (`0x5A`) → **`91 µs`** (`~10,99 kHz`, IRQ 28).
- **Puls-Ende:** Dekrementiert die verbleibenden Heiz-Ticks (`[0x20000340 + 0x3d]`). Bei `0` wird sofort `PA2 = LOW` (`0x08009bb0`) gesetzt (`Heizung AUS`).
- **Kurzschluss- & Lasterkennung (`0x08009abc`):** Exakt **4 Ticks (`364 µs`) nach Pulsstart** (`remaining == initial - 4`) wird `PC1` (`ADC1_CH11`) gemessen und `I` berechnet:
  - Sofortige Notabschaltung bei Kurzschluss/Überstrom (`[0x20000340 + 0x23] = 1`):
    - **`handle_type == 1`:** `I > 10,0 A` (`0x40240000`)
    - **`handle_type == 0`:** `I > 15,0 A` (`0x402E0000`)
  - Lasterkennung für PID-Parametersatz (`[0x20000340 + 0x24]`): `I < 5,0 A` (`0x40140000`) setzt `flag24 = 1`.

---

## 2. Griff-Erkennung (`PB6`) & Bereichsumschaltung (`PC13`, `PC14`, `PC15`)

- `Stand_Read` (`0x080146d0`) liest `PB5` (Sleep-Ablage), `Handle_Read` (`0x08006c74`) liest `PB6`.
- `Sleep_Stand_0x08011310` entprellt `PB6` über `5` Zyklen (`50 ms`) und schaltet drei Analog-Mux-/Verstärker-Steuerleitungen (`PC13`, `PC14`, `PC15`) um. Gleichzeitig wird der PID für `100` Zyklen (`1,0 s`, `[0x20000028 + 5] = 100`) pausiert (`0x08011360`), damit sich die Thermoelement-Eingangsstufe einschwingen kann:

| Zustand `PB6` | Stock `handle_type` | `PC13` | `PC14` | `PC15` | Anmerkung |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`PB6 == 1`** | `1` *(Standard an Teststation)* | `LOW` | `HIGH` | `LOW` | Aktive Konfiguration bei `pb6=1` im Phase-1-Bring-Up |
| **`PB6 == 0`** | `0` | `HIGH` | `LOW` | `HIGH` | Zweiter Handgrifftyp (`F245` vs. `F210`) |

> [!IMPORTANT]
> Die physische Zuordnung von `handle_type = 1` vs. `0` zu `F245` und `F210` wird vor Freigabe von `PA2` noch einmal direkt am angeschlossenen Handgriff auf `COM7` gegenbestätigt.

---

## 3. Thermoelement-Kennlinien (`PC0` / `ADC1_CH10` → °C, `0x0800fd84`)

Nach `1,0 ms` Heizpause liest `0x0800fd84` `ADC1_CH10` (`PC0`, `tip_adc`) und rechnet über eine **5-Segment stückweise lineare Interpolation** mit 4 Stützstellen (`150 °C`, `250 °C`, `350 °C`, `450 °C`) auf °C um (zusätzlich kalibrierbar über Flash-Offsets um `0x8000` aus `0x0803F800` + EMA-Filter `0x08014a70` mit `alpha = 0,2` + 40-Sample-Ringpuffer für die Anzeige):

| Stützpunkt | `handle_type == 1` (`PB6 == 1`) | `handle_type == 0` (`PB6 == 0`) |
| :--- | :--- | :--- |
| `0 °C` | `0 raw` (`0 mV`) | `0 raw` (`0 mV`) |
| `150 °C` | `1100 raw` (`0x044C`, `~886 mV`) | `276 raw` (`0x0114`, `~222 mV`) |
| `250 °C` | `1870 raw` (`0x074E`, `~1507 mV`) | `608 raw` (`0x0260`, `~490 mV`) |
| `350 °C` | `2610 raw` (`0x0A32`, `~2103 mV`) | `882 raw` (`0x0372`, `~711 mV`) |
| `450 °C` | `3340 raw` (`0x0D0C`, `~2692 mV`) | `1210 raw` (`0x04BA`, `~975 mV`) |

*(Im Bring-Up gemessener Raumtemperaturwert `11..25 raw` bei `PB6 = 1` entspricht damit plausiblen `~1,5..3,4 °C` über `0 °C`-Referenz bzw. Raum-/Kaltstellenbereich vor Null-Offset-Abgleich.)*

---

## 4. Inkrementeller PID-Regler & Duty-Begrenzung (`0x080129f0`, `0x08012cfc`)

Der Regler arbeitet alle `10 ms` als inkrementeller PID (`err = target_temp - tip_temp`) mit Bang-Bang-Schwellen:

- **Bang-Bang-Grenzen:**
  - `err > +50 °C` (`0x40490000`) → `100 %` (durch aktives Duty-Cap begrenzt)
  - `err < -5 °C` (`0xC0140000`) → `0 %` (`PA2` aus)
- **PID-Parameter (`Kp`, `Ki`, `Kd`) und Puls-Limits (`1 Tick = 91 µs`):**

| Modus | Bedingung | `Kp` | `Ki` | `Kd` | Max. Pulsdauer / `10 ms` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `handle_type == 1` | Last `I >= 5 A` (`flag24 == 0`) | `2,28` | `0,92` | `0,28` | `80` Ticks (`7,28 ms`) bei `T < 100 °C`, sonst `100` (eff. `~9,0 ms` bis Slot 1) |
| `handle_type == 1` | Kleine Last `I < 5 A` (`flag24 == 1`) | `2,48` | `0,015` | `0,02` | wie oben |
| `handle_type == 0` | `T <= 150 °C` | `0,30` | `0,02` | `0,01` | Hart begrenzt auf `30` Ticks (`2,73 ms` ≈ `27,3 %`) |
| `handle_type == 0` | `T > 150 °C` | `0,40` | `0,02` | `0,01` | Hart begrenzt auf `30` Ticks (`2,73 ms`) |
| `handle_type == 0` | `T > 250 °C` | `0,50` | `0,02` | `0,01` | Hart begrenzt auf `30` Ticks (`2,73 ms`) |
| `handle_type == 0` | `T > 350 °C` | `0,60` | `0,02` | `0,01` | Hart begrenzt auf `30` Ticks (`2,73 ms`) |

---

## 5. Sicherheits-, Power- & Peripheriefunktionen

- **Open-Tip / Spitzen-Wechsel (`0x08011400`):** `tip_adc > 4000` (`0x0FA0`) meldet „keine Spitze“ und sperrt die Heizung sofort. Nach Wiedereinstecken wartet die Firmware `50` Zyklen (`500 ms`), gibt dann zunächst einen **`5`-Tick-Prüfpuls (`455 µs`)** (`0x0801143e`) ab, um auf Kurzschluss zu testen (`364 µs`-Messung in `TMR2`), und schaltet erst danach den normalen PID frei.
- **Kurzschluss- & Dauerüberlastschutz:** Sofortabschaltung im `TMR2` bei `I > 10 A` (`handle_type 1`) bzw. `> 15 A` (`handle_type 0`) sowie `30 s`-Dauerüberlastschutz (`I > 5 A`).
- **Soft-Power-Latch (`PB0` / `PB1`, `0x08011834`):** `PB1` hält das System eingeschaltet (`HIGH`). Nach einer `PB0`-Flankensequenz schaltet die Station `PB1` auf `LOW`.
- **I2C-Nebenbus (`PB10` / `PB11`, `0x08014304`):** Bit-Bang-I2C an Slave-Adresse `0x78` (optionales Power-/Display-IC; in Phase 1 nicht benötigt).
- **Flash-Settings (`0x0803F800`):** `200` Bytes (`0x64` Halfwords, Stock-Magic `0x001F` am Offset `0x00`), inklusive getrennter 3-Punkt-Kalibrier-Offsets für `handle_type 1` (`+0x18..+0x1C`) und `handle_type 0` (`+0x2E..+0x32`, jeweils `0x8000 - val`).
- **Zusätzliche Sicherheitsregeln für OpenDWS-200 Phase 2:**
  1. Hardware-Watchdog (`WDT` ~2 s) erzwingt im Fehler-/Hängerfall Reset mit sofortigem `PA2 = LOW` ab Instruktion 1.
  2. **24-V-Rail-Sperre (`PA3`):** Heizung bleibt strikt blockiert, solange `V_rail < 15 V` (verhindert Heizversuche im reinen USB-5-V-Versorgungsbetrieb).
  3. **Auto-Nullpunkt für Hall-Sensor (`PC1`):** Beim Start und im Leerlauf (`heater == 0`) aktuellen Ruhe-Offset einmessen (auf echter Hardware: `~1642 mV` statt ideal `1650 mV`).
  4. **Absolutes Software-Limit:** Harte Abschaltung bei `T > 480 °C`, fehlendem Griff-Signal oder Überschreiten der maximalen Einschaltdauer ohne Temperaturanstieg (*Thermal Runaway Protection*).
