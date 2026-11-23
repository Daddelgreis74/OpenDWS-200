#include "protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char out[1024];
    size_t len;
    uint8_t bl;
    uint16_t beep_ms;
    uint8_t leds;
    uint8_t rebooted;
} mock_ctx_t;

static void mock_write(const char *data, size_t len, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    if (m->len + len < sizeof(m->out)) {
        memcpy(m->out + m->len, data, len);
        m->len += len;
        m->out[m->len] = '\0';
    }
}

static void mock_bl(uint8_t pct, void *user) {
    ((mock_ctx_t *)user)->bl = pct;
}

static void mock_beep(uint16_t ms, void *user) {
    ((mock_ctx_t *)user)->beep_ms = ms;
}

static void mock_leds(uint8_t mask, void *user) {
    ((mock_ctx_t *)user)->leds = mask;
}

static void mock_reboot(void *user) {
    ((mock_ctx_t *)user)->rebooted = 1u;
}

static void reset_out(mock_ctx_t *m) {
    m->len = 0u;
    m->out[0] = '\0';
}

int main(void) {
    mock_ctx_t m;
    memset(&m, 0, sizeof(m));
    m.bl = 80u;

    protocol_ops_t ops = {
        .write = mock_write,
        .set_backlight = mock_bl,
        .beep = mock_beep,
        .set_leds = mock_leds,
        .reboot = mock_reboot,
        .user = &m
    };

    protocol_ctx_t proto;
    protocol_init(&proto, &ops, 0u, 100u);

    sensor_state_t sst;
    sensor_snapshot_t snap;
    sensors_state_init(&sst);
    sensors_update_snapshot(&sst, 345u, 2048u, 1500u, &snap);

    input_state_t inp = {
        .held_mask = KEY_BIT_PD2,
        .pb5_sleep = 1u,
        .pb6_handle = 0u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 7u
    };

    /* INFO */
    protocol_handle_line(&proto, "INFO\r\n", &snap, &inp, m.bl, m.leds, 1234u);
    assert(strstr(m.out, "INFO fw=") != NULL);
    assert(strstr(m.out, "mcu=AT32F415RCT7") != NULL);
    assert(strstr(m.out, "heater=LOCKED_OFF") != NULL);

    /* STAT */
    reset_out(&m);
    protocol_handle_line(&proto, "stat", &snap, &inp, m.bl, m.leds, 1234u);
    assert(strstr(m.out, "STAT ms=1234") != NULL);
    assert(strstr(m.out, "tip_adc=345") != NULL);
    assert(strstr(m.out, "cur_ma=0") != NULL);
    assert(strstr(m.out, "keys=0x01") != NULL);
    assert(strstr(m.out, "heater=0") != NULL);

    /* BL / BEEP / LED */
    reset_out(&m);
    protocol_handle_line(&proto, "BL 55", &snap, &inp, m.bl, m.leds, 1234u);
    assert(m.bl == 55u);
    assert(strcmp(m.out, "OK BL=55\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "BL 150", &snap, &inp, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "ERR BL_RANGE_0_100\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "BEEP 250", &snap, &inp, m.bl, m.leds, 1234u);
    assert(m.beep_ms == 250u);
    assert(strcmp(m.out, "OK BEEP=250\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "LED 0xA5", &snap, &inp, m.bl, m.leds, 1234u);
    assert(m.leds == 0xA5u);
    assert(strcmp(m.out, "OK LED=0xA5\r\n") == 0);

    /* STREAM */
    reset_out(&m);
    protocol_handle_line(&proto, "STREAM 200", &snap, &inp, m.bl, m.leds, 1000u);
    assert(proto.stream_enabled == 1u && proto.stream_interval_ms == 200u);
    assert(strcmp(m.out, "OK STREAM=ON INTERVAL=200\r\n") == 0);

    reset_out(&m);
    protocol_tick(&proto, &snap, &inp, m.bl, m.leds, 1100u);
    assert(m.len == 0u);
    protocol_tick(&proto, &snap, &inp, m.bl, m.leds, 1200u);
    assert(strstr(m.out, "STAT ms=1200") != NULL);

    /* Safety interlock commands must all be rejected */
    for (size_t i = 0u; i < 4u; ++i) {
        static const char *const blocked[4] = { "HEAT 1", "TEMP 350", "SET 300", "PID 1 2 3" };
        reset_out(&m);
        protocol_handle_line(&proto, blocked[i], &snap, &inp, m.bl, m.leds, 1300u);
        assert(strcmp(m.out, "ERR HEATER_LOCKED_IN_SAFE_MODE\r\n") == 0);
    }

    /* Unknown command */
    reset_out(&m);
    protocol_handle_line(&proto, "FOOBAR", &snap, &inp, m.bl, m.leds, 1300u);
    assert(strcmp(m.out, "ERR UNKNOWN_CMD\r\n") == 0);

    /* REBOOT */
    reset_out(&m);
    protocol_handle_line(&proto, "REBOOT", &snap, &inp, m.bl, m.leds, 1300u);
    assert(m.rebooted == 1u);
    assert(strcmp(m.out, "OK REBOOT\r\n") == 0);

    printf("test_protocol: PASS\n");
    return 0;
}
