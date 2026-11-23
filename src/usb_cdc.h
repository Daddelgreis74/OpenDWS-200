#pragma once

#include <stddef.h>
#include <stdint.h>

void usb_cdc_init(void);
void usb_cdc_poll(void);
uint8_t usb_cdc_is_connected(void);
uint8_t usb_cdc_dtr(void);
size_t usb_cdc_write(const uint8_t *data, size_t len);
size_t usb_cdc_read_line(char *buf, size_t max_len);
