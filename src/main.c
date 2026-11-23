#include "app_config.h"
#include "board.h"
#include "display.h"
#include "input.h"
#include "protocol.h"
#include "sensors.h"
#include "settings.h"
#include "ui.h"
#include "usb_cdc.h"

#include <stddef.h>
#include <stdint.h>

static void proto_write_cb(const char *data, size_t len, void *user) {
    (void)user;
    (void)usb_cdc_write((const uint8_t *)data, len);
}

static void proto_bl_cb(uint8_t pct, void *user) {
    (void)user;
    board_set_backlight(pct);
}

static void proto_beep_cb(uint16_t ms, void *user) {
    (void)user;
    board_buzzer_beep(ms);
}

static void proto_led_cb(uint8_t mask, void *user) {
    uint8_t *chase_en = (uint8_t *)user;
    if (chase_en != NULL) {
        *chase_en = 0u;
    }
    board_set_leds(mask);
}

static void proto_reboot_cb(void *user) {
    (void)user;
    board_delay_ms(20u);
    board_reboot();
}

int main(void) {
    board_init();
    board_heater_force_off();

    input_init();
    sensors_init();

    opendws_settings_t settings;
    settings_load_safe(&settings);

    display_init();
    board_set_backlight(settings.backlight_pct);

    usb_cdc_init();

    uint8_t led_chase_en = 0u;
    uint8_t led_chase_step = 0u;
    uint32_t last_chase_ms = 0u;

    protocol_ops_t ops = {
        .write = proto_write_cb,
        .set_backlight = proto_bl_cb,
        .beep = proto_beep_cb,
        .set_leds = proto_led_cb,
        .reboot = proto_reboot_cb,
        .user = &led_chase_en
    };
    protocol_ctx_t proto;
    protocol_init(&proto, &ops, settings.stream_enabled, settings.stream_interval_ms);

    if (settings.buzzer_en) {
        board_buzzer_beep(60u);
    }

    sensor_snapshot_t snap;
    input_state_t inp;
    sensors_sample_all(&snap);
    input_poll(board_ticks_ms(), &inp);

    ui_draw_static();

    ui_status_t ui_st = {
        .usb_connected = usb_cdc_is_connected(),
        .stream_enabled = proto.stream_enabled,
        .stream_interval_ms = proto.stream_interval_ms,
        .backlight_pct = board_get_backlight(),
        .led_mask = board_get_leds(),
        .uptime_ms = board_ticks_ms()
    };
    ui_draw_dynamic(&snap, &inp, &ui_st);

    uint32_t last_sample_ms = ui_st.uptime_ms;
    uint32_t last_ui_ms = ui_st.uptime_ms;
    char cmd_buf[96];

    for (;;) {
        board_heater_force_off();
        board_wdt_feed();

        uint32_t now = board_ticks_ms();
        board_buzzer_tick(now);
        usb_cdc_poll();

        input_poll(now, &inp);

        if (inp.pressed_mask & KEY_BIT_PD2) {
            led_chase_en = led_chase_en ? 0u : 1u;
            if (!led_chase_en) {
                board_set_leds(0u);
            } else {
                led_chase_step = 0u;
                last_chase_ms = now;
                board_set_leds(0x01u);
            }
            board_buzzer_beep(25u);
        }
        if (inp.pressed_mask & KEY_BIT_PB3) {
            uint8_t bl = board_get_backlight();
            bl = (bl > 10u) ? (uint8_t)(bl - 10u) : 10u;
            board_set_backlight(bl);
            board_buzzer_beep(25u);
        }
        if (inp.pressed_mask & KEY_BIT_PB4) {
            uint8_t bl = board_get_backlight();
            bl = (bl < 90u) ? (uint8_t)(bl + 10u) : 100u;
            board_set_backlight(bl);
            board_buzzer_beep(25u);
        }
        if (inp.pressed_mask & KEY_BIT_PC12) {
            proto.stream_enabled = proto.stream_enabled ? 0u : 1u;
            proto.last_stream_ms = now;
            board_buzzer_beep(25u);
        }
        if (inp.pressed_mask & KEY_BIT_PC11) {
            board_buzzer_beep(120u);
        }

        if (led_chase_en && (uint32_t)(now - last_chase_ms) >= 120u) {
            last_chase_ms = now;
            led_chase_step = (uint8_t)((led_chase_step + 1u) & 7u);
            board_set_leds((uint8_t)(1u << led_chase_step));
        }

        if ((uint32_t)(now - last_sample_ms) >= SENSOR_SAMPLE_MS) {
            last_sample_ms = now;
            sensors_sample_all(&snap);
        }

        while (usb_cdc_read_line(cmd_buf, sizeof(cmd_buf)) > 0u) {
            protocol_handle_line(
                &proto,
                cmd_buf,
                &snap,
                &inp,
                board_get_backlight(),
                board_get_leds(),
                now
            );
        }

        protocol_tick(&proto, &snap, &inp, board_get_backlight(), board_get_leds(), now);

        if ((uint32_t)(now - last_ui_ms) >= UI_REFRESH_MS) {
            last_ui_ms = now;
            ui_st.usb_connected = usb_cdc_is_connected();
            ui_st.stream_enabled = proto.stream_enabled;
            ui_st.stream_interval_ms = proto.stream_interval_ms;
            ui_st.backlight_pct = board_get_backlight();
            ui_st.led_mask = board_get_leds();
            ui_st.uptime_ms = now;
            ui_draw_dynamic(&snap, &inp, &ui_st);
        }
    }

    return 0;
}
