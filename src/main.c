#include "app_config.h"
#include "board.h"
#include "display.h"
#include "heater.h"
#include "input.h"
#include "menu.h"
#include "protocol.h"
#include "sensors.h"
#include "settings.h"
#include "ui.h"
#include "usb_cdc.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    opendws_settings_t settings;
    sensor_state_t ui_sensors;
    heater_status_t hst;
    menu_state_t menu;
    protocol_ctx_t *proto;
    uint8_t active_preset;
    uint8_t led_override;
    uint32_t save_toast_until_ms;
} app_ctx_t;

static void app_beep(const app_ctx_t *app, uint16_t ms) {
    if (app == NULL || app->settings.buzzer_en != 0u) {
        board_buzzer_beep(ms);
    }
}

static uint8_t find_preset_index(const opendws_settings_t *s, uint16_t temp_c) {
    for (uint8_t i = 0u; i < 3u; ++i) {
        if (s->presets_c[i] == temp_c) {
            return i;
        }
    }
    return 0xFFu;
}

static void sync_settings_from_runtime(app_ctx_t *app) {
    heater_get_status(&app->hst);
    app->settings.target_c = app->hst.target_c;
    app->settings.sleep_temp_c = app->hst.sleep_temp_c;
    app->settings.sleep_en = app->hst.sleep_en;
    app->settings.backlight_pct = board_get_backlight();
    if (app->proto != NULL) {
        app->settings.stream_enabled = app->proto->stream_enabled;
        app->settings.stream_interval_ms = app->proto->stream_interval_ms;
    }
}

static void proto_write_cb(const char *data, size_t len, void *user) {
    (void)user;
    (void)usb_cdc_write((const uint8_t *)data, len);
}

static void proto_bl_cb(uint8_t pct, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    board_set_backlight(pct);
    if (app != NULL) {
        app->settings.backlight_pct = board_get_backlight();
    }
}

static void proto_beep_cb(uint16_t ms, void *user) {
    (void)user;
    board_buzzer_beep(ms);
}

static void proto_led_cb(uint8_t mask, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    if (app != NULL) {
        app->led_override = 1u;
        app->settings.led_test_mask = mask;
    }
    board_set_leds(mask);
}

static void proto_leds_auto_cb(void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    if (app != NULL) {
        app->led_override = 0u;
    }
}

static void proto_reboot_cb(void *user) {
    (void)user;
    board_delay_ms(20u);
    board_reboot();
}

static heater_cmd_err_t proto_set_heater_cb(uint8_t enable, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    heater_cmd_err_t err = heater_set_enable(enable);
    if (app != NULL) {
        heater_get_status(&app->hst);
    }
    return err;
}

static heater_cmd_err_t proto_set_target_cb(uint16_t temp_c, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    heater_cmd_err_t err = heater_set_target(temp_c);
    if (err == HEATER_CMD_OK && app != NULL) {
        app->settings.target_c = temp_c;
        app->active_preset = find_preset_index(&app->settings, temp_c);
        heater_get_status(&app->hst);
    }
    return err;
}

static heater_cmd_err_t proto_select_preset_cb(uint8_t idx_1_to_3, uint16_t *out_temp_c, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    if (app == NULL || idx_1_to_3 < 1u || idx_1_to_3 > 3u) {
        return HEATER_CMD_ERR_RANGE;
    }
    uint8_t idx = (uint8_t)(idx_1_to_3 - 1u);
    uint16_t t = app->settings.presets_c[idx];
    heater_cmd_err_t err = heater_set_target(t);
    if (err == HEATER_CMD_OK) {
        app->settings.target_c = t;
        app->active_preset = idx;
        if (out_temp_c != NULL) {
            *out_temp_c = t;
        }
        heater_get_status(&app->hst);
    }
    return err;
}

static heater_cmd_err_t proto_set_sleep_cb(uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    heater_cmd_err_t err = heater_set_sleep(enable, sleep_temp_c, update_temp);
    if (err == HEATER_CMD_OK && app != NULL) {
        app->settings.sleep_en = enable ? 1u : 0u;
        if (update_temp != 0u) {
            app->settings.sleep_temp_c = sleep_temp_c;
        }
        heater_get_status(&app->hst);
    }
    return err;
}

static heater_cmd_err_t proto_set_cal_cb(int16_t offset_c, void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    heater_cmd_err_t err = heater_set_cal(offset_c);
    if (err == HEATER_CMD_OK && app != NULL) {
        heater_get_status(&app->hst);
        if (app->hst.is_f245 != 0u) {
            app->settings.cal_f245_deci_c = (int16_t)(offset_c * 10);
        } else {
            app->settings.cal_f210_deci_c = (int16_t)(offset_c * 10);
        }
    }
    return err;
}

static heater_cmd_err_t proto_clear_fault_cb(void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    heater_cmd_err_t err = heater_clear_fault();
    if (app != NULL) {
        heater_get_status(&app->hst);
    }
    return err;
}

static int proto_save_cb(void *user) {
    app_ctx_t *app = (app_ctx_t *)user;
    if (app == NULL) {
        return 0;
    }
    sync_settings_from_runtime(app);
    int ok = settings_save_flash(&app->settings);
    if (ok != 0) {
        app->save_toast_until_ms = board_ticks_ms() + 2000u;
        app_beep(app, 90u);
    }
    return ok;
}

static void update_auto_leds(const app_ctx_t *app, uint32_t now_ms) {
    if (app->led_override != 0u) {
        return;
    }

    uint16_t active_faults = app->hst.fault_mask & (uint16_t)~FAULT_SETTLING;
    uint8_t mask = 0u;
    if (app->hst.state == HEATER_FAULT || active_faults != 0u) {
        mask = ((now_ms / 250u) & 1u) ? 0xAAu : 0x55u;
    } else if (app->hst.state == HEATER_HEATING || app->hst.state == HEATER_PROBE) {
        uint32_t n = ((uint32_t)app->hst.duty_pct * 8u + 50u) / 100u;
        if (n == 0u) {
            mask = 0x01u;
        } else if (n >= 8u) {
            mask = 0xFFu;
        } else {
            mask = (uint8_t)((1u << n) - 1u);
        }
    } else if (app->hst.state == HEATER_SLEEP) {
        mask = ((now_ms / 500u) & 1u) ? 0x01u : 0x00u;
    }
    board_set_leds(mask);
}

static void adjust_target_step(app_ctx_t *app, int16_t delta_c) {
    int32_t next = (int32_t)app->hst.target_c + (int32_t)delta_c;
    if (next < (int32_t)TEMP_MIN_C) next = (int32_t)TEMP_MIN_C;
    if (next > (int32_t)TEMP_MAX_C) next = (int32_t)TEMP_MAX_C;
    if (heater_set_target((uint16_t)next) == HEATER_CMD_OK) {
        app->settings.target_c = (uint16_t)next;
        app->active_preset = find_preset_index(&app->settings, (uint16_t)next);
        heater_get_status(&app->hst);
    }
}

static void apply_menu_actions(app_ctx_t *app, uint32_t act, uint32_t now) {
    if ((act & MENU_ACT_CHANGED_SLEEP) != 0u) {
        (void)heater_set_sleep(app->settings.sleep_en, app->settings.sleep_temp_c, 1u);
        heater_get_status(&app->hst);
    }
    if ((act & MENU_ACT_CHANGED_PRESET) != 0u) {
        if (app->active_preset < 3u) {
            uint16_t p_temp = app->settings.presets_c[app->active_preset];
            if (heater_set_target(p_temp) == HEATER_CMD_OK) {
                app->settings.target_c = p_temp;
                heater_get_status(&app->hst);
            }
        } else {
            app->active_preset = find_preset_index(&app->settings, app->hst.target_c);
        }
    }
    if ((act & MENU_ACT_CHANGED_CAL) != 0u) {
        int16_t cal_dc = app->hst.is_f245 ? app->settings.cal_f245_deci_c : app->settings.cal_f210_deci_c;
        (void)heater_set_cal((int16_t)(cal_dc / 10));
        heater_get_status(&app->hst);
    }
    if ((act & MENU_ACT_CHANGED_BL) != 0u) {
        board_set_backlight(app->settings.backlight_pct);
    }
    if ((act & MENU_ACT_CHANGED_STREAM) != 0u && app->proto != NULL) {
        app->proto->stream_enabled = app->settings.stream_enabled;
        app->proto->last_stream_ms = now;
    }
    if ((act & MENU_ACT_SAVE_FLASH) != 0u) {
        sync_settings_from_runtime(app);
        if (settings_save_flash(&app->settings) != 0) {
            app->save_toast_until_ms = now + 2000u;
            app_beep(app, 100u);
        } else {
            board_buzzer_beep(220u);
        }
        return;
    }
    if ((act & MENU_ACT_BEEP_OK) != 0u) {
        app_beep(app, 35u);
    } else if ((act & MENU_ACT_BEEP_NAV) != 0u) {
        app_beep(app, 18u);
    }
}

int main(void) {
    board_init();
    board_heater_force_off();

    input_init();
    sensors_init();

    app_ctx_t app;
    sensors_state_init(&app.ui_sensors);
    settings_load_safe(&app.settings);
    menu_init(&app.menu);
    app.active_preset = find_preset_index(&app.settings, app.settings.target_c);
    app.led_override = 0u;
    app.save_toast_until_ms = 0u;

    /* Initialize PID/safety state + PB6->PC13..PC15 mux; ALWAYS boots in STANDBY */
    heater_init(
        app.settings.target_c,
        app.settings.sleep_temp_c,
        app.settings.sleep_en,
        app.settings.cal_f245_deci_c,
        app.settings.cal_f210_deci_c
    );

    display_init();
    board_set_backlight(app.settings.backlight_pct);

    usb_cdc_init();

    protocol_ops_t ops = {
        .write = proto_write_cb,
        .set_backlight = proto_bl_cb,
        .beep = proto_beep_cb,
        .set_leds = proto_led_cb,
        .set_leds_auto = proto_leds_auto_cb,
        .reboot = proto_reboot_cb,
        .set_heater = proto_set_heater_cb,
        .set_target = proto_set_target_cb,
        .select_preset = proto_select_preset_cb,
        .set_sleep = proto_set_sleep_cb,
        .set_cal = proto_set_cal_cb,
        .clear_fault = proto_clear_fault_cb,
        .save_settings = proto_save_cb,
        .user = &app
    };
    protocol_ctx_t proto;
    protocol_init(&proto, &ops, app.settings.stream_enabled, app.settings.stream_interval_ms);
    app.proto = &proto;

    /* Start TMR1 (1 ms blanking/PID) and TMR2 (91 us pulse/OC) */
    heater_hw_start();

    app_beep(&app, 60u);

    uint16_t tip_raw = 0u;
    uint16_t cur_raw = 2048u;
    uint16_t aux_raw = 0u;
    sensor_snapshot_t snap;
    input_state_t inp;

    heater_get_latest_adc(&tip_raw, &cur_raw, &aux_raw);
    heater_get_status(&app.hst);
    sensors_update_snapshot(&app.ui_sensors, tip_raw, cur_raw, aux_raw, &snap);
    sensors_record_temp_c(&app.ui_sensors, (uint16_t)(app.hst.tip_deci_c / 10), &snap);
    input_poll(board_ticks_ms(), &inp);

    ui_draw_static();

    ui_status_t ui_st;
    ui_st.hst = app.hst;
    ui_st.menu = app.menu;
    ui_st.settings = app.settings;
    ui_st.presets_c[0] = app.settings.presets_c[0];
    ui_st.presets_c[1] = app.settings.presets_c[1];
    ui_st.presets_c[2] = app.settings.presets_c[2];
    ui_st.active_preset = app.active_preset;
    ui_st.cal_offset_c = (int16_t)((app.hst.is_f245 ? app.settings.cal_f245_deci_c : app.settings.cal_f210_deci_c) / 10);
    ui_st.usb_connected = usb_cdc_is_connected();
    ui_st.stream_enabled = proto.stream_enabled;
    ui_st.stream_interval_ms = proto.stream_interval_ms;
    ui_st.backlight_pct = board_get_backlight();
    ui_st.led_mask = board_get_leds();
    ui_st.save_toast_active = 0u;
    ui_st.uptime_ms = board_ticks_ms();
    ui_draw_dynamic(&snap, &inp, &ui_st);

    uint32_t last_sample_ms = ui_st.uptime_ms;
    uint32_t last_ui_ms = ui_st.uptime_ms;
    uint8_t pd2_armed_on_main = 0u;
    uint8_t pd2_was_long = 0u;
    char cmd_buf[96];

    for (;;) {
        board_wdt_feed();

        uint32_t now = board_ticks_ms();
        board_buzzer_tick(now);
        usb_cdc_poll();

        input_poll(now, &inp);

        if (app.menu.open != 0u) {
            pd2_armed_on_main = 0u;
            pd2_was_long = 0u;
            if (inp.pressed_mask != 0u) {
                uint32_t act = menu_handle_key(&app.menu, inp.pressed_mask, &app.settings, app.hst.is_f245);
                apply_menu_actions(&app, act, now);
            }
        } else {
            /* Taste 5 (PB4): MENU -> Open On-Screen Settings Menu immediately */
            if (inp.pressed_mask & KEY_BIT_PB4) {
                pd2_armed_on_main = 0u;
                pd2_was_long = 0u;
                sync_settings_from_runtime(&app);
                uint32_t act = menu_handle_key(&app.menu, KEY_BIT_PB4, &app.settings, app.hst.is_f245);
                apply_menu_actions(&app, act, now);
            } else {
                /* Taste 1 (PC11): CH / Return -> Clear fault or cycle CH1/CH2/CH3 presets */
                if (inp.pressed_mask & KEY_BIT_PC11) {
                    heater_get_status(&app.hst);
                    if ((app.hst.fault_mask & (uint16_t)~FAULT_SETTLING) != 0u) {
                        heater_cmd_err_t err = heater_clear_fault();
                        app_beep(&app, (err == HEATER_CMD_OK) ? 40u : 150u);
                    } else {
                        uint8_t next_p = (app.active_preset < 2u) ? (uint8_t)(app.active_preset + 1u) : 0u;
                        uint16_t p_temp = app.settings.presets_c[next_p];
                        if (heater_set_target(p_temp) == HEATER_CMD_OK) {
                            app.settings.target_c = p_temp;
                            app.active_preset = next_p;
                        }
                        app_beep(&app, 25u);
                    }
                    heater_get_status(&app.hst);
                }

                /* Taste 2 (PC12): Pfeil links -> Target temp -5 C (long press: -25 C total) */
                if (inp.pressed_mask & KEY_BIT_PC12) {
                    adjust_target_step(&app, -5);
                    app_beep(&app, 20u);
                }
                if (inp.long_mask & KEY_BIT_PC12) {
                    adjust_target_step(&app, -20);
                    app_beep(&app, 45u);
                }

                /* Taste 3 (PD2): MODE / OK -> Short: toggle HEAT/STANDBY, Long: Save Flash */
                if (inp.pressed_mask & KEY_BIT_PD2) {
                    pd2_armed_on_main = 1u;
                    pd2_was_long = 0u;
                }
                if (pd2_armed_on_main != 0u && (inp.long_mask & KEY_BIT_PD2) != 0u) {
                    pd2_was_long = 1u;
                    sync_settings_from_runtime(&app);
                    if (settings_save_flash(&app.settings) != 0) {
                        app.save_toast_until_ms = now + 2000u;
                        app_beep(&app, 120u);
                    } else {
                        board_buzzer_beep(250u);
                    }
                }
                if (inp.released_mask & KEY_BIT_PD2) {
                    if (pd2_armed_on_main != 0u && pd2_was_long == 0u) {
                        heater_get_status(&app.hst);
                        heater_cmd_err_t err = heater_set_enable(app.hst.armed ? 0u : 1u);
                        if (err == HEATER_CMD_OK) {
                            app_beep(&app, 40u);
                        } else {
                            board_buzzer_beep(180u);
                        }
                        heater_get_status(&app.hst);
                    }
                    pd2_armed_on_main = 0u;
                    pd2_was_long = 0u;
                }

                /* Taste 4 (PB3): Pfeil rechts -> Target temp +5 C (long press: +25 C total) */
                if (inp.pressed_mask & KEY_BIT_PB3) {
                    adjust_target_step(&app, 5);
                    app_beep(&app, 20u);
                }
                if (inp.long_mask & KEY_BIT_PB3) {
                    adjust_target_step(&app, 20);
                    app_beep(&app, 45u);
                }
            }
        }

        /* Refresh filtered snapshots from ISR-captured ADC values (never contend for ADC1) */
        if ((uint32_t)(now - last_sample_ms) >= SENSOR_SAMPLE_MS) {
            last_sample_ms = now;
            heater_get_latest_adc(&tip_raw, &cur_raw, &aux_raw);
            heater_get_status(&app.hst);
            sensors_update_snapshot(&app.ui_sensors, tip_raw, cur_raw, aux_raw, &snap);
            sensors_record_temp_c(&app.ui_sensors, (uint16_t)(app.hst.tip_deci_c / 10), &snap);
        }

        while (usb_cdc_read_line(cmd_buf, sizeof(cmd_buf)) > 0u) {
            heater_get_status(&app.hst);
            protocol_handle_line(
                &proto,
                cmd_buf,
                &snap,
                &inp,
                &app.hst,
                board_get_backlight(),
                board_get_leds(),
                now
            );
        }

        heater_get_status(&app.hst);
        protocol_tick(
            &proto,
            &snap,
            &inp,
            &app.hst,
            board_get_backlight(),
            board_get_leds(),
            now
        );

        update_auto_leds(&app, now);

        if ((uint32_t)(now - last_ui_ms) >= UI_REFRESH_MS) {
            last_ui_ms = now;
            ui_st.hst = app.hst;
            ui_st.menu = app.menu;
            ui_st.settings = app.settings;
            ui_st.presets_c[0] = app.settings.presets_c[0];
            ui_st.presets_c[1] = app.settings.presets_c[1];
            ui_st.presets_c[2] = app.settings.presets_c[2];
            ui_st.active_preset = app.active_preset;
            ui_st.cal_offset_c = (int16_t)(
                (app.hst.is_f245 ? app.settings.cal_f245_deci_c : app.settings.cal_f210_deci_c) / 10
            );
            ui_st.usb_connected = usb_cdc_is_connected();
            ui_st.stream_enabled = proto.stream_enabled;
            ui_st.stream_interval_ms = proto.stream_interval_ms;
            ui_st.backlight_pct = board_get_backlight();
            ui_st.led_mask = board_get_leds();
            ui_st.save_toast_active = ((int32_t)(app.save_toast_until_ms - now) > 0) ? 1u : 0u;
            ui_st.uptime_ms = now;
            ui_draw_dynamic(&snap, &inp, &ui_st);
        }
    }

    return 0;
}
