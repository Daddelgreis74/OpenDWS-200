#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t irq_cnt;
    uint16_t rst_cnt;
    uint16_t setup_cnt;
    uint8_t  gccfg_hi; /* (OTG_GCCFG >> 16) & 0xFF; expected 0x2D */
} usb_cdc_debug_t;

void usb_cdc_init(void);
void usb_cdc_poll(void);
uint8_t usb_cdc_is_connected(void);
uint8_t usb_cdc_dtr(void);
void usb_cdc_get_debug(usb_cdc_debug_t *out);
size_t usb_cdc_write(const uint8_t *data, size_t len);
size_t usb_cdc_read_line(char *buf, size_t max_len);
