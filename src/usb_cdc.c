#include "usb_cdc.h"

#include "board.h"
#include "hw.h"
#include "runtime.h"

#ifndef OPENDWS_HOST_TEST

#define USB_VID 0x2E3Cu
#define USB_PID 0x5740u

#define EP0_MPS 64u
#define EP1_MPS 64u
#define EP2_MPS 16u

#define RX_RING_SIZE 256u
#define TX_RING_SIZE 1024u

/* OTG_GINTSTS / OTG_GINTMSK bits */
#define GINT_USBSUSP   (1u << 11)
#define GINT_USBRST    (1u << 12)
#define GINT_ENUMDONE  (1u << 13)
#define GINT_RXFLVL    (1u << 4)
#define GINT_IEPINT    (1u << 18)
#define GINT_OEPINT    (1u << 19)
#define GINT_WKUP      (1u << 31)

/* OTG_DIEPCTL / DOEPCTL bits */
#define DEPCTL_EPENA   (1u << 31)
#define DEPCTL_EPDIS   (1u << 30)
#define DEPCTL_SETD0   (1u << 28)
#define DEPCTL_SNAK    (1u << 27)
#define DEPCTL_CNAK    (1u << 26)
#define DEPCTL_STALL   (1u << 21)
#define DEPCTL_USBAEP  (1u << 15)

/* OTG_DIEPINT / DOEPINT bits */
#define DEPINT_XFRC    (1u << 0)
#define DEPINT_SETUP   (1u << 3)

typedef enum {
    EP0_IDLE = 0,
    EP0_DATA_IN,
    EP0_DATA_OUT,
    EP0_STATUS_IN,
    EP0_STATUS_OUT
} ep0_state_t;

static volatile uint8_t g_configured = 0u;
static volatile uint8_t g_dtr = 0u;
static volatile uint8_t g_ep1_tx_busy = 0u;
static volatile uint8_t g_ep1_need_zlp = 0u;
static volatile uint16_t g_dbg_irq_cnt = 0u;
static volatile uint16_t g_dbg_rst_cnt = 0u;
static volatile uint16_t g_dbg_setup_cnt = 0u;

static ep0_state_t g_ep0_state = EP0_IDLE;
static uint8_t g_setup_pkt[8];
static const uint8_t *g_ep0_tx_ptr = NULL;
static uint16_t g_ep0_tx_rem = 0u;
static uint8_t g_ep0_tx_zlp = 0u;
static uint8_t g_ep0_scratch[8];
static uint8_t g_ep0_out_buf[8];
static uint16_t g_ep0_out_len = 0u;
static uint8_t g_ep0_out_req = 0u;

static uint8_t g_line_coding[7] = {
    0x00u, 0xC2u, 0x01u, 0x00u, /* 115200 bps LE */
    0u,                         /* 1 stop bit */
    0u,                         /* None parity */
    8u                          /* 8 data bits */
};

static volatile uint8_t g_rx_ring[RX_RING_SIZE];
static volatile uint16_t g_rx_head = 0u;
static volatile uint16_t g_rx_tail = 0u;

static volatile uint8_t g_tx_ring[TX_RING_SIZE];
static volatile uint16_t g_tx_head = 0u;
static volatile uint16_t g_tx_tail = 0u;

static const uint8_t g_dev_desc[18] = {
    18u, 0x01u,
    0x00u, 0x02u,       /* USB 2.00 */
    0x02u, 0x00u, 0x00u,/* CDC */
    EP0_MPS,
    (uint8_t)(USB_VID & 0xFFu), (uint8_t)(USB_VID >> 8),
    (uint8_t)(USB_PID & 0xFFu), (uint8_t)(USB_PID >> 8),
    0x00u, 0x01u,       /* bcdDevice 1.00 */
    1u, 2u, 3u,         /* String indices */
    1u                  /* bNumConfigurations */
};

static const uint8_t g_cfg_desc[67] = {
    /* Configuration Descriptor */
    9u, 0x02u, 67u, 0u, 2u, 1u, 0u, 0xC0u, 50u,

    /* Interface 0: CDC Communication */
    9u, 0x04u, 0u, 0u, 1u, 0x02u, 0x02u, 0x01u, 0u,
    /* Header Functional Descriptor */
    5u, 0x24u, 0x00u, 0x10u, 0x01u,
    /* Call Management Functional Descriptor */
    5u, 0x24u, 0x01u, 0x00u, 0x01u,
    /* ACM Functional Descriptor */
    4u, 0x24u, 0x02u, 0x02u,
    /* Union Functional Descriptor */
    5u, 0x24u, 0x06u, 0x00u, 0x01u,
    /* Endpoint 2 IN (Interrupt) */
    7u, 0x05u, 0x82u, 0x03u, EP2_MPS, 0u, 16u,

    /* Interface 1: CDC Data */
    9u, 0x04u, 1u, 0u, 2u, 0x0Au, 0x00u, 0x00u, 0u,
    /* Endpoint 1 OUT (Bulk) */
    7u, 0x05u, 0x01u, 0x02u, EP1_MPS, 0u, 0u,
    /* Endpoint 1 IN (Bulk) */
    7u, 0x05u, 0x81u, 0x02u, EP1_MPS, 0u, 0u
};

static const uint8_t g_str_lang[4] = { 4u, 0x03u, 0x09u, 0x04u };
static const uint8_t g_str_mfg[16] = {
    16u, 0x03u, 'O',0,'p',0,'e',0,'n',0,'D',0,'W',0,'S',0
};
static const uint8_t g_str_prod[50] = {
    50u, 0x03u,
    'O',0,'p',0,'e',0,'n',0,'D',0,'W',0,'S',0,'-',0,'2',0,'0',0,'0',0,' ',0,
    'P',0,'h',0,'a',0,'s',0,'e',0,'-',0,'2',0,' ',0,'C',0,'D',0,'C',0
};
static const uint8_t g_str_ser[20] = {
    20u, 0x03u, 'D',0,'W',0,'S',0,'2',0,'0',0,'0',0,'-',0,'P',0,'1',0
};

static void fifo_write(uint8_t ep, const uint8_t *buf, uint16_t len) {
    uint32_t words = ((uint32_t)len + 3u) >> 2;
    for (uint32_t w = 0u; w < words; ++w) {
        uint32_t v = 0u;
        uint32_t base = w * 4u;
        for (uint32_t b = 0u; b < 4u && (base + b) < len; ++b) {
            v |= ((uint32_t)buf[base + b]) << (b * 8u);
        }
        OTG_FIFO(ep) = v;
    }
}

static void fifo_read(uint8_t *dst, uint16_t len, uint16_t max_dst) {
    uint32_t words = ((uint32_t)len + 3u) >> 2;
    for (uint32_t w = 0u; w < words; ++w) {
        uint32_t v = OTG_FIFO(0);
        uint32_t base = w * 4u;
        for (uint32_t b = 0u; b < 4u && (base + b) < len; ++b) {
            uint8_t byte = (uint8_t)(v >> (b * 8u));
            if (dst != NULL && (base + b) < max_dst) {
                dst[base + b] = byte;
            }
        }
    }
}

static void rx_ring_push_fifo(uint16_t len) {
    uint32_t words = ((uint32_t)len + 3u) >> 2;
    for (uint32_t w = 0u; w < words; ++w) {
        uint32_t v = OTG_FIFO(0);
        uint32_t base = w * 4u;
        for (uint32_t b = 0u; b < 4u && (base + b) < len; ++b) {
            uint16_t next = (uint16_t)((g_rx_head + 1u) % RX_RING_SIZE);
            if (next != g_rx_tail) {
                g_rx_ring[g_rx_head] = (uint8_t)(v >> (b * 8u));
                g_rx_head = next;
            }
        }
    }
}

static void ep0_arm_setup(void) {
    OTG_DOEPTSIZ(0) = (3u << 29) | (1u << 19) | 24u;
    OTG_DOEPCTL(0) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP;
}

static void ep0_start_in(const uint8_t *data, uint16_t len, uint16_t req_len) {
    if (len > req_len) len = req_len;
    g_ep0_tx_ptr = data;
    g_ep0_tx_rem = len;
    g_ep0_tx_zlp = (len < req_len && (len % EP0_MPS) == 0u && len > 0u) ? 1u : 0u;
    g_ep0_state = (len == 0u) ? EP0_STATUS_IN : EP0_DATA_IN;

    uint16_t chunk = (g_ep0_tx_rem > EP0_MPS) ? EP0_MPS : g_ep0_tx_rem;
    OTG_DIEPTSIZ(0) = (1u << 19) | chunk;
    OTG_DIEPCTL(0) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP;
    if (chunk > 0u) {
        fifo_write(0u, g_ep0_tx_ptr, chunk);
        g_ep0_tx_ptr += chunk;
        g_ep0_tx_rem = (uint16_t)(g_ep0_tx_rem - chunk);
    }
}

static void ep0_stall(void) {
    OTG_DIEPCTL(0) |= DEPCTL_STALL;
    g_ep0_state = EP0_IDLE;
    ep0_arm_setup();
}

static void ep1_arm_out(void) {
    OTG_DOEPTSIZ(1) = (1u << 19) | EP1_MPS;
    OTG_DOEPCTL(1) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP | (2u << 18) | EP1_MPS;
}

static void activate_cdc_endpoints(void) {
    /* EP1 OUT Bulk */
    OTG_DOEPCTL(1) = DEPCTL_USBAEP | (2u << 18) | DEPCTL_SETD0 | EP1_MPS;
    /* EP1 IN Bulk (TX FIFO 1) */
    OTG_DIEPCTL(1) = DEPCTL_USBAEP | (2u << 18) | (1u << 22) | DEPCTL_SETD0 | DEPCTL_SNAK | EP1_MPS;
    /* EP2 IN Interrupt (TX FIFO 2) */
    OTG_DIEPCTL(2) = DEPCTL_USBAEP | (3u << 18) | (2u << 22) | DEPCTL_SETD0 | DEPCTL_SNAK | EP2_MPS;

    OTG_DAINTMSK |= (1u << 0) | (1u << 1) | (1u << 16) | (1u << 17);
    g_ep1_tx_busy = 0u;
    g_ep1_need_zlp = 0u;
    ep1_arm_out();
}

static void usb_flush_tx_fifo(uint32_t num) {
    OTG_GRSTCTL = (num << 6) | (1u << 5);
    for (uint32_t t = 0u; t < 10000u && (OTG_GRSTCTL & (1u << 5)) != 0u; ++t) {
    }
}

static void usb_flush_rx_fifo(void) {
    OTG_GRSTCTL = (1u << 4);
    for (uint32_t t = 0u; t < 10000u && (OTG_GRSTCTL & (1u << 4)) != 0u; ++t) {
    }
}

static void handle_setup_packet(void) {
    g_dbg_setup_cnt++;
    uint8_t bmRequest = g_setup_pkt[0];
    uint8_t bRequest  = g_setup_pkt[1];
    uint16_t wValue   = (uint16_t)g_setup_pkt[2] | ((uint16_t)g_setup_pkt[3] << 8);
    uint16_t wIndex   = (uint16_t)g_setup_pkt[4] | ((uint16_t)g_setup_pkt[5] << 8);
    uint16_t wLength  = (uint16_t)g_setup_pkt[6] | ((uint16_t)g_setup_pkt[7] << 8);
    (void)wIndex;

    uint8_t req_type = bmRequest & 0x60u;

    /* Standard Device / Interface / Endpoint Requests */
    if (req_type == 0x00u) {
        switch (bRequest) {
        case 0x06u: { /* GET_DESCRIPTOR */
            uint8_t desc_type = (uint8_t)(wValue >> 8);
            uint8_t desc_idx  = (uint8_t)(wValue & 0xFFu);
            if (desc_type == 0x01u) {
                ep0_start_in(g_dev_desc, sizeof(g_dev_desc), wLength);
                return;
            } else if (desc_type == 0x02u) {
                ep0_start_in(g_cfg_desc, sizeof(g_cfg_desc), wLength);
                return;
            } else if (desc_type == 0x03u) {
                if (desc_idx == 0u) { ep0_start_in(g_str_lang, sizeof(g_str_lang), wLength); return; }
                if (desc_idx == 1u) { ep0_start_in(g_str_mfg,  sizeof(g_str_mfg),  wLength); return; }
                if (desc_idx == 2u) { ep0_start_in(g_str_prod, sizeof(g_str_prod), wLength); return; }
                if (desc_idx == 3u) { ep0_start_in(g_str_ser,  sizeof(g_str_ser),  wLength); return; }
            }
            ep0_stall();
            return;
        }
        case 0x05u: /* SET_ADDRESS */
            OTG_DCFG = (OTG_DCFG & ~(0x7Fu << 4)) | (((uint32_t)(wValue & 0x7Fu)) << 4);
            ep0_start_in(NULL, 0u, 0u);
            return;
        case 0x09u: /* SET_CONFIGURATION */
            g_configured = ((wValue & 0xFFu) != 0u) ? 1u : 0u;
            if (g_configured) {
                activate_cdc_endpoints();
            }
            ep0_start_in(NULL, 0u, 0u);
            return;
        case 0x08u: /* GET_CONFIGURATION */
            g_ep0_scratch[0] = g_configured;
            ep0_start_in(g_ep0_scratch, 1u, wLength);
            return;
        case 0x00u: /* GET_STATUS */
            g_ep0_scratch[0] = ((bmRequest & 0x1Fu) == 0u) ? 0x01u : 0x00u; /* Self-powered */
            g_ep0_scratch[1] = 0x00u;
            ep0_start_in(g_ep0_scratch, 2u, wLength);
            return;
        case 0x0Au: /* GET_INTERFACE */
            g_ep0_scratch[0] = 0u;
            ep0_start_in(g_ep0_scratch, 1u, wLength);
            return;
        case 0x0Bu: /* SET_INTERFACE */
            ep0_start_in(NULL, 0u, 0u);
            return;
        default:
            ep0_stall();
            return;
        }
    }

    /* CDC Class-Specific Requests */
    if (req_type == 0x20u) {
        if (bRequest == 0x20u && wLength == 7u) { /* SET_LINE_CODING */
            g_ep0_state = EP0_DATA_OUT;
            g_ep0_out_len = 0u;
            g_ep0_out_req = 0x20u;
            OTG_DOEPTSIZ(0) = (1u << 19) | 7u;
            OTG_DOEPCTL(0) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP;
            return;
        }
        if (bRequest == 0x21u) { /* GET_LINE_CODING */
            ep0_start_in(g_line_coding, sizeof(g_line_coding), wLength);
            return;
        }
        if (bRequest == 0x22u) { /* SET_CONTROL_LINE_STATE */
            g_dtr = (wValue & 0x01u) ? 1u : 0u;
            ep0_start_in(NULL, 0u, 0u);
            return;
        }
    }

    ep0_stall();
}

static void ep1_start_tx_locked(void) {
    if (!g_configured || g_ep1_tx_busy) return;

    if (g_tx_head == g_tx_tail) {
        if (g_ep1_need_zlp) {
            g_ep1_need_zlp = 0u;
            g_ep1_tx_busy = 1u;
            OTG_DIEPTSIZ(1) = (1u << 19) | 0u;
            OTG_DIEPCTL(1) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP | (2u << 18) | (1u << 22) | EP1_MPS;
        }
        return;
    }

    uint8_t pkt[EP1_MPS];
    uint16_t count = 0u;
    while (count < EP1_MPS && g_tx_head != g_tx_tail) {
        pkt[count++] = g_tx_ring[g_tx_tail];
        g_tx_tail = (uint16_t)((g_tx_tail + 1u) % TX_RING_SIZE);
    }

    g_ep1_tx_busy = 1u;
    g_ep1_need_zlp = (count == EP1_MPS && g_tx_head == g_tx_tail) ? 1u : 0u;

    OTG_DIEPTSIZ(1) = (1u << 19) | count;
    OTG_DIEPCTL(1) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP | (2u << 18) | (1u << 22) | EP1_MPS;
    fifo_write(1u, pkt, count);
}

void OTGFS1_IRQHandler(void) {
    g_dbg_irq_cnt++;
    uint32_t gint = OTG_GINTSTS & OTG_GINTMSK;

    if (gint & GINT_USBRST) {
        OTG_GINTSTS = GINT_USBRST;
        g_dbg_rst_cnt++;
        g_configured = 0u;
        g_dtr = 0u;
        g_ep1_tx_busy = 0u;
        g_ep1_need_zlp = 0u;
        g_ep0_state = EP0_IDLE;

        OTG_DCTL &= ~(1u << 0); /* Clear RWKUPSIG */
        OTG_DCFG &= ~(0x7Fu << 4);
        usb_flush_tx_fifo(0x10u);
        usb_flush_rx_fifo();

        for (uint32_t ep = 0u; ep < 4u; ++ep) {
            OTG_DIEPINT(ep) = 0xFFu;
            OTG_DOEPINT(ep) = 0xFFu;
        }
        OTG_DIEPCTL(0) = DEPCTL_USBAEP | DEPCTL_SNAK;
        OTG_DOEPCTL(0) = DEPCTL_USBAEP | DEPCTL_SNAK;
        OTG_DAINTMSK = (1u << 0) | (1u << 16);
        OTG_DOEPMSK = DEPINT_XFRC | DEPINT_SETUP;
        OTG_DIEPMSK = DEPINT_XFRC;
        ep0_arm_setup();
    }

    if (gint & GINT_ENUMDONE) {
        OTG_GINTSTS = GINT_ENUMDONE;
        OTG_DIEPCTL(0) &= ~0x7FFu; /* EP0 MPS = 64 bytes */
        OTG_DCTL &= ~(1u << 8);    /* Clear CGINAK */
        OTG_GUSBCFG = (OTG_GUSBCFG & ~(0xFu << 10)) | (5u << 10); /* TRDT = 5 for 144 MHz AHB */
        ep0_arm_setup();
    }

    if (gint & GINT_RXFLVL) {
        while ((OTG_GINTSTS & GINT_RXFLVL) != 0u) {
            uint32_t st = OTG_GRXSTSP;
            uint8_t ep = (uint8_t)(st & 0xFu);
            uint16_t bcnt = (uint16_t)((st >> 4) & 0x7FFu);
            uint8_t pktsts = (uint8_t)((st >> 17) & 0xFu);

            if (ep == 0u && pktsts == 6u) {
                fifo_read(g_setup_pkt, bcnt, sizeof(g_setup_pkt));
            } else if (ep == 0u && pktsts == 2u) {
                fifo_read(g_ep0_out_buf, bcnt, sizeof(g_ep0_out_buf));
                g_ep0_out_len = bcnt;
            } else if (ep == 1u && pktsts == 2u) {
                rx_ring_push_fifo(bcnt);
            } else if (bcnt > 0u) {
                fifo_read(NULL, bcnt, 0u);
            }
        }
    }

    if (gint & GINT_IEPINT) {
        uint32_t daint = OTG_DAINT & OTG_DAINTMSK;
        if (daint & (1u << 0)) {
            uint32_t epint = OTG_DIEPINT(0);
            OTG_DIEPINT(0) = epint;
            if (epint & DEPINT_XFRC) {
                if (g_ep0_state == EP0_DATA_IN) {
                    if (g_ep0_tx_rem > 0u || g_ep0_tx_zlp) {
                        uint16_t chunk = (g_ep0_tx_rem > EP0_MPS) ? EP0_MPS : g_ep0_tx_rem;
                        if (chunk == 0u) g_ep0_tx_zlp = 0u;
                        OTG_DIEPTSIZ(0) = (1u << 19) | chunk;
                        OTG_DIEPCTL(0) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP;
                        if (chunk > 0u) {
                            fifo_write(0u, g_ep0_tx_ptr, chunk);
                            g_ep0_tx_ptr += chunk;
                            g_ep0_tx_rem = (uint16_t)(g_ep0_tx_rem - chunk);
                        }
                    } else {
                        g_ep0_state = EP0_STATUS_OUT;
                        OTG_DOEPTSIZ(0) = (3u << 29) | (1u << 19) | 64u;
                        OTG_DOEPCTL(0) = DEPCTL_EPENA | DEPCTL_CNAK | DEPCTL_USBAEP;
                    }
                } else if (g_ep0_state == EP0_STATUS_IN) {
                    g_ep0_state = EP0_IDLE;
                    ep0_arm_setup();
                }
            }
        }
        if (daint & (1u << 1)) {
            uint32_t epint = OTG_DIEPINT(1);
            OTG_DIEPINT(1) = epint;
            if (epint & DEPINT_XFRC) {
                g_ep1_tx_busy = 0u;
                ep1_start_tx_locked();
            }
        }
    }

    if (gint & GINT_OEPINT) {
        uint32_t daint = OTG_DAINT & OTG_DAINTMSK;
        if (daint & (1u << 16)) {
            uint32_t epint = OTG_DOEPINT(0);
            OTG_DOEPINT(0) = epint;
            if (epint & DEPINT_SETUP) {
                handle_setup_packet();
            } else if (epint & DEPINT_XFRC) {
                if (g_ep0_state == EP0_DATA_OUT) {
                    if (g_ep0_out_req == 0x20u && g_ep0_out_len == 7u) {
                        memcpy(g_line_coding, g_ep0_out_buf, 7u);
                    }
                    g_ep0_out_req = 0u;
                    ep0_start_in(NULL, 0u, 0u);
                } else if (g_ep0_state == EP0_STATUS_OUT) {
                    g_ep0_state = EP0_IDLE;
                    ep0_arm_setup();
                }
            }
        }
        if (daint & (1u << 17)) {
            uint32_t epint = OTG_DOEPINT(1);
            OTG_DOEPINT(1) = epint;
            if (epint & DEPINT_XFRC) {
                ep1_arm_out();
            }
        }
    }

    if (gint & GINT_USBSUSP) {
        OTG_GINTSTS = GINT_USBSUSP;
        g_configured = 0u;
        g_dtr = 0u;
        g_ep1_tx_busy = 0u;
    }

    if (gint & GINT_WKUP) {
        OTG_DCTL &= ~(1u << 0);
        OTG_PCGCCTL = 0u;
        OTG_GINTSTS = GINT_WKUP;
    }
}

void usb_cdc_init(void) {
    g_configured = 0u;
    g_dtr = 0u;
    g_ep1_tx_busy = 0u;
    g_ep1_need_zlp = 0u;
    g_ep0_state = EP0_IDLE;
    g_rx_head = g_rx_tail = 0u;
    g_tx_head = g_tx_tail = 0u;
    g_dbg_irq_cnt = 0u;
    g_dbg_rst_cnt = 0u;
    g_dbg_setup_cnt = 0u;

    /* Leave PA11 (DM) and PA12 (DP) in reset default floating input for OTG PHY */
    gpio_config_pin(GPIOA_BASE, 11u, GPIO_MODE_IN_FLOAT, 0u);
    gpio_config_pin(GPIOA_BASE, 12u, GPIO_MODE_IN_FLOAT, 0u);

    CRM_AHBEN |= CRM_AHB_OTGFS1;

    /* Ignore VBUS sensing before disconnecting / initializing core (matches IPS3608) */
    OTG_GCCFG |= (1u << 21);

    /* Soft disconnect to force fresh host re-enumeration after bootloader */
    OTG_DCTL |= (1u << 1);
    board_delay_ms(20u);

    /* Mask global interrupts during core init */
    OTG_GAHBCFG = 0u;
    OTG_GINTMSK = 0u;

    /* Wait for AHB idle, then perform Core soft reset */
    for (uint32_t t = 0u; t < 100000u && (OTG_GRSTCTL & (1u << 31)) == 0u; ++t) {
    }
    OTG_GRSTCTL |= (1u << 0);
    for (uint32_t t = 0u; t < 100000u && (OTG_GRSTCTL & (1u << 0)) != 0u; ++t) {
    }
    board_delay_ms(5u);

    /* Power on embedded FS PHY + A/B valid sense + ignore PA9 VBUS BEFORE FDEVMODE */
    OTG_GCCFG = (1u << 16) | (1u << 18) | (1u << 19) | (1u << 21); /* 0x002D0000 */
    board_delay_ms(5u);

    /* Force Device Mode + TRDT = 5 AFTER PHY is powered on */
    OTG_GUSBCFG = (OTG_GUSBCFG & ~((1u << 29) | (0xFu << 10))) | (1u << 30) | (5u << 10);
    board_delay_ms(25u);

    /* Restart PHY clock and set Full-Speed internal PHY + clear PFIVL */
    OTG_PCGCCTL = 0u;
    OTG_DCFG = (OTG_DCFG & ~((3u << 11) | 0x3u)) | 0x3u;

    usb_flush_tx_fifo(0x10u);
    usb_flush_rx_fifo();

    OTG_DAINTMSK = 0u;
    OTG_DIEPMSK = 0u;
    OTG_DOEPMSK = 0u;

    /* Zero TX FIFO registers, then configure AT32F415 FIFO layout (matches IPS3608) */
    for (uint32_t n = 1u; n <= 4u; ++n) {
        OTG_DIEPTXF(n) = 0u;
    }
    OTG_GRXFSIZ = 0x80u;
    OTG_DIEPTXF0 = (0x18u << 16) | 0x80u;
    OTG_DIEPTXF(1) = (0x14u << 16) | 0x98u;
    OTG_DIEPTXF(2) = (0x14u << 16) | 0xACu;
    OTG_DIEPTXF(3) = (0x14u << 16) | 0xC0u;

    /* Explicitly reset all 4 IN and 4 OUT endpoints before enabling interrupts */
    for (uint32_t ep = 0u; ep < 4u; ++ep) {
        if ((OTG_DIEPCTL(ep) & DEPCTL_EPENA) != 0u) {
            OTG_DIEPCTL(ep) = DEPCTL_EPDIS | DEPCTL_SNAK;
        } else {
            OTG_DIEPCTL(ep) = 0u;
        }
        OTG_DIEPTSIZ(ep) = 0u;
        OTG_DIEPINT(ep) = 0xFFu;

        if ((OTG_DOEPCTL(ep) & DEPCTL_EPENA) != 0u) {
            OTG_DOEPCTL(ep) = DEPCTL_EPDIS | DEPCTL_SNAK;
        } else {
            OTG_DOEPCTL(ep) = 0u;
        }
        OTG_DOEPTSIZ(ep) = 0u;
        OTG_DOEPINT(ep) = 0xFFu;
    }

    OTG_GINTSTS = 0xFFFFFFFFu;
    OTG_GINTMSK = GINT_USBRST | GINT_ENUMDONE | GINT_RXFLVL |
                  GINT_IEPINT | GINT_OEPINT | GINT_USBSUSP | GINT_WKUP;

    /* Soft connect (clear SDISCON) and enable global OTG interrupt */
    OTG_DCTL &= ~(1u << 1);
    OTG_GAHBCFG = (1u << 0);
    irq_set_priority(OTGFS1_IRQn, 0x80u);
    irq_enable(OTGFS1_IRQn);
}

void usb_cdc_poll(void) {
    if (!g_configured || g_ep1_tx_busy) return;
    irq_disable(OTGFS1_IRQn);
    ep1_start_tx_locked();
    irq_enable(OTGFS1_IRQn);
}

uint8_t usb_cdc_is_connected(void) {
    return g_configured;
}

uint8_t usb_cdc_dtr(void) {
    return g_dtr;
}

void usb_cdc_get_debug(usb_cdc_debug_t *out) {
    if (out == NULL) return;
    out->irq_cnt = g_dbg_irq_cnt;
    out->rst_cnt = g_dbg_rst_cnt;
    out->setup_cnt = g_dbg_setup_cnt;
    out->gccfg_hi = (uint8_t)((OTG_GCCFG >> 16) & 0xFFu);
}

size_t usb_cdc_write(const uint8_t *data, size_t len) {
    if (!g_configured || data == NULL || len == 0u) {
        return 0u;
    }
    size_t written = 0u;
    irq_disable(OTGFS1_IRQn);
    for (size_t i = 0u; i < len; ++i) {
        uint16_t next = (uint16_t)((g_tx_head + 1u) % TX_RING_SIZE);
        if (next == g_tx_tail) {
            break;
        }
        g_tx_ring[g_tx_head] = data[i];
        g_tx_head = next;
        written++;
    }
    ep1_start_tx_locked();
    irq_enable(OTGFS1_IRQn);
    return written;
}

size_t usb_cdc_read_line(char *buf, size_t max_len) {
    if (buf == NULL || max_len < 2u) return 0u;

    /* Discard leading CR/LF so CRLF pairs do not stall subsequent reads */
    while (g_rx_tail != g_rx_head) {
        uint8_t c = g_rx_ring[g_rx_tail];
        if (c != '\r' && c != '\n') {
            break;
        }
        g_rx_tail = (uint16_t)((g_rx_tail + 1u) % RX_RING_SIZE);
    }

    /* Scan ring for a CR or LF terminator before consuming */
    uint16_t scan = g_rx_tail;
    uint16_t count = 0u;
    uint8_t found = 0u;
    while (scan != g_rx_head) {
        uint8_t c = g_rx_ring[scan];
        scan = (uint16_t)((scan + 1u) % RX_RING_SIZE);
        count++;
        if (c == '\r' || c == '\n') {
            found = 1u;
            break;
        }
        if (count + 1u >= max_len) {
            found = 1u;
            break;
        }
    }
    if (!found) {
        return 0u;
    }

    size_t out = 0u;
    while (g_rx_tail != g_rx_head && out + 1u < max_len) {
        uint8_t c = g_rx_ring[g_rx_tail];
        g_rx_tail = (uint16_t)((g_rx_tail + 1u) % RX_RING_SIZE);
        if (c == '\r' || c == '\n') {
            break;
        }
        buf[out++] = (char)c;
    }
    buf[out] = '\0';
    return out;
}

#else

void usb_cdc_init(void) {}
void usb_cdc_poll(void) {}
uint8_t usb_cdc_is_connected(void) { return 0u; }
uint8_t usb_cdc_dtr(void) { return 0u; }
void usb_cdc_get_debug(usb_cdc_debug_t *out) {
    if (out != NULL) {
        out->irq_cnt = 0u;
        out->rst_cnt = 0u;
        out->setup_cnt = 0u;
        out->gccfg_hi = 0u;
    }
}
size_t usb_cdc_write(const uint8_t *data, size_t len) { (void)data; (void)len; return 0u; }
size_t usb_cdc_read_line(char *buf, size_t max_len) { (void)buf; (void)max_len; return 0u; }

#endif /* OPENDWS_HOST_TEST */
