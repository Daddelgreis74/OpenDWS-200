# OpenDWS-200 USB-CDC Telemetry & Control Protocol (Phase 2)

During normal operation, OpenDWS-200 enumerates on its USB-C port as a **USB CDC-ACM virtual COM port** (`VID 0x2E3C` / `PID 0x5740`, `OpenDWS-200 Phase-2 CDC`).

- **Line framing:** ASCII terminated by `\r` or `\n` (responses terminate with `\r\n`).
- **Case sensitivity:** Commands are case-insensitive.
- **Baud rate:** Any (e.g., `115200 8N1`), as full-speed USB bulk transfers handle framing directly.

---

## Command Reference

| Command | Description | Example Response |
| :--- | :--- | :--- |
| `HELP` or `?` | List all available commands | `OK CMDS: HELP \| INFO \| STAT \| ...` |
| `INFO` | Firmware version, MCU, handle, state, 24 V rail, uptime | `INFO fw=v1.0.0 mcu=AT32F415RCT7 clk=144MHz safe_mode=0 handle=F245 state=STANDBY rail_mv=22940 uptime_ms=12540` |
| `STAT` | Full telemetry snapshot | `STAT ms=12540 state=HEAT fault=0x00(NONE) handle=F245 tip_c=300.2 target_c=300 act_c=300 duty=45 ticks=40 rail_mv=22928 ... cal=0,0,0 presets=200,300,350` |
| `HEAT <ON\|OFF\|1\|0>` | Arm heater or enter `STANDBY` (rejected on `NO_24V_RAIL` or active fault) | `OK HEAT=ON` or `ERR NO_24V_RAIL` |
| `TEMP <100..450>` | Set target temperature in °C (`SET <t>` alias supported) | `OK TEMP=320` |
| `PRESET <1..3> [100..450]` | Load preset `CH1–CH3`, or assign a new temperature (`100..450 °C`) | `OK PRESET=2 TEMP=325` |
| `SLEEP <ON\|OFF\|0\|50..250>` | Enable/disable auto-stand or set standby temperature (`0` = `0 W` cold, `50..250 °C`, default `100 °C`) | `OK SLEEP=ON` / `OK SLEEP=100` |
| `CAL [150\|250\|350] <-50..50>` | Set 3-point tip calibration offset (`150`/`250`/`350 °C`) in °C for the active handle (without point: updates all 3) | `OK CAL 250=12` or `OK CAL=2` |
| `CLEAR` | Clear latched faults (`OVERCUR`, `OVERLOAD`, `OVERTEMP`, `RUNAWAY`) | `OK CLEAR` |
| `SAVE` | Persist current settings safely (pausing PWM with `PA2=LOW`) to Flash `0x0803F800` | `OK SAVE` |
| `STREAM [ON\|OFF\|50..2000]` | Control periodic `STAT` streaming | `OK STREAM=ON INTERVAL=100` |
| `BL <0..100>` | Set LCD backlight brightness in % | `OK BL=80` |
| `BEEP [10..1000]` | Sound piezo buzzer for `ms` (default `80 ms`) | `OK BEEP=80` |
| `LED <0..255\|0x..\|AUTO>` | Set front LEDs manually or return to `AUTO` (power/fault mode) | `OK LED=AUTO` |
| `REBOOT` | Trigger system/watchdog reset | `OK REBOOT` |

---

## Key `STAT` Fields

- `state`: `STANDBY`, `SLEEP`, `PROBE`, `HEAT`, or `FAULT`.
- `fault`: Active bitmask and primary fault name (`NONE`, `SETTLING`, `OPEN_TIP`, `NO_24V`, `OVERVOLT`, `OVERCUR`, `OVERLOAD`, `OVERTEMP`, `RUNAWAY`).
- `handle`: Active soldering handle (`F245` when `PB6=1`, `F210` when `PB6=0`).
- `tip_c`, `target_c`, `act_c`: Filtered tip temperature (`0.1 °C` resolution for PID and data logging), configured setpoint, and active setpoint (including standby reduction). Station TFT and PC Studio hero KPIs apply additional smoothing and round to whole `°C`.
- `cal`: Current 3-point calibration offsets (`150,250,350` °C) for the active handle (`F245`/`F210`).
- `presets`: Current preset temperatures (`CH1,CH2,CH3` in °C, e.g., `200,300,350`).
- `duty`, `ticks`: Active PWM duty cycle in % and pulse width in `91 µs` ticks.
- `rail_mv`: Measured `24 V` supply rail (`PA3 × 16`; heating is blocked below `15000 mV`).
- `cur_ma`, `cur_pk`, `zero_mv`: Live heater current, `364 µs` peak current, and auto-tracked Hall-sensor zero baseline (`PC1`).
