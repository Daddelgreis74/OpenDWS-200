#include "display.h"

#include "app_config.h"
#include "board.h"
#include "hw.h"

#include <stddef.h>

#ifndef OPENDWS_HOST_TEST

#define LCD_CS_LOW()   (GPIO_CLR(GPIOB_BASE) = (1u << 12))
#define LCD_CS_HIGH()  (GPIO_SCR(GPIOB_BASE) = (1u << 12))
#define LCD_DC_CMD()   (GPIO_CLR(GPIOC_BASE) = (1u << 6))
#define LCD_DC_DATA()  (GPIO_SCR(GPIOC_BASE) = (1u << 6))
#define LCD_RST_LOW()  (GPIO_CLR(GPIOC_BASE) = (1u << 7))
#define LCD_RST_HIGH() (GPIO_SCR(GPIOC_BASE) = (1u << 7))

static inline void spi2_write_byte(uint8_t byte) {
    while ((SPI_STS(SPI2_BASE) & SPI_STS_TDBE) == 0u) {
    }
    SPI_DT(SPI2_BASE) = (uint32_t)byte;
}

static inline void spi2_wait_idle(void) {
    while ((SPI_STS(SPI2_BASE) & SPI_STS_TDBE) == 0u) {
    }
    while ((SPI_STS(SPI2_BASE) & SPI_STS_BF) != 0u) {
    }
}

static void lcd_cmd(uint8_t cmd) {
    LCD_CS_LOW();
    LCD_DC_CMD();
    spi2_write_byte(cmd);
    spi2_wait_idle();
    LCD_CS_HIGH();
}

static void lcd_data8(uint8_t data) {
    LCD_CS_LOW();
    LCD_DC_DATA();
    spi2_write_byte(data);
    spi2_wait_idle();
    LCD_CS_HIGH();
}

static void lcd_cmd_data(uint8_t cmd, const uint8_t *data, uint32_t len) {
    lcd_cmd(cmd);
    if (len == 0u) {
        return;
    }
    LCD_CS_LOW();
    LCD_DC_DATA();
    for (uint32_t i = 0u; i < len; ++i) {
        spi2_write_byte(data[i]);
    }
    spi2_wait_idle();
    LCD_CS_HIGH();
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t caset[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFFu),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFFu)
    };
    const uint8_t raset[4] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFFu),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFFu)
    };
    lcd_cmd_data(0x2Au, caset, 4u);
    lcd_cmd_data(0x2Bu, raset, 4u);
    lcd_cmd(0x2Cu);
}

void display_init(void) {
    CRM_APB2EN |= CRM_APB2_GPIOB | CRM_APB2_GPIOC;
    CRM_APB1EN |= CRM_APB1_SPI2;

    /* PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI */
    gpio_config_pin(GPIOB_BASE, 12u, GPIO_MODE_OUT_PP_50M, 0u);
    gpio_config_pin(GPIOB_BASE, 13u, GPIO_MODE_AF_PP_50M, 0u);
    gpio_config_pin(GPIOB_BASE, 14u, GPIO_MODE_IN_FLOAT, 0u);
    gpio_config_pin(GPIOB_BASE, 15u, GPIO_MODE_AF_PP_50M, 0u);

    /* PC6=DC, PC7=RST */
    gpio_config_pin(GPIOC_BASE, 6u, GPIO_MODE_OUT_PP_50M, 0u);
    gpio_config_pin(GPIOC_BASE, 7u, GPIO_MODE_OUT_PP_50M, 0u);

    LCD_CS_HIGH();
    LCD_DC_DATA();
    LCD_RST_HIGH();

    /*
     * SPI2 Master, 8-bit, MSB first, SWCSEN=1, SWCSIL=1, MDIV=/4 (18 MHz @ 72 MHz APB1)
     * Mode 3 (CLKPOL=1, CLKPHA=1) verified against DWS-200 stock firmware (0x0801330A..14).
     */
    SPI_I2SCTRL(SPI2_BASE) = 0u;
    SPI_CTRL2(SPI2_BASE) = 0u;
    SPI_CTRL1(SPI2_BASE) = (1u << 9) | (1u << 8) | (1u << 3) | (1u << 2) | (1u << 1) | (1u << 0);
    SPI_CTRL1(SPI2_BASE) |= (1u << 6); /* SPIEN */

    /* Hardware reset */
    LCD_RST_LOW();
    board_delay_ms(10u);
    LCD_RST_HIGH();
    board_delay_ms(120u);

    /* ST7789V init sequence verified from APP_DWS_200_V1.5 (0x080072DC & 0x080143A0) */
    lcd_cmd(0x11u); /* SLPOUT */
    board_delay_ms(120u);

    lcd_cmd(0x36u); /* MADCTL */
    lcd_data8(0xA0u); /* 320x240 landscape, offset 0,0 */

    lcd_cmd(0x3Au); /* COLMOD */
    lcd_data8(0x05u); /* 16-bit RGB565 */

    static const uint8_t b2[] = { 0x0Cu, 0x0Cu, 0x00u, 0x33u, 0x33u };
    static const uint8_t d0[] = { 0xA4u, 0xA1u };
    static const uint8_t e0[] = {
        0xD0u, 0x01u, 0x08u, 0x0Fu, 0x11u, 0x2Au, 0x36u,
        0x55u, 0x44u, 0x3Au, 0x0Bu, 0x06u, 0x11u, 0x20u
    };
    static const uint8_t e1[] = {
        0xD0u, 0x02u, 0x07u, 0x0Au, 0x0Bu, 0x18u, 0x34u,
        0x43u, 0x4Au, 0x2Bu, 0x1Bu, 0x1Cu, 0x22u, 0x1Fu
    };

    lcd_cmd_data(0xB2u, b2, sizeof(b2));
    lcd_cmd(0xB7u); lcd_data8(0x35u);
    lcd_cmd(0xBBu); lcd_data8(0x28u);
    lcd_cmd(0xC0u); lcd_data8(0x2Cu);
    lcd_cmd(0xC2u); lcd_data8(0x01u);
    lcd_cmd(0xC3u); lcd_data8(0x0Bu);
    lcd_cmd(0xC4u); lcd_data8(0x20u);
    lcd_cmd(0xC6u); lcd_data8(0x0Fu);
    lcd_cmd_data(0xD0u, d0, sizeof(d0));
    lcd_cmd_data(0xE0u, e0, sizeof(e0));
    lcd_cmd_data(0xE1u, e1, sizeof(e1));

    lcd_cmd(0x29u); /* DISPON */
    display_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, 0x0000u);
}

void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565) {
    if (w == 0u || h == 0u || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    if ((uint32_t)x + w > LCD_WIDTH)  w = (uint16_t)(LCD_WIDTH - x);
    if ((uint32_t)y + h > LCD_HEIGHT) h = (uint16_t)(LCD_HEIGHT - y);

    lcd_set_window(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u));

    uint8_t hi = (uint8_t)(rgb565 >> 8);
    uint8_t lo = (uint8_t)(rgb565 & 0xFFu);
    uint32_t count = (uint32_t)w * (uint32_t)h;

    LCD_CS_LOW();
    LCD_DC_DATA();
    while (count--) {
        spi2_write_byte(hi);
        spi2_write_byte(lo);
    }
    spi2_wait_idle();
    LCD_CS_HIGH();
}

void display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    if (pixels == NULL || w == 0u || h == 0u || x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    if ((uint32_t)x + w > LCD_WIDTH)  w = (uint16_t)(LCD_WIDTH - x);
    if ((uint32_t)y + h > LCD_HEIGHT) h = (uint16_t)(LCD_HEIGHT - y);

    lcd_set_window(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u));

    uint32_t count = (uint32_t)w * (uint32_t)h;
    LCD_CS_LOW();
    LCD_DC_DATA();
    for (uint32_t i = 0u; i < count; ++i) {
        uint16_t px = pixels[i];
        spi2_write_byte((uint8_t)(px >> 8));
        spi2_write_byte((uint8_t)(px & 0xFFu));
    }
    spi2_wait_idle();
    LCD_CS_HIGH();
}

#else

void display_init(void) {
    host_display_init();
}

void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565) {
    host_display_fill_rect(x, y, w, h, rgb565);
}

void display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    host_display_blit_rgb565(x, y, w, h, pixels);
}

#endif /* OPENDWS_HOST_TEST */
