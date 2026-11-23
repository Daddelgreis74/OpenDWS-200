#include <stdint.h>

#include "app_config.h"
#include "hw.h"

extern uint32_t __data_load;
extern uint32_t __data_start;
extern uint32_t __data_end;
extern uint32_t __bss_start;
extern uint32_t __bss_end;
extern uint32_t __stack_top;

void Reset_Handler(void);
void Reset_Handler_C(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void EXINT9_5_IRQHandler(void);
void OTGFS1_IRQHandler(void);
int main(void);

/*
 * AT32F415RCT7 vector table (16 core exceptions + 76 peripheral IRQs = 92 entries, 0x170 bytes).
 * Reserved IRQ slots match the AT32F415 silicon mapping.
 */
__attribute__((section(".isr_vector"), used))
void (*const vector_table[92])(void) = {
    [0] = (void (*)(void))&__stack_top,
    [1] = Reset_Handler,
    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [4] = HardFault_Handler, /* MemManage */
    [5] = HardFault_Handler, /* BusFault */
    [6] = HardFault_Handler, /* UsageFault */
    [11] = Default_Handler,  /* SVCall */
    [12] = Default_Handler,  /* DebugMon */
    [14] = Default_Handler,  /* PendSV */
    [15] = SysTick_Handler,

    [16 ... 38] = Default_Handler,
    [39] = EXINT9_5_IRQHandler, /* IRQ 23: PB5 Sleep stand edge detect */
    [40 ... 58] = Default_Handler,
    /* Reserved AT32F415 IRQ slots: 43..48 (vec 59..64) */
    [59 ... 64] = 0,
    [65 ... 66] = Default_Handler,
    /* Reserved IRQ 51 (vec 67) */
    [67] = 0,
    [68 ... 69] = Default_Handler,
    /* Reserved IRQs 54..55 (vec 70..71) */
    [70 ... 71] = 0,
    [72 ... 75] = Default_Handler,
    /* Reserved IRQs 60..66 (vec 76..82) */
    [76 ... 82] = 0,
    [83] = OTGFS1_IRQHandler, /* IRQ 67: USB OTG FS */
    /* Reserved IRQs 68..69 (vec 84..85) */
    [84 ... 85] = 0,
    [86 ... 87] = Default_Handler,
    /* Reserved IRQs 72..74 (vec 88..90) */
    [88 ... 90] = 0,
    [91] = Default_Handler,
};

/*
 * First instructions executed after bootloader jump or reset:
 * Set MSP, disable interrupts, force PA2 (heater) LOW immediately before touching RAM.
 * NOTE: AT32F415 has NO FPU - never touch SCB->CPACR (0xE000ED88).
 */
__attribute__((naked, section(".text.Reset_Handler")))
void Reset_Handler(void) {
    __asm__ volatile(
        "ldr r0, =__stack_top\n"
        "msr msp, r0\n"
        "cpsid i\n"
        /* Enable GPIOA (bit 2) and GPIOB (bit 3) clocks in CRM_APB2EN (0x40021018) */
        "ldr r0, =0x40021018\n"
        "ldr r1, [r0]\n"
        "movs r2, #0x0c\n"
        "orrs r1, r2\n"
        "str r1, [r0]\n"
        "ldr r1, [r0]\n"
        /* Force PA2 = Output Push-Pull 50 MHz and drive LOW (GPIOA_CLR = 0x40010814) */
        "ldr r0, =0x40010800\n"
        "ldr r1, [r0]\n"
        "bic r1, r1, #0x0f00\n"
        "orr r1, r1, #0x0300\n"
        "str r1, [r0]\n"
        "ldr r0, =0x40010814\n"
        "movs r1, #0x04\n"
        "str r1, [r0]\n"
        /* Configure PB1 = Output Push-Pull 50 MHz and drive HIGH (GPIOB_SCR = 0x40010C10) */
        "ldr r0, =0x40010c00\n"
        "ldr r1, [r0]\n"
        "bic r1, r1, #0x00f0\n"
        "orr r1, r1, #0x0030\n"
        "str r1, [r0]\n"
        "ldr r0, =0x40010c10\n"
        "movs r1, #0x02\n"
        "str r1, [r0]\n"
        "b Reset_Handler_C\n"
    );
}

static void fail_safe_halt(void) {
    /* Force PA2 heater output LOW before spinning */
    GPIO_CLR(GPIOA_BASE) = (1u << 2);
    for (;;) {
        GPIO_CLR(GPIOA_BASE) = (1u << 2);
        __asm__ volatile("nop");
    }
}

void NMI_Handler(void) {
    fail_safe_halt();
}

void HardFault_Handler(void) {
    fail_safe_halt();
}

void Default_Handler(void) {
    fail_safe_halt();
}

void Reset_Handler_C(void) {
    /* Ensure heater stays locked LOW */
    GPIO_CLR(GPIOA_BASE) = (1u << 2);

    /* Relocate vector table to APP_BASE_ADDR and clear any bootloader interrupts */
    SCB_VTOR = APP_BASE_ADDR;
    SYST_CSR = 0u;
    NVIC_ICER0 = 0xFFFFFFFFu;
    NVIC_ICER1 = 0xFFFFFFFFu;
    NVIC_ICER2 = 0xFFFFFFFFu;
    NVIC_ICPR0 = 0xFFFFFFFFu;
    NVIC_ICPR1 = 0xFFFFFFFFu;
    NVIC_ICPR2 = 0xFFFFFFFFu;
    __asm__ volatile("dsb\nisb");

    /* If bootloader left USB OTGFS1 connected, perform clean soft-disconnect */
    if ((CRM_AHBEN & CRM_AHB_OTGFS1) != 0u) {
        OTG_DCTL |= (1u << 1); /* SDISCON */
        for (volatile uint32_t i = 0u; i < 60000u; ++i) {
            __asm__ volatile("nop");
        }
        OTG_GINTMSK = 0u;
        OTG_GINTSTS = 0xFFFFFFFFu;
    }

    /* Copy .data from Flash to RAM */
    uint32_t *src = &__data_load;
    uint32_t *dst = &__data_start;
    while (dst < &__data_end) {
        *dst++ = *src++;
    }

    /* Zero .bss */
    dst = &__bss_start;
    while (dst < &__bss_end) {
        *dst++ = 0u;
    }

    __asm__ volatile("cpsie i");
    (void)main();
    fail_safe_halt();
}
