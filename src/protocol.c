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
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
) {
    if (ctx == NULL || snap == NULL || inp == NULL) return;

    char out[240];
    size_t p = 0u;
    p = fmt_append_str(out, p, sizeof(out), "STAT ms=");
    p = append_u32(out, p, sizeof(out), uptime_ms);
    p = fmt_append_str(out, p, sizeof(out), " tip_adc=");
    p = append_u32(out, p, sizeof(out), snap->tip_avg);
    p = fmt_append_str(out, p, sizeof(out), " tip_mv=");
    p = append_u32(out, p, sizeof(out), snap->tip_mv);
    p = fmt_append_str(out, p, sizeof(out), " cur_adc=");
    p = append_u32(out, p, sizeof(out), snap->cur_avg);
    p = fmt_append_str(out, p, sizeof(out), " cur_mv=");
    p = append_u32(out, p, sizeof(out), snap->cur_mv);
    p = fmt_append_str(out, p, sizeof(out), " cur_ma=");
    p = append_i32(out, p, sizeof(out), snap->cur_ma);
    p = fmt_append_str(out, p, sizeof(out), " aux_adc=");
    p = append_u32(out, p, sizeof(out), snap->aux_avg);
    p = fmt_append_str(out, p, sizeof(out), " aux_mv=");
    p = append_u32(out, p, sizeof(out), snap->aux_mv);
    p = fmt_append_str(out, p, sizeof(out), " keys=");
    p = append_hex8(out, p, sizeof(out), inp->held_mask);
    p = fmt_append_str(out, p, sizeof(out), " pb5=");
    p = append_u32(out, p, sizeof(out), inp->pb5_sleep);
    p = fmt_append_str(out, p, sizeof(out), " pb5_cnt=");
    p = append_u32(out, p, sizeof(out), inp->pb5_edges);
    p = fmt_append_str(out, p, sizeof(out), " pb6=");
    p = append_u32(out, p, sizeof(out), inp->pb6_handle);
    p = fmt_append_str(out, p, sizeof(out), " pb0=");
    p = append_u32(out, p, sizeof(out), inp->pb0_raw);
    p = fmt_append_str(out, p, sizeof(out), " pb2=");
    p = append_u32(out, p, sizeof(out), inp->pb2_raw);
    p = fmt_append_str(out, p, sizeof(out), " bl=");
    p = append_u32(out, p, sizeof(out), bl_pct);
    p = fmt_append_str(out, p, sizeof(out), " led=");
    p = append_hex8(out, p, sizeof(out), led_mask);
    p = fmt_append_str(out, p, sizeof(out), " heater=0\r\n");
    emit_str(ctx, out);
}

void protocol_tick(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t now_ms
) {
    if (ctx == NULL || ctx->stream_enabled == 0u) return;
    if ((uint32_t)(now_ms - ctx->last_stream_ms) >= ctx->stream_interval_ms) {
        ctx->last_stream_ms = now_ms;
        protocol_emit_stat(ctx, snap, inp, bl_pct, led_mask, now_ms);
    }
}

void protocol_handle_line(
    protocol_ctx_t *ctx,
    const char *line,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
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
            "OK CMDS: HELP | INFO | STAT | STREAM [ON|OFF|50..2000] | BL <0..100> | BEEP [10..1000] | LED <0..255> | REBOOT\r\n");
        return;
    }

    if (eq_ci(cmd, "INFO")) {
        char buf[160];
        size_t p = 0u;
        p = fmt_append_str(buf, p, sizeof(buf), "INFO fw=");
        p = fmt_append_str(buf, p, sizeof(buf), OPENDWS_VERSION_STR);
        p = fmt_append_str(buf, p, sizeof(buf), " mcu=AT32F415RCT7 clk=144MHz safe_mode=1 heater=LOCKED_OFF uptime_ms=");
        p = append_u32(buf, p, sizeof(buf), uptime_ms);
        (void)fmt_append_str(buf, p, sizeof(buf), "\r\n");
        emit_str(ctx, buf);
        return;
    }

    if (eq_ci(cmd, "STAT")) {
        protocol_emit_stat(ctx, snap, inp, bl_pct, led_mask, uptime_ms);
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
        uint32_t mask = 0u;
        if (!parse_u32(cmd + 3, &mask) || mask > 255u) {
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

    /* Phase 1 hardware safety interlock: reject any heater/temp/PID commands */
    if (prefix_ci(cmd, "HEAT") || prefix_ci(cmd, "HEATER") ||
        prefix_ci(cmd, "TEMP") || prefix_ci(cmd, "SET") || prefix_ci(cmd, "PID")) {
        emit_str(ctx, "ERR HEATER_LOCKED_IN_SAFE_MODE\r\n");
        return;
    }

    emit_str(ctx, "ERR UNKNOWN_CMD\r\n");
}
