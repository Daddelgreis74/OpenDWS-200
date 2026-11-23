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
    uint8_t leds_auto;
    uint8_t rebooted;
    uint8_t saved;
    heater_status_t hst;
} mock_ctx_t;

static void mock_write(const char *data, size_t len, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    if (m->len + len < sizeof(m->out)) {
        memcpy(m->out + m->len, data, len);
        m->len += len;
        m->out[m->len] = '\0';
    }
}

static void mock_bl(uint8_t pct, void *user) { ((mock_ctx_t *)user)->bl = pct; }
static void mock_beep(uint16_t ms, void *user) { ((mock_ctx_t *)user)->beep_ms = ms; }
static void mock_leds(uint8_t mask, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    m->leds = mask;
    m->leds_auto = 0u;
}
static void mock_leds_auto(void *user) { ((mock_ctx_t *)user)->leds_auto = 1u; }
static void mock_reboot(void *user) { ((mock_ctx_t *)user)->rebooted = 1u; }

static heater_cmd_err_t mock_set_heater(uint8_t en, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    if (en != 0u && m->hst.rail_mv < RAIL_MIN_MV) {
        return HEATER_CMD_ERR_NO_24V;
    }
    m->hst.armed = en ? 1u : 0u;
    m->hst.state = en ? HEATER_HEATING : HEATER_STANDBY;
    return HEATER_CMD_OK;
}

static heater_cmd_err_t mock_set_target(uint16_t t, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    m->hst.target_c = t;
    m->hst.active_target_c = t;
    return HEATER_CMD_OK;
}

static heater_cmd_err_t mock_select_preset(uint8_t idx, uint16_t *out_t, void *user) {
    static const uint16_t p[3] = { 200u, 300u, 350u };
    mock_ctx_t *m = (mock_ctx_t *)user;
    m->hst.target_c = p[idx - 1u];
    if (out_t != NULL) *out_t = m->hst.target_c;
    return HEATER_CMD_OK;
}

static heater_cmd_err_t mock_set_sleep(uint8_t en, uint16_t st, uint8_t upd, void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    m->hst.sleep_en = en;
    if (upd != 0u) m->hst.sleep_temp_c = st;
    return HEATER_CMD_OK;
}

static heater_cmd_err_t mock_set_cal(int16_t off_c, void *user) {
    (void)off_c;
    (void)user;
    return HEATER_CMD_OK;
}

static heater_cmd_err_t mock_clear_fault(void *user) {
    mock_ctx_t *m = (mock_ctx_t *)user;
    m->hst.fault_mask = FAULT_NONE;
    return HEATER_CMD_OK;
}

static int mock_save(void *user) {
    ((mock_ctx_t *)user)->saved = 1u;
    return 1;
}

static void reset_out(mock_ctx_t *m) {
    m->len = 0u;
    m->out[0] = '\0';
}

int main(void) {
    mock_ctx_t m;
    memset(&m, 0, sizeof(m));
    m.bl = 80u;
    m.hst.state = HEATER_STANDBY;
    m.hst.is_f245 = 1u;
    m.hst.target_c = 300u;
    m.hst.active_target_c = 300u;
    m.hst.sleep_temp_c = 100u;
    m.hst.sleep_en = 1u;
    m.hst.tip_deci_c = 245;
    m.hst.rail_mv = 22920u;
    m.hst.cur_zero_mv = 1642u;

    protocol_ops_t ops = {
        .write = mock_write,
        .set_backlight = mock_bl,
        .beep = mock_beep,
        .set_leds = mock_leds,
        .set_leds_auto = mock_leds_auto,
        .reboot = mock_reboot,
        .set_heater = mock_set_heater,
        .set_target = mock_set_target,
        .select_preset = mock_select_preset,
        .set_sleep = mock_set_sleep,
        .set_cal = mock_set_cal,
        .clear_fault = mock_clear_fault,
        .save_settings = mock_save,
        .user = &m
    };

    protocol_ctx_t proto;
    protocol_init(&proto, &ops, 0u, 100u);

    sensor_state_t sst;
    sensor_snapshot_t snap;
    sensors_state_init(&sst);
    sensors_update_snapshot(&sst, 345u, 2038u, 1779u, &snap);

    input_state_t inp = {
        .held_mask = KEY_BIT_PD2,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 7u
    };

    /* INFO */
    protocol_handle_line(&proto, "INFO\r\n", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strstr(m.out, "INFO fw=") != NULL);
    assert(strstr(m.out, "safe_mode=0") != NULL);
    assert(strstr(m.out, "handle=F245") != NULL);

    /* STAT */
    reset_out(&m);
    protocol_handle_line(&proto, "stat", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strstr(m.out, "STAT ms=1234") != NULL);
    assert(strstr(m.out, "tip_c=24.5") != NULL);
    assert(strstr(m.out, "rail_mv=22920") != NULL);
    assert(strstr(m.out, "heater=0") != NULL);

    /* HEAT blocked when < 15 V USB-only */
    m.hst.rail_mv = 2320u;
    reset_out(&m);
    protocol_handle_line(&proto, "HEAT ON", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "ERR NO_24V_RAIL\r\n") == 0);
    assert(m.hst.armed == 0u);

    /* HEAT succeeds on 24 V rail */
    m.hst.rail_mv = 22920u;
    reset_out(&m);
    protocol_handle_line(&proto, "HEAT ON", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK HEAT=ON\r\n") == 0);
    assert(m.hst.armed == 1u);

    reset_out(&m);
    protocol_handle_line(&proto, "HEAT OFF", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK HEAT=OFF\r\n") == 0);
    assert(m.hst.armed == 0u);

    /* TEMP, PRESET, SLEEP, CAL, CLEAR, SAVE */
    reset_out(&m);
    protocol_handle_line(&proto, "TEMP 340", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK TEMP=340\r\n") == 0);
    assert(m.hst.target_c == 340u);

    reset_out(&m);
    protocol_handle_line(&proto, "TEMP 600", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "ERR TEMP_RANGE_100_450\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "PRESET 3", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK PRESET=3 TEMP=350\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "SLEEP OFF", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK SLEEP=OFF\r\n") == 0);
    assert(m.hst.sleep_en == 0u);

    reset_out(&m);
    protocol_handle_line(&proto, "SLEEP 90", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK SLEEP=90\r\n") == 0);
    assert(m.hst.sleep_temp_c == 90u);

    reset_out(&m);
    protocol_handle_line(&proto, "SLEEP 30", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "ERR SLEEP_RANGE_50_250\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "CAL -12", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(strcmp(m.out, "OK CAL=-12\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "SAVE", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(m.saved == 1u);
    assert(strcmp(m.out, "OK SAVE\r\n") == 0);

    /* BL / BEEP / LED / AUTO */
    reset_out(&m);
    protocol_handle_line(&proto, "BL 55", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(m.bl == 55u);
    assert(strcmp(m.out, "OK BL=55\r\n") == 0);

    reset_out(&m);
    protocol_handle_line(&proto, "BEEP 250", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(m.beep_ms == 250u);

    reset_out(&m);
    protocol_handle_line(&proto, "LED 0xA5", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(m.leds == 0xA5u && m.leds_auto == 0u);

    reset_out(&m);
    protocol_handle_line(&proto, "LED AUTO", &snap, &inp, &m.hst, m.bl, m.leds, 1234u);
    assert(m.leds_auto == 1u);
    assert(strcmp(m.out, "OK LED=AUTO\r\n") == 0);

    /* STREAM & REBOOT */
    reset_out(&m);
    protocol_handle_line(&proto, "STREAM 200", &snap, &inp, &m.hst, m.bl, m.leds, 1000u);
    assert(proto.stream_enabled == 1u && proto.stream_interval_ms == 200u);

    reset_out(&m);
    protocol_tick(&proto, &snap, &inp, &m.hst, m.bl, m.leds, 1200u);
    assert(strstr(m.out, "STAT ms=1200") != NULL);

    reset_out(&m);
    protocol_handle_line(&proto, "REBOOT", &snap, &inp, &m.hst, m.bl, m.leds, 1300u);
    assert(m.rebooted == 1u);

    printf("test_protocol: PASS\n");
    return 0;
}
