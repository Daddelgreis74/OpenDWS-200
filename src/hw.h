#pragma once

#include <stdint.h>

#ifndef OPENDWS_HOST_TEST

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr)  (*(volatile uint8_t *)(uintptr_t)(addr))

/* Base peripheral map for Artery AT32F415RCT7 */
#define TMR2_BASE       0x40000000u
#define TMR3_BASE       0x40000400u
#define WDT_BASE        0x40003000u
#define SPI2_BASE       0x40003800u
#define PWC_BASE        0x40007000u

#define IOMUX_BASE      0x40010000u
#define EXINT_BASE      0x40010400u
#define GPIOA_BASE      0x40010800u
#define GPIOB_BASE      0x40010C00u
#define GPIOC_BASE      0x40011000u
#define GPIOD_BASE      0x40011400u
#define GPIOF_BASE      0x40011C00u
#define ADC1_BASE       0x40012400u
#define TMR1_BASE       0x40012C00u
#define USART1_BASE     0x40013800u

#define DMA1_BASE       0x40020000u
#define DMA2_BASE       0x40020400u
#define CRM_BASE        0x40021000u
#define FLASH_REG_BASE  0x40022000u

#define OTGFS1_BASE     0x50000000u

/* Cortex-M4 Core Registers (no FPU on AT32F415) */
#define SYST_CSR        REG32(0xE000E010u)
#define SYST_RVR        REG32(0xE000E014u)
#define SYST_CVR        REG32(0xE000E018u)

#define NVIC_ISER0      REG32(0xE000E100u)
#define NVIC_ISER1      REG32(0xE000E104u)
#define NVIC_ISER2      REG32(0xE000E108u)
#define NVIC_ICER0      REG32(0xE000E180u)
#define NVIC_ICER1      REG32(0xE000E184u)
#define NVIC_ICER2      REG32(0xE000E188u)
#define NVIC_ICPR0      REG32(0xE000E280u)
#define NVIC_ICPR1      REG32(0xE000E284u)
#define NVIC_ICPR2      REG32(0xE000E288u)

#define SCB_VTOR        REG32(0xE000ED08u)
#define SCB_AIRCR       REG32(0xE000ED0Cu)

/* FLASH Controller */
#define FLASH_PSR       REG32(FLASH_REG_BASE + 0x00u)

/* CRM (Clock and Reset Management) */
#define CRM_CTRL        REG32(CRM_BASE + 0x00u)
#define CRM_CFG         REG32(CRM_BASE + 0x04u)
#define CRM_CLKINT      REG32(CRM_BASE + 0x08u)
#define CRM_APB2RST     REG32(CRM_BASE + 0x0Cu)
#define CRM_APB1RST     REG32(CRM_BASE + 0x10u)
#define CRM_AHBEN       REG32(CRM_BASE + 0x14u)
#define CRM_APB2EN      REG32(CRM_BASE + 0x18u)
#define CRM_APB1EN      REG32(CRM_BASE + 0x1Cu)
#define CRM_PLL         REG32(CRM_BASE + 0x2Cu)
#define CRM_MISC1       REG32(CRM_BASE + 0x30u)
#define CRM_MISC2       REG32(CRM_BASE + 0x54u)

/* CRM_CTRL bits */
#define CRM_CTRL_HICKEN     (1u << 0)
#define CRM_CTRL_HICKSTBL   (1u << 1)
#define CRM_CTRL_HEXTEN     (1u << 16)
#define CRM_CTRL_HEXTSTBL   (1u << 17)
#define CRM_CTRL_PLLEN      (1u << 24)
#define CRM_CTRL_PLLSTBL    (1u << 25)

/* CRM_AHBEN bits */
#define CRM_AHB_DMA1        (1u << 0)
#define CRM_AHB_DMA2        (1u << 1)
#define CRM_AHB_OTGFS1      (1u << 12)

/* CRM_APB2EN bits */
#define CRM_APB2_IOMUX      (1u << 0)
#define CRM_APB2_GPIOA      (1u << 2)
#define CRM_APB2_GPIOB      (1u << 3)
#define CRM_APB2_GPIOC      (1u << 4)
#define CRM_APB2_GPIOD      (1u << 5)
#define CRM_APB2_GPIOF      (1u << 7)
#define CRM_APB2_ADC1       (1u << 9)
#define CRM_APB2_TMR1       (1u << 11)
#define CRM_APB2_USART1     (1u << 14)

/* CRM_APB1EN bits */
#define CRM_APB1_TMR2       (1u << 0)
#define CRM_APB1_TMR3       (1u << 1)
#define CRM_APB1_SPI2       (1u << 14)
#define CRM_APB1_PWC        (1u << 28)

/* IOMUX */
#define IOMUX_REMAP         REG32(IOMUX_BASE + 0x04u)
#define IOMUX_EXINTC1       REG32(IOMUX_BASE + 0x08u)
#define IOMUX_EXINTC2       REG32(IOMUX_BASE + 0x0Cu)
#define IOMUX_EXINTC3       REG32(IOMUX_BASE + 0x10u)
#define IOMUX_EXINTC4       REG32(IOMUX_BASE + 0x14u)
#define IOMUX_REMAP4        REG32(IOMUX_BASE + 0x24u)

/* EXINT */
#define EXINT_INTEN         REG32(EXINT_BASE + 0x00u)
#define EXINT_EVTEN         REG32(EXINT_BASE + 0x04u)
#define EXINT_POLCFG1       REG32(EXINT_BASE + 0x08u)
#define EXINT_POLCFG2       REG32(EXINT_BASE + 0x0Cu)
#define EXINT_SWTRG         REG32(EXINT_BASE + 0x10u)
#define EXINT_INTSTS        REG32(EXINT_BASE + 0x14u)

/* GPIO */
#define GPIO_CFGL(base)     REG32((base) + 0x00u)
#define GPIO_CFGH(base)     REG32((base) + 0x04u)
#define GPIO_IDT(base)      REG32((base) + 0x08u)
#define GPIO_ODT(base)      REG32((base) + 0x0Cu)
#define GPIO_SCR(base)      REG32((base) + 0x10u)
#define GPIO_CLR(base)      REG32((base) + 0x14u)

/* GPIO mode nibbles: MODE[1:0] | (CNF[1:0] << 2) */
#define GPIO_MODE_ANALOG    0x0u
#define GPIO_MODE_IN_FLOAT  0x4u
#define GPIO_MODE_IN_PULL   0x8u
#define GPIO_MODE_OUT_PP_2M 0x2u
#define GPIO_MODE_OUT_PP_50M 0x3u
#define GPIO_MODE_AF_PP_50M 0xBu

/* TMR */
#define TMR_CTRL1(base)     REG32((base) + 0x00u)
#define TMR_CTRL2(base)     REG32((base) + 0x04u)
#define TMR_IDEN(base)      REG32((base) + 0x0Cu)
#define TMR_ISTS(base)      REG32((base) + 0x10u)
#define TMR_SWEVT(base)     REG32((base) + 0x14u)
#define TMR_CM1(base)       REG32((base) + 0x18u)
#define TMR_CM2(base)       REG32((base) + 0x1Cu)
#define TMR_CCTRL(base)     REG32((base) + 0x20u)
#define TMR_CVAL(base)      REG32((base) + 0x24u)
#define TMR_DIV(base)       REG32((base) + 0x28u)
#define TMR_PR(base)        REG32((base) + 0x2Cu)
#define TMR_C1DT(base)      REG32((base) + 0x34u)
#define TMR_C2DT(base)      REG32((base) + 0x38u)
#define TMR_C3DT(base)      REG32((base) + 0x3Cu)
#define TMR_C4DT(base)      REG32((base) + 0x40u)

/* SPI */
#define SPI_CTRL1(base)     REG32((base) + 0x00u)
#define SPI_CTRL2(base)     REG32((base) + 0x04u)
#define SPI_STS(base)       REG32((base) + 0x08u)
#define SPI_DT(base)        REG32((base) + 0x0Cu)
#define SPI_I2SCTRL(base)   REG32((base) + 0x1Cu)

#define SPI_STS_RDBF        (1u << 0)
#define SPI_STS_TDBE        (1u << 1)
#define SPI_STS_BF          (1u << 7)

/* ADC1 */
#define ADC_STS(base)       REG32((base) + 0x00u)
#define ADC_CTRL1(base)     REG32((base) + 0x04u)
#define ADC_CTRL2(base)     REG32((base) + 0x08u)
#define ADC_SPT1(base)      REG32((base) + 0x0Cu)
#define ADC_SPT2(base)      REG32((base) + 0x10u)
#define ADC_OSQ1(base)      REG32((base) + 0x2Cu)
#define ADC_OSQ2(base)      REG32((base) + 0x30u)
#define ADC_OSQ3(base)      REG32((base) + 0x34u)
#define ADC_ODT(base)       REG32((base) + 0x4Cu)

#define ADC_STS_OCCE        (1u << 1)
#define ADC_CTRL2_ADCEN     (1u << 0)
#define ADC_CTRL2_ADCAL     (1u << 2)
#define ADC_CTRL2_ADCALINIT (1u << 3)
#define ADC_CTRL2_OCTEN     (1u << 20)
#define ADC_CTRL2_OCSWTRG   (1u << 22)

/* Watchdog (WDT) */
#define WDT_CMD             REG32(WDT_BASE + 0x00u)
#define WDT_DIV             REG32(WDT_BASE + 0x04u)
#define WDT_RLD             REG32(WDT_BASE + 0x08u)
#define WDT_STS             REG32(WDT_BASE + 0x0Cu)

#define WDT_CMD_UNLOCK      0x5555u
#define WDT_CMD_RELOAD      0xAAAAu
#define WDT_CMD_ENABLE      0xCCCCu

/* OTGFS1 (Synopsys DWC2 FS Device Core @ 0x50000000) */
#define OTG_GOTGCTL         REG32(OTGFS1_BASE + 0x000u)
#define OTG_GOTGINT         REG32(OTGFS1_BASE + 0x004u)
#define OTG_GAHBCFG         REG32(OTGFS1_BASE + 0x008u)
#define OTG_GUSBCFG         REG32(OTGFS1_BASE + 0x00Cu)
#define OTG_GRSTCTL         REG32(OTGFS1_BASE + 0x010u)
#define OTG_GINTSTS         REG32(OTGFS1_BASE + 0x014u)
#define OTG_GINTMSK         REG32(OTGFS1_BASE + 0x018u)
#define OTG_GRXSTSR         REG32(OTGFS1_BASE + 0x01Cu)
#define OTG_GRXSTSP         REG32(OTGFS1_BASE + 0x020u)
#define OTG_GRXFSIZ         REG32(OTGFS1_BASE + 0x024u)
#define OTG_DIEPTXF0        REG32(OTGFS1_BASE + 0x028u)
#define OTG_GCCFG           REG32(OTGFS1_BASE + 0x038u)
#define OTG_DIEPTXF(n)      REG32(OTGFS1_BASE + 0x104u + (((uint32_t)(n) - 1u) * 4u))

#define OTG_DCFG            REG32(OTGFS1_BASE + 0x800u)
#define OTG_DCTL            REG32(OTGFS1_BASE + 0x804u)
#define OTG_DSTS            REG32(OTGFS1_BASE + 0x808u)
#define OTG_DIEPMSK         REG32(OTGFS1_BASE + 0x810u)
#define OTG_DOEPMSK         REG32(OTGFS1_BASE + 0x814u)
#define OTG_DAINT           REG32(OTGFS1_BASE + 0x818u)
#define OTG_DAINTMSK        REG32(OTGFS1_BASE + 0x81Cu)

#define OTG_DIEPCTL(ep)     REG32(OTGFS1_BASE + 0x900u + ((uint32_t)(ep) * 0x20u))
#define OTG_DIEPINT(ep)     REG32(OTGFS1_BASE + 0x908u + ((uint32_t)(ep) * 0x20u))
#define OTG_DIEPTSIZ(ep)    REG32(OTGFS1_BASE + 0x910u + ((uint32_t)(ep) * 0x20u))
#define OTG_DTXFSTS(ep)     REG32(OTGFS1_BASE + 0x918u + ((uint32_t)(ep) * 0x20u))

#define OTG_DOEPCTL(ep)     REG32(OTGFS1_BASE + 0xB00u + ((uint32_t)(ep) * 0x20u))
#define OTG_DOEPINT(ep)     REG32(OTGFS1_BASE + 0xB08u + ((uint32_t)(ep) * 0x20u))
#define OTG_DOEPTSIZ(ep)    REG32(OTGFS1_BASE + 0xB10u + ((uint32_t)(ep) * 0x20u))

#define OTG_PCGCCTL         REG32(OTGFS1_BASE + 0xE00u)
#define OTG_FIFO(ep)        (*(volatile uint32_t *)(uintptr_t)(OTGFS1_BASE + 0x1000u + ((uint32_t)(ep) * 0x1000u)))

/* IRQ Numbers (AT32F415) */
#define EXINT9_5_IRQn       23u
#define OTGFS1_IRQn         67u

static inline void irq_enable(uint32_t irqn) {
    if (irqn < 32u) {
        NVIC_ISER0 = (1u << irqn);
    } else if (irqn < 64u) {
        NVIC_ISER1 = (1u << (irqn - 32u));
    } else {
        NVIC_ISER2 = (1u << (irqn - 64u));
    }
}

static inline void gpio_config_pin(uint32_t gpio_base, uint32_t pin, uint32_t mode4, uint8_t odr_pull) {
    if (pin < 8u) {
        uint32_t shift = pin * 4u;
        GPIO_CFGL(gpio_base) = (GPIO_CFGL(gpio_base) & ~(0xFu << shift)) | ((mode4 & 0xFu) << shift);
    } else {
        uint32_t shift = (pin - 8u) * 4u;
        GPIO_CFGH(gpio_base) = (GPIO_CFGH(gpio_base) & ~(0xFu << shift)) | ((mode4 & 0xFu) << shift);
    }
    if (mode4 == GPIO_MODE_IN_PULL) {
        if (odr_pull) {
            GPIO_SCR(gpio_base) = (1u << pin);
        } else {
            GPIO_CLR(gpio_base) = (1u << pin);
        }
    }
}

#endif /* OPENDWS_HOST_TEST */
