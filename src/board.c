#include "board.h"

#include "app_config.h"
#include "hw.h"

static volatile uint32_t g_ticks_ms = 0u;
static uint32_t g_buzzer_off_ms = 0u;
static uint8_t g_buzzer_active = 0u;
static uint8_t g_backlight_pct = 80u;
static uint8_t g_led_mask = 0u;

#ifndef OPENDWS_HOST_TEST

void SysTick_Handler(void) {
    g_ticks_ms++;
    /* Safety invariant: keep heater PA2 LOW on every 1 ms tick */
    GPIO_CLR(GPIOA_BASE) = (1u << 2);
}

void board_heater_force_off(void) {
    GPIO_CLR(GPIOA_BASE) = (1u << 2);
}

static void board_clock_init(void) {
    /* Ensure HICK is active and switch back to HICK before reconfiguring PLL */
    CRM_CTRL |= CRM_CTRL_HICKEN;
    while ((CRM_CTRL & CRM_CTRL_HICKSTBL) == 0u) {
    }
    CRM_CFG &= ~0x3u;
    while ((CRM_CFG & (0x3u << 2)) != 0u) {
    }

    CRM_CTRL &= ~(CRM_CTRL_PLLEN | CRM_CTRL_HEXTEN);
    CRM_CFG = 0u;
    CRM_PLL = 0x00001F10u;
    CRM_MISC1 = 0x00100000u;
    CRM_MISC2 = 0x00000000u;

    /* Flash latency + prefetch for 144 MHz (matches stock V1.5 FLASH_PSR |= 0x154) */
    FLASH_PSR |= 0x154u;

    /* Enable 8 MHz HEXT crystal with bounded timeout */
    CRM_CTRL |= CRM_CTRL_HEXTEN;
    uint32_t timeout = 1000000u;
    while ((CRM_CTRL & CRM_CTRL_HEXTSTBL) == 0u && timeout > 0u) {
        timeout--;
    }

    if ((CRM_CTRL & CRM_CTRL_HEXTSTBL) != 0u) {
        /*
         * PLL = (HEXT / 2) * 36 = 4 MHz * 36 = 144 MHz
         * AHB = /1 (144 MHz), APB1 = /2 (72 MHz), APB2 = /2 (72 MHz)
         * ADC = /6 (12 MHz), USB = /3 (144 MHz / 3 = 48 MHz)
         * PLLMULT = 0x23 (0b10_0011): L[21:18]=0x3, H[30:29]=0x2
         * USBDIV  = 0b101 (/3): L[23:22]=0b01, H[27]=1 => 0x08400000
         */
        uint32_t cfg = 0u;
        cfg |= (0u << 4);   /* AHBDIV = /1 */
        cfg |= (4u << 8);   /* APB1DIV = /2 */
        cfg |= (4u << 11);  /* APB2DIV = /2 */
        cfg |= (2u << 14);  /* ADCDIV = /6 (bit 28 = 0) */
        cfg |= (1u << 16);  /* PLLSRC = HEXT */
        cfg |= (1u << 17);  /* PLLHEXTDIV = HEXT / 2 */
        cfg |= (0x3u << 18);/* PLLMULT_L = 0b0011 */
        cfg |= (0x2u << 29);/* PLLMULT_H = 0b10 */
        cfg |= (1u << 22);  /* USBDIV_L = 0b01 */
        cfg |= (1u << 27);  /* USBDIV_H = 1 */
        CRM_CFG = cfg;

        CRM_CTRL |= CRM_CTRL_PLLEN;
        while ((CRM_CTRL & CRM_CTRL_PLLSTBL) == 0u) {
        }

        /* Auto-step mode required on AT32F415 when switching to >108 MHz PLL */
        CRM_MISC2 |= (0x3u << 4);
        CRM_CFG = (CRM_CFG & ~0x3u) | 0x2u;
        while ((CRM_CFG & (0x3u << 2)) != (0x2u << 2)) {
        }
        CRM_MISC2 &= ~(0x3u << 4);
    }

    /* Route PLL/USBDIV (48 MHz) rather than HICK to USB OTGFS1 */
    CRM_MISC1 &= ~(1u << 24);
}

static void wdt_init(void) {
    /* LICK (~40 kHz) / 64 = ~625 Hz; RLD = 1250 -> ~2.0 s timeout */
    WDT_CMD = WDT_CMD_UNLOCK;
    WDT_DIV = 4u;
    WDT_RLD = 1250u;
    WDT_CMD = WDT_CMD_RELOAD;
    WDT_CMD = WDT_CMD_ENABLE;
}

void board_init(void) {
    board_clock_init();

    CRM_APB2EN |= CRM_APB2_IOMUX | CRM_APB2_GPIOA | CRM_APB2_GPIOB |
                  CRM_APB2_GPIOC | CRM_APB2_GPIOD | CRM_APB2_GPIOF;
    CRM_APB1EN |= CRM_APB1_TMR3 | CRM_APB1_PWC;

    /*
     * IOMUX_REMAP / IOMUX_REMAP4 (verified from APP_DWS_200_V1.5 0x08013152):
     * - IOMUX_REMAP  SWJTAG_CONF[26:24] = 0b010 -> SWD enabled, JTAG disabled (frees PB3 & PB4!)
     * - IOMUX_REMAP4 TMR3_GMUX[11:8]    = 0x3   -> Full remap (PC8 = CH3 Buzzer, PC9 = CH4 Backlight)
     */
    IOMUX_REMAP = (IOMUX_REMAP & ~(0x7u << 24)) | (0x2u << 24);
    IOMUX_REMAP4 = (IOMUX_REMAP4 & ~(0xFu << 8)) | (0x3u << 8);

    /* PA2: Heater control -> strictly output push-pull, forced LOW */
    gpio_config_pin(GPIOA_BASE, 2u, GPIO_MODE_OUT_PP_50M, 0u);
    board_heater_force_off();

    /* PB1: Stock board enable/power latch -> output push-pull HIGH */
    gpio_config_pin(GPIOB_BASE, 1u, GPIO_MODE_OUT_PP_50M, 0u);
    GPIO_SCR(GPIOB_BASE) = (1u << 1);

    /* 8 Front-panel LEDs: PF4, PF5, PA4..PA7, PC4, PC5 -> output push-pull LOW */
    gpio_config_pin(GPIOF_BASE, 4u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOF_BASE, 5u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOA_BASE, 4u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOA_BASE, 5u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOA_BASE, 6u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOA_BASE, 7u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOC_BASE, 4u, GPIO_MODE_OUT_PP_2M, 0u);
    gpio_config_pin(GPIOC_BASE, 5u, GPIO_MODE_OUT_PP_2M, 0u);
    board_set_leds(0u);

    /* PC8 (TMR3_CH3 Buzzer) and PC9 (TMR3_CH4 Backlight) */
    gpio_config_pin(GPIOC_BASE, 8u, GPIO_MODE_AF_PP_50M, 0u);
    gpio_config_pin(GPIOC_BASE, 9u, GPIO_MODE_AF_PP_50M, 0u);

    /* TMR3: 144 MHz / 53 = 2.717 MHz, Period 999 -> ~2.717 kHz PWM */
    TMR_CTRL1(TMR3_BASE) = 0u;
    TMR_DIV(TMR3_BASE) = (TMR3_CLK_HZ / 2700000u) - 1u;
    TMR_PR(TMR3_BASE) = 999u;
    TMR_CM2(TMR3_BASE) = 0x6868u; /* CH3 & CH4: PWM1 mode + preload enable */
    TMR_CCTRL(TMR3_BASE) = (1u << 8) | (1u << 12); /* Enable CH3 & CH4 output */
    TMR_C3DT(TMR3_BASE) = 0u;
    TMR_C4DT(TMR3_BASE) = 800u;
    TMR_SWEVT(TMR3_BASE) = 1u;
    TMR_CTRL1(TMR3_BASE) = (1u << 7) | (1u << 0); /* PRBEN | TMREN */
    g_backlight_pct = 80u;

    /* SysTick: 1 kHz */
    SYST_RVR = (SYSCLK_HZ / 1000u) - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 0x7u;

    wdt_init();
}

void board_set_backlight(uint8_t pct) {
    if (pct > 100u) {
        pct = 100u;
    }
    g_backlight_pct = pct;
    TMR_C4DT(TMR3_BASE) = (uint32_t)pct * 10u;
}

uint8_t board_get_backlight(void) {
    return g_backlight_pct;
}

void board_buzzer_beep(uint16_t ms) {
    if (ms == 0u) {
        TMR_C3DT(TMR3_BASE) = 0u;
        g_buzzer_active = 0u;
        return;
    }
    if (ms > 1000u) {
        ms = 1000u;
    }
    TMR_C3DT(TMR3_BASE) = 500u;
    g_buzzer_off_ms = g_ticks_ms + (uint32_t)ms;
    g_buzzer_active = 1u;
}

void board_buzzer_tick(uint32_t now_ms) {
    if (g_buzzer_active != 0u && (int32_t)(now_ms - g_buzzer_off_ms) >= 0) {
        TMR_C3DT(TMR3_BASE) = 0u;
        g_buzzer_active = 0u;
    }
}

void board_set_leds(uint8_t mask) {
    g_led_mask = mask;
    /* Bit 0: PF4, Bit 1: PF5 */
    if (mask & (1u << 0)) GPIO_SCR(GPIOF_BASE) = (1u << 4); else GPIO_CLR(GPIOF_BASE) = (1u << 4);
    if (mask & (1u << 1)) GPIO_SCR(GPIOF_BASE) = (1u << 5); else GPIO_CLR(GPIOF_BASE) = (1u << 5);
    /* Bit 2..5: PA4..PA7 */
    if (mask & (1u << 2)) GPIO_SCR(GPIOA_BASE) = (1u << 4); else GPIO_CLR(GPIOA_BASE) = (1u << 4);
    if (mask & (1u << 3)) GPIO_SCR(GPIOA_BASE) = (1u << 5); else GPIO_CLR(GPIOA_BASE) = (1u << 5);
    if (mask & (1u << 4)) GPIO_SCR(GPIOA_BASE) = (1u << 6); else GPIO_CLR(GPIOA_BASE) = (1u << 6);
    if (mask & (1u << 5)) GPIO_SCR(GPIOA_BASE) = (1u << 7); else GPIO_CLR(GPIOA_BASE) = (1u << 7);
    /* Bit 6..7: PC4..PC5 */
    if (mask & (1u << 6)) GPIO_SCR(GPIOC_BASE) = (1u << 4); else GPIO_CLR(GPIOC_BASE) = (1u << 4);
    if (mask & (1u << 7)) GPIO_SCR(GPIOC_BASE) = (1u << 5); else GPIO_CLR(GPIOC_BASE) = (1u << 5);
}

uint8_t board_get_leds(void) {
    return g_led_mask;
}

uint32_t board_ticks_ms(void) {
    return g_ticks_ms;
}

void board_delay_ms(uint32_t ms) {
    uint32_t start = g_ticks_ms;
    while ((uint32_t)(g_ticks_ms - start) < ms) {
        board_heater_force_off();
        board_wdt_feed();
    }
}

void board_wdt_feed(void) {
    WDT_CMD = WDT_CMD_RELOAD;
}

void board_reboot(void) {
    board_heater_force_off();
    SCB_AIRCR = 0x05FA0004u;
    for (;;) {
        board_heater_force_off();
    }
}

#else

void board_init(void) {}
void board_heater_force_off(void) {}
void board_set_backlight(uint8_t pct) { g_backlight_pct = pct <= 100u ? pct : 100u; }
uint8_t board_get_backlight(void) { return g_backlight_pct; }
void board_buzzer_beep(uint16_t ms) { g_buzzer_off_ms = g_ticks_ms + ms; g_buzzer_active = ms ? 1u : 0u; }
void board_buzzer_tick(uint32_t now_ms) { if (g_buzzer_active && (int32_t)(now_ms - g_buzzer_off_ms) >= 0) g_buzzer_active = 0u; }
void board_set_leds(uint8_t mask) { g_led_mask = mask; }
uint8_t board_get_leds(void) { return g_led_mask; }
uint32_t board_ticks_ms(void) { return g_ticks_ms; }
void board_delay_ms(uint32_t ms) { g_ticks_ms += ms; }
void board_wdt_feed(void) {}
void board_reboot(void) {}

#endif /* OPENDWS_HOST_TEST */
