#include "protocol.h"

#include "app_config.h"
#include "fmt.h"
#include "runtime.h"

static void emit_str(protocol_ctx_t *ctx, const char *s) {
    if (ctx != NULL && ctx->ops.write != NULL && s != NULL) {
        size_t len = strlen(s);
        if (len > 0u) {
            ctx->ops.write(s, len, ctx->ops.user);
        }
    }
}

static size_t append_u32(char *buf, size_t pos, size_t cap, uint32_t val) {
    char tmp[16];
    fmt_uint_pad(tmp, sizeof(tmp), val, 1u, '0');
    return fmt_append_str(buf, pos, cap, tmp);
}

static size_t append_i32(char *buf, size_t pos, size_t cap, int32_t val) {
    char tmp[16];
    fmt_int_pad(tmp, sizeof(tmp), val, 1u, '0', 0u);
    return fmt_append_str(buf, pos, cap, tmp);
}

static size_t append_deci(char *buf, size_t pos, size_t cap, int32_t deci) {
    if (deci < 0) {
        pos = fmt_append_str(buf, pos, cap, "-");
        deci = -deci;
    }
    uint32_t whole = (uint32_t)deci / 10u;
    uint32_t frac = (uint32_t)deci % 10u;
    pos = append_u32(buf, pos, cap, whole);
    pos = fmt_append_str(buf, pos, cap, ".");
    return append_u32(buf, pos, cap, frac);
}

static size_t append_hex8(char *buf, size_t pos, size_t cap, uint8_t val) {
    char tmp[4];
    fmt_hex8(tmp, sizeof(tmp), val);
    pos = fmt_append_str(buf, pos, cap, "0x");
    return fmt_append_str(buf, pos, cap, tmp);
}

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static int eq_ci(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (to_upper(*a) != to_upper(*b)) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int prefix_ci(const char *s, const char *pfx) {
    while (*pfx != '\0') {
        if (to_upper(*s) != to_upper(*pfx)) return 0;
        s++;
        pfx++;
    }
    return (*s == '\0' || *s == ' ' || *s == '\t');
}

static int parse_u32(const char *s, uint32_t *out) {
    if (s == NULL || *s == '\0') return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return 0;

    uint32_t base = 10u;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16u;
        s += 2;
        if (*s == '\0') return 0;
    }

    uint32_t val = 0u;
    while (*s != '\0') {
        char c = *s++;
        if (c == ' ' || c == '\t') {
            while (*s == ' ' || *s == '\t') s++;
            if (*s != '\0') return 0;
            break;
        }
        uint32_t d;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (base == 16u && c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a' + 10);
        } else if (base == 16u && c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
        if (val > (0xFFFFFFFFu - d) / base) {
            return 0;
        }
        val = val * base + d;
    }
    *out = val;
    return 1;
}

static int parse_i32(const char *s, int32_t *out) {
    if (s == NULL || *s == '\0') return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return 0;

    int32_t sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    uint32_t u = 0u;
    if (!parse_u32(s, &u) || u > 0x7FFFFFFFu) {
        return 0;
    }
    *out = sign * (int32_t)u;
    return 1;
}

static void emit_heater_err(protocol_ctx_t *ctx, heater_cmd_err_t err) {
    emit_str(ctx, heater_err_msg(err));
    emit_str(ctx, "\r\n");
}

void protocol_init(
    protocol_ctx_t *ctx,
    const protocol_ops_t *ops,
    uint8_t stream_enabled,
    uint16_t stream_interval_ms
) {
    if (ctx == NULL) return;
    if (ops != NULL) {
        ctx->ops = *ops;
    } else {
        memset(&ctx->ops, 0, sizeof(ctx->ops));
    }
    ctx->stream_enabled = stream_enabled ? 1u : 0u;
    if (stream_interval_ms < STREAM_MIN_MS) stream_interval_ms = STREAM_MIN_MS;
    if (stream_interval_ms > STREAM_MAX_MS) stream_interval_ms = STREAM_MAX_MS;
    ctx->stream_interval_ms = stream_interval_ms;
    ctx->last_stream_ms = 0u;
}

void protocol_emit_stat(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
) {
    if (ctx == NULL || snap == NULL || inp == NULL) return;

    heater_status_t fallback;
    if (hst == NULL) {
        memset(&fallback, 0, sizeof(fallback));
        fallback.is_f245 = inp->pb6_handle;
        fallback.target_c = TEMP_DEFAULT_C;
        fallback.active_target_c = TEMP_DEFAULT_C;
        fallback.rail_mv = snap->rail_mv;
        fallback.cur_zero_mv = CUR_ZERO_DEFAULT_MV;
        fallback.state = HEATER_STANDBY;
        hst = &fallback;
    }

    char out[384];
    size_t p = 0u;
    p = fmt_append_str(out, p, sizeof(out), "STAT ms=");
    p = append_u32(out, p, sizeof(out), uptime_ms);
    p = fmt_append_str(out, p, sizeof(out), " state=");
    p = fmt_append_str(out, p, sizeof(out), heater_state_name(hst->state));
    p = fmt_append_str(out, p, sizeof(out), " fault=");
    p = append_hex8(out, p, sizeof(out), (uint8_t)(hst->fault_mask & 0xFFu));
    p = fmt_append_str(out, p, sizeof(out), "(");
    p = fmt_append_str(out, p, sizeof(out), heater_fault_name(hst->fault_mask));
    p = fmt_append_str(out, p, sizeof(out), ") handle=");
    p = fmt_append_str(out, p, sizeof(out), hst->is_f245 ? "F245" : "F210");
    p = fmt_append_str(out, p, sizeof(out), " tip_c=");
    p = append_deci(out, p, sizeof(out), hst->tip_deci_c);
    p = fmt_append_str(out, p, sizeof(out), " target_c=");
    p = append_u32(out, p, sizeof(out), hst->target_c);
    p = fmt_append_str(out, p, sizeof(out), " act_c=");
    p = append_u32(out, p, sizeof(out), hst->active_target_c);
    p = fmt_append_str(out, p, sizeof(out), " duty=");
    p = append_u32(out, p, sizeof(out), hst->duty_pct);
    p = fmt_append_str(out, p, sizeof(out), " ticks=");
    p = append_u32(out, p, sizeof(out), hst->duty_ticks);
    p = fmt_append_str(out, p, sizeof(out), " rail_mv=");
    p = append_u32(out, p, sizeof(out), hst->rail_mv);
    p = fmt_append_str(out, p, sizeof(out), " cur_ma=");
    p = append_i32(out, p, sizeof(out), hst->cur_ma);
    p = fmt_append_str(out, p, sizeof(out), " cur_pk=");
    p = append_i32(out, p, sizeof(out), hst->cur_peak_ma);
    p = fmt_append_str(out, p, sizeof(out), " zero_mv=");
    p = append_u32(out, p, sizeof(out), hst->cur_zero_mv);
    p = fmt_append_str(out, p, sizeof(out), " tip_adc=");
    p = append_u32(out, p, sizeof(out), snap->tip_avg);
    p = fmt_append_str(out, p, sizeof(out), " cur_adc=");
    p = append_u32(out, p, sizeof(out), snap->cur_avg);
    p = fmt_append_str(out, p, sizeof(out), " aux_adc=");
    p = append_u32(out, p, sizeof(out), snap->aux_avg);
    p = fmt_append_str(out, p, sizeof(out), " stand=");
    p = append_u32(out, p, sizeof(out), hst->in_stand);
    p = fmt_append_str(out, p, sizeof(out), " sleep=");
    p = append_u32(out, p, sizeof(out), hst->sleep_en);
    p = fmt_append_str(out, p, sizeof(out), " keys=");
    p = append_hex8(out, p, sizeof(out), inp->held_mask);
    p = fmt_append_str(out, p, sizeof(out), " pb5=");
    p = append_u32(out, p, sizeof(out), inp->pb5_sleep);
    p = fmt_append_str(out, p, sizeof(out), " pb6=");
    p = append_u32(out, p, sizeof(out), inp->pb6_handle);
    p = fmt_append_str(out, p, sizeof(out), " bl=");
    p = append_u32(out, p, sizeof(out), bl_pct);
    p = fmt_append_str(out, p, sizeof(out), " led=");
    p = append_hex8(out, p, sizeof(out), led_mask);
    p = fmt_append_str(out, p, sizeof(out), " heater=");
    p = append_u32(out, p, sizeof(out), hst->armed);
    p = fmt_append_str(out, p, sizeof(out), "\r\n");
    emit_str(ctx, out);
}

void protocol_tick(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t now_ms
) {
    if (ctx == NULL || ctx->stream_enabled == 0u) return;
    if ((uint32_t)(now_ms - ctx->last_stream_ms) >= ctx->stream_interval_ms) {
        ctx->last_stream_ms = now_ms;
        protocol_emit_stat(ctx, snap, inp, hst, bl_pct, led_mask, now_ms);
    }
}

void protocol_handle_line(
    protocol_ctx_t *ctx,
    const char *line,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
) {
    if (ctx == NULL || line == NULL) return;

    char cmd[96];
    size_t len = 0u;
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
    while (*line != '\0' && *line != '\r' && *line != '\n' && len + 1u < sizeof(cmd)) {
        cmd[len++] = *line++;
    }
    while (len > 0u && (cmd[len - 1u] == ' ' || cmd[len - 1u] == '\t')) {
        len--;
    }
    cmd[len] = '\0';
    if (len == 0u) return;

    if (eq_ci(cmd, "HELP") || eq_ci(cmd, "?")) {
        emit_str(ctx,
            "OK CMDS: HELP | INFO | STAT | HEAT <ON|OFF> | TEMP <100..450> | PRESET <1..3> | "
            "SLEEP <ON|OFF|50..250> | CAL <-50..50> | CLEAR | SAVE | STREAM [ON|OFF|50..2000] | "
            "BL <0..100> | BEEP [10..1000] | LED <0..255|AUTO> | REBOOT\r\n");
        return;
    }

    if (eq_ci(cmd, "INFO")) {
        heater_status_t fb;
        if (hst == NULL) {
            memset(&fb, 0, sizeof(fb));
            fb.is_f245 = (inp != NULL) ? inp->pb6_handle : 1u;
            fb.state = HEATER_STANDBY;
            fb.rail_mv = (snap != NULL) ? snap->rail_mv : 0u;
            hst = &fb;
        }
        char buf[200];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "INFO fw=");
        p = fmt_append_str(buf, p, sizeof(buf), OPENDWS_VERSION_STR);
        p = fmt_append_str(buf, p, sizeof(buf), " mcu=AT32F415RCT7 clk=144MHz safe_mode=0 handle=");
        p = fmt_append_str(buf, p, sizeof(buf), hst->is_f245 ? "F245" : "F210");
        p = fmt_append_str(buf, p, sizeof(buf), " state=");
        p = fmt_append_str(buf, p, sizeof(buf), heater_state_name(hst->state));
        p = fmt_append_str(buf, p, sizeof(buf), " rail_mv=");
        p = append_u32(buf, p, sizeof(buf), hst->rail_mv);
        p = fmt_append_str(buf, p, sizeof(buf), " uptime_ms=");
        p = append_u32(buf, p, sizeof(buf), uptime_ms);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (eq_ci(cmd, "STAT")) {
        protocol_emit_stat(ctx, snap, inp, hst, bl_pct, led_mask, uptime_ms);
        return;
    }

    if (prefix_ci(cmd, "HEAT")) {
        const char *arg = cmd + 4;
        while (*arg == ' ' || *arg == '\t') arg++;
        uint8_t enable;
        if (eq_ci(arg, "ON") || eq_ci(arg, "1")) {
            enable = 1u;
        } else if (eq_ci(arg, "OFF") || eq_ci(arg, "0")) {
            enable = 0u;
        } else {
            emit_str(ctx, "ERR USAGE_HEAT_ON_OFF\r\n");
            return;
        }
        if (ctx->ops.set_heater == NULL) {
            emit_str(ctx, "ERR UNSUPPORTED\r\n");
            return;
        }
        heater_cmd_err_t err = ctx->ops.set_heater(enable, ctx->ops.user);
        if (err != HEATER_CMD_OK) {
            emit_heater_err(ctx, err);
            return;
        }
        emit_str(ctx, enable ? "OK HEAT=ON\r\n" : "OK HEAT=OFF\r\n");
        return;
    }

    if (prefix_ci(cmd, "TEMP") || prefix_ci(cmd, "SET")) {
        const char *arg = prefix_ci(cmd, "TEMP") ? (cmd + 4) : (cmd + 3);
        uint32_t temp_c = 0u;
        if (!parse_u32(arg, &temp_c) || temp_c < TEMP_MIN_C || temp_c > TEMP_MAX_C) {
            emit_str(ctx, "ERR TEMP_RANGE_100_450\r\n");
            return;
        }
        if (ctx->ops.set_target != NULL) {
            heater_cmd_err_t err = ctx->ops.set_target((uint16_t)temp_c, ctx->ops.user);
            if (err != HEATER_CMD_OK) {
                emit_heater_err(ctx, err);
                return;
            }
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK TEMP=");
        p = append_u32(buf, p, sizeof(buf), temp_c);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "PRESET")) {
        uint32_t idx = 0u;
        if (!parse_u32(cmd + 6, &idx) || idx < 1u || idx > 3u) {
            emit_str(ctx, "ERR PRESET_RANGE_1_3\r\n");
            return;
        }
        uint16_t temp_c = TEMP_DEFAULT_C;
        if (ctx->ops.select_preset != NULL) {
            heater_cmd_err_t err = ctx->ops.select_preset((uint8_t)idx, &temp_c, ctx->ops.user);
            if (err != HEATER_CMD_OK) {
                emit_heater_err(ctx, err);
                return;
            }
        }
        char buf[48];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK PRESET=");
        p = append_u32(buf, p, sizeof(buf), idx);
        p = fmt_append_str(buf, p, sizeof(buf), " TEMP=");
        p = append_u32(buf, p, sizeof(buf), temp_c);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "SLEEP")) {
        const char *arg = cmd + 5;
        while (*arg == ' ' || *arg == '\t') arg++;
        if (eq_ci(arg, "ON")) {
            if (ctx->ops.set_sleep != NULL) {
                heater_cmd_err_t err = ctx->ops.set_sleep(1u, 0u, 0u, ctx->ops.user);
                if (err != HEATER_CMD_OK) {
                    emit_heater_err(ctx, err);
                    return;
                }
            }
            emit_str(ctx, "OK SLEEP=ON\r\n");
            return;
        }
        if (eq_ci(arg, "OFF")) {
            if (ctx->ops.set_sleep != NULL) {
                heater_cmd_err_t err = ctx->ops.set_sleep(0u, 0u, 0u, ctx->ops.user);
                if (err != HEATER_CMD_OK) {
                    emit_heater_err(ctx, err);
                    return;
                }
            }
            emit_str(ctx, "OK SLEEP=OFF\r\n");
            return;
        }
        uint32_t st = 0u;
        if (!parse_u32(arg, &st) || st < SLEEP_TEMP_MIN_C || st > SLEEP_TEMP_MAX_C) {
            emit_str(ctx, "ERR SLEEP_RANGE_50_250\r\n");
            return;
        }
        if (ctx->ops.set_sleep != NULL) {
            heater_cmd_err_t err = ctx->ops.set_sleep(1u, (uint16_t)st, 1u, ctx->ops.user);
            if (err != HEATER_CMD_OK) {
                emit_heater_err(ctx, err);
                return;
            }
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK SLEEP=");
        p = append_u32(buf, p, sizeof(buf), st);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "CAL")) {
        int32_t off = 0;
        if (!parse_i32(cmd + 3, &off) || off < CAL_OFFSET_MIN_C || off > CAL_OFFSET_MAX_C) {
            emit_str(ctx, "ERR CAL_RANGE_-50_50\r\n");
            return;
        }
        if (ctx->ops.set_cal != NULL) {
            heater_cmd_err_t err = ctx->ops.set_cal((int16_t)off, ctx->ops.user);
            if (err != HEATER_CMD_OK) {
                emit_heater_err(ctx, err);
                return;
            }
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK CAL=");
        p = append_i32(buf, p, sizeof(buf), off);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (eq_ci(cmd, "CLEAR")) {
        if (ctx->ops.clear_fault != NULL) {
            heater_cmd_err_t err = ctx->ops.clear_fault(ctx->ops.user);
            if (err != HEATER_CMD_OK) {
                emit_heater_err(ctx, err);
                return;
            }
        }
        emit_str(ctx, "OK CLEAR\r\n");
        return;
    }

    if (eq_ci(cmd, "SAVE")) {
        if (ctx->ops.save_settings != NULL && ctx->ops.save_settings(ctx->ops.user) != 0) {
            emit_str(ctx, "OK SAVE\r\n");
        } else {
            emit_str(ctx, "ERR SAVE_FAILED\r\n");
        }
        return;
    }

    if (prefix_ci(cmd, "STREAM")) {
        const char *arg = cmd + 6;
        while (*arg == ' ' || *arg == '\t') arg++;
        if (*arg == '\0' || eq_ci(arg, "ON")) {
            ctx->stream_enabled = 1u;
            ctx->last_stream_ms = uptime_ms;
        } else if (eq_ci(arg, "OFF")) {
            ctx->stream_enabled = 0u;
            emit_str(ctx, "OK STREAM=OFF\r\n");
            return;
        } else {
            uint32_t ms = 0u;
            if (!parse_u32(arg, &ms) || ms < STREAM_MIN_MS || ms > STREAM_MAX_MS) {
                emit_str(ctx, "ERR STREAM_RANGE_50_2000\r\n");
                return;
            }
            ctx->stream_interval_ms = (uint16_t)ms;
            ctx->stream_enabled = 1u;
            ctx->last_stream_ms = uptime_ms;
        }
        char buf[64];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK STREAM=ON INTERVAL=");
        p = append_u32(buf, p, sizeof(buf), ctx->stream_interval_ms);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "BL")) {
        uint32_t pct = 0u;
        if (!parse_u32(cmd + 2, &pct) || pct > 100u) {
            emit_str(ctx, "ERR BL_RANGE_0_100\r\n");
            return;
        }
        if (ctx->ops.set_backlight != NULL) {
            ctx->ops.set_backlight((uint8_t)pct, ctx->ops.user);
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK BL=");
        p = append_u32(buf, p, sizeof(buf), pct);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "BEEP")) {
        const char *arg = cmd + 4;
        while (*arg == ' ' || *arg == '\t') arg++;
        uint32_t ms = 80u;
        if (*arg != '\0') {
            if (!parse_u32(arg, &ms) || ms < 10u || ms > 1000u) {
                emit_str(ctx, "ERR BEEP_RANGE_10_1000\r\n");
                return;
            }
        }
        if (ctx->ops.beep != NULL) {
            ctx->ops.beep((uint16_t)ms, ctx->ops.user);
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK BEEP=");
        p = append_u32(buf, p, sizeof(buf), ms);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (prefix_ci(cmd, "LED")) {
        const char *arg = cmd + 3;
        while (*arg == ' ' || *arg == '\t') arg++;
        if (eq_ci(arg, "AUTO")) {
            if (ctx->ops.set_leds_auto != NULL) {
                ctx->ops.set_leds_auto(ctx->ops.user);
            }
            emit_str(ctx, "OK LED=AUTO\r\n");
            return;
        }
        uint32_t mask = 0u;
        if (!parse_u32(arg, &mask) || mask > 255u) {
            emit_str(ctx, "ERR LED_RANGE_0_255\r\n");
            return;
        }
        if (ctx->ops.set_leds != NULL) {
            ctx->ops.set_leds((uint8_t)mask, ctx->ops.user);
        }
        char buf[32];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "OK LED=");
        p = append_hex8(buf, p, sizeof(buf), (uint8_t)mask);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (eq_ci(cmd, "REBOOT")) {
        emit_str(ctx, "OK REBOOT\r\n");
        if (ctx->ops.reboot != NULL) {
            ctx->ops.reboot(ctx->ops.user);
        }
        return;
    }

    emit_str(ctx, "ERR UNKNOWN_CMD\r\n");
}
