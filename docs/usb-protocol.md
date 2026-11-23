# OpenDWS-200 USB-CDC Telemetrie- & Steuerungsprotokoll

Im laufenden Betrieb meldet sich OpenDWS-200 am USB-C-Port als **USB CDC-ACM (Virtueller COM-Port)** mit `VID 0x2E3C` / `PID 0x5740` (`OpenDWS-200 Bring-Up CDC`).

- **Zeilenformat:** ASCII, abgeschlossen mit `\r` oder `\n` (Antworten enden mit `\r\n`).
- **Groß-/Kleinschreibung:** Kommandos sind case-insensitiv.
- **Baudrate:** Beliebig (z. B. `115200 8N1`), da reines USB Full-Speed Bulk.

---

## Befehlsübersicht

| Befehl | Beschreibung | Beispielantwort |
| :--- | :--- | :--- |
| `HELP` oder `?` | Liste der verfügbaren Befehle | `OK CMDS: HELP \| INFO \| STAT \| ...` |
| `INFO` | Firmware-Version, MCU-Infos, Safety-Status | `INFO fw=v0.1.0-bringup mcu=AT32F415RCT7 clk=144MHz safe_mode=1 heater=LOCKED_OFF uptime_ms=12540` |
| `STAT` | Einmaliger Telemetrie-Schnappschuss | `STAT ms=12540 tip_adc=120 tip_mv=97 cur_adc=2048 cur_mv=1650 cur_ma=0 aux_adc=1500 aux_mv=1209 keys=0x00 pb5=1 pb5_cnt=3 pb6=1 pb0=1 pb2=0 bl=80 led=0x00 heater=0` |
| `STREAM ON` | Aktiviert zyklischen `STAT`-Stream | `OK STREAM=ON INTERVAL=100` |
| `STREAM OFF` | Stoppt zyklischen Stream | `OK STREAM=OFF` |
| `STREAM <50..2000>` | Intervall in ms setzen + aktivieren | `OK STREAM=ON INTERVAL=200` |
| `BL <0..100>` | Display-Backlight in % setzen | `OK BL=60` |
| `BEEP [10..1000]` | Piezo-Buzzer für `ms` (Standard 80 ms) | `OK BEEP=150` |
| `LED <0..255\|0x..>` | 8 Front-LEDs per Bitmaske schalten | `OK LED=0xA5` |
| `REBOOT` | System-Reset auslösen | `OK REBOOT` |
| `HEAT*`/`TEMP*`/`SET*`/`PID*` | **Phase-1-Sperre** (Heizung deaktiviert) | `ERR HEATER_LOCKED_IN_SAFE_MODE` |

---

## Felder von `STAT`

- `ms`: Uptime in Millisekunden seit Boot.
- `tip_adc` / `tip_mv`: Gefilterter 16-Sample-Mittelwert auf `PC0` (`ADC1_CH10`, 0..4095) und umgerechnete Millivolt.
- `cur_adc` / `cur_mv` / `cur_ma`: Stromsensor auf `PC1` (`ADC1_CH11`, 66 mV/A um 1,65 V Nullpunkt).
- `aux_adc` / `aux_mv`: 3. Analogkanal auf `PA3` (`ADC1_CH3`).
- `keys`: Entprellte 5-Bit-Tastenmaske (`bit0=PD2, bit1=PB3, bit2=PB4, bit3=PC12, bit4=PC11`).
- `pb5` / `pb5_cnt`: Aktueller Pegel von `PB5` (Sleep-Ablage) und Interrupt-Flankenzähler (`EXINT5`).
- `pb6`: Griff-Erkennung (`F245` / `F210`).
- `pb0`, `pb2`: Zusätzliche Diagnose-Eingänge.
- `bl`: Display-Helligkeit (`0..100` %).
- `led`: Aktive Front-LED-Bitmaske (`0x00..0xFF`).
- `heater`: In Phase 1 fest `0` (`PA2 = LOW`).
