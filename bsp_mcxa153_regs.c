/*
 * bsp_mcxa153_regs.c - board layer for the FRDM-MCXA153, REGISTER LEVEL.
 *
 * Use this file instead of board/frdmmcxa153/bsp_mcxa153.c when the project is
 * based on the MIC5 course example (no NXP SDK drivers). It only needs the
 * device header (MCXA153.h via fsl_device_registers.h / CMSIS).
 * Put exactly ONE of the two bsp_*.c files in the project.
 *
 * Interrupt handler #1: GPIO1_IRQHandler   SW3 (P1_7), falling edge -> app_button_isr()   NVIC prio 4
 * Interrupt handler #2: LPUART0_IRQHandler console RX (MCU-Link VCOM) -> app_uart_rx_isr() NVIC prio 3
 * Servo PWM:  CTIMER1 MAT2 on P1_4 (header J1-2), 50 Hz, 1 us resolution.
 * LEDs (active low): red P3_12, green P3_13, blue P3_0.
 * Console: LPUART0 on P0_2 (RX) / P0_3 (TX), 115200 8N1.
 *
 * Register names were checked against NXP's MCXA153 device headers
 * (mcux-devices-mcx: MCXA153_COMMON.h, periph/PERI_*.h), and the clock/reset
 * sequences copy what the SDK's fsl_clock.c / fsl_reset.c do.
 *
 * Course project options (define them in the project settings if needed):
 *   BSP_INIT_LPUART0=0  if the course code already initialises LPUART0
 *                       (pins, clock, baud rate). Keep the RX interrupt part.
 *   BSP_CTIMER_CLK_HZ   CTIMER1 input clock (FRO_HF). Default: SystemCoreClock,
 *                       which is FRO_HF when the AHB divider is 1.
 */
#include "fsl_device_registers.h"

#include "app_config.h"
#include "bsp.h"

#ifndef BSP_INIT_LPUART0
#define BSP_INIT_LPUART0    1
#endif
#ifndef BSP_CTIMER_CLK_HZ
#define BSP_CTIMER_CLK_HZ   SystemCoreClock
#endif

/* MRCC clock-gate / reset bit numbers (fsl_clock.h / fsl_reset.h of the SDK) */
#define CC0_CTIMER1   (1u << 3)
#define CC0_LPUART0   (1u << 19)
#define CC0_PORT0     (1u << 29)
#define CC0_PORT1     (1u << 30)
#define CC1_PORT3     (1u << 0)
#define CC1_GPIO1     (1u << 6)
#define CC1_GPIO3     (1u << 8)

#define BTN_PIN       7u           /* P1_7 = SW3 */
#define BTN_PRIO      4u
#define RX_PRIO       3u

#define LED_RED_PIN   12u          /* P3_12 */
#define LED_GREEN_PIN 13u          /* P3_13 */
#define LED_BLUE_PIN  0u           /* P3_0  */
static const uint32_t s_ledPin[] = { LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN };

#define SERVO_US_PER_TICK  1u      /* CTIMER1 prescaled to 1 MHz */

/* ---------- MRCC helpers (same sequence as the SDK drivers) ---------- */
static void clk_unlock(void) { SYSCON->CLKUNLOCK &= ~SYSCON_CLKUNLOCK_UNLOCK_MASK; }
/* No clk_lock(): vConfigureTimerForRunTimeStats() (course runtime_stats.c)
 * writes the CTIMER2 MRCC registers later, from vTaskStartScheduler(). With
 * SYSCON CLKUNLOCK[UNLOCK] set those writes are ignored, CTIMER2 stays
 * gated and its first register access is a precise BusFault (BFAR = CTIMER2). */

static void clocks_and_resets(void)
{
    clk_unlock();
    MRCC0->MRCC_GLB_CC0_SET = CC0_CTIMER1 | CC0_PORT1 | CC0_PORT0 |
                              (BSP_INIT_LPUART0 ? CC0_LPUART0 : 0u);
    MRCC0->MRCC_GLB_CC1_SET = CC1_PORT3 | CC1_GPIO1 | CC1_GPIO3;

    /* Release from reset (set = out of reset) and wait until done. */
    uint32_t rst0 = CC0_CTIMER1 | CC0_PORT1 | CC0_PORT0 | (BSP_INIT_LPUART0 ? CC0_LPUART0 : 0u);
    uint32_t rst1 = CC1_PORT3 | CC1_GPIO1 | CC1_GPIO3;
    MRCC0->MRCC_GLB_RST0_SET = rst0;
    MRCC0->MRCC_GLB_RST1_SET = rst1;
    while ((MRCC0->MRCC_GLB_RST0 & rst0) != rst0) {}
    while ((MRCC0->MRCC_GLB_RST1 & rst1) != rst1) {}

    /* CTIMER1 function clock enable (RM 26.1.5 step 2, as in the course
     * runtime_stats.c). Without it CTIMER1 does not count. */
    SYSCON->CTIMERGLOBALSTARTEN |= SYSCON_CTIMERGLOBALSTARTEN_CTIMER1_CLK_EN(1);

    /* CTIMER1 functional clock = FRO_HF (select 1), divider 1. */
    MRCC0->MRCC_CTIMER1_CLKSEL = 1u;
    MRCC0->MRCC_CTIMER1_CLKDIV = MRCC_MRCC_CTIMER1_CLKDIV_HALT_MASK | MRCC_MRCC_CTIMER1_CLKDIV_RESET_MASK;
    MRCC0->MRCC_CTIMER1_CLKDIV = MRCC_MRCC_CTIMER1_CLKDIV_HALT_MASK | 0u;   /* DIV = 0 -> /1 */
    MRCC0->MRCC_CTIMER1_CLKDIV &= ~MRCC_MRCC_CTIMER1_CLKDIV_HALT_MASK;

#if BSP_INIT_LPUART0
    /* LPUART0 functional clock = FRO12M (select 0), divider 1. */
    MRCC0->MRCC_LPUART0_CLKSEL = 0u;
    MRCC0->MRCC_LPUART0_CLKDIV = MRCC_MRCC_LPUART0_CLKDIV_HALT_MASK | MRCC_MRCC_LPUART0_CLKDIV_RESET_MASK;
    MRCC0->MRCC_LPUART0_CLKDIV = MRCC_MRCC_LPUART0_CLKDIV_HALT_MASK | 0u;
    MRCC0->MRCC_LPUART0_CLKDIV &= ~MRCC_MRCC_LPUART0_CLKDIV_HALT_MASK;
#endif
}

static void pins_init(void)
{
    /* SW3: GPIO (mux 0), pull-up, passive filter, input buffer enabled. */
    PORT1->PCR[BTN_PIN] = PORT_PCR_MUX(0) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK |
                          PORT_PCR_PFE_MASK | PORT_PCR_IBE_MASK;
    GPIO1->PDDR &= ~(1u << BTN_PIN);

    /* P1_4 = CT1_MAT2 (mux 4), servo signal. */
    PORT1->PCR[4] = PORT_PCR_MUX(4) | PORT_PCR_IBE_MASK;

    /* LEDs: GPIO outputs, off = high (active low). */
    for (unsigned i = 0; i < 3u; i++) {
        PORT3->PCR[s_ledPin[i]] = PORT_PCR_MUX(0) | PORT_PCR_IBE_MASK;
        GPIO3->PSOR = 1u << s_ledPin[i];
        GPIO3->PDDR |= 1u << s_ledPin[i];
    }

#if BSP_INIT_LPUART0
    /* P0_2 = LPUART0_RXD, P0_3 = LPUART0_TXD (mux 2), pull-up. */
    PORT0->PCR[2] = PORT_PCR_MUX(2) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_IBE_MASK;
    PORT0->PCR[3] = PORT_PCR_MUX(2) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_IBE_MASK;
#endif
}

#if BSP_INIT_LPUART0
static void uart_init(void)
{
    /* 12 MHz / (OSR 26 * SBR 4) = 115384 baud (+0.16 %). OSR field = OSR - 1. */
    LPUART0->CTRL = 0;
    LPUART0->BAUD = LPUART_BAUD_OSR(26u - 1u) | LPUART_BAUD_SBR(4u);
    LPUART0->CTRL = LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK;
}
#endif

static void servo_init(void)
{
    /* CTIMER1: prescale to 1 MHz, MR3 = period (reset on match),
     * MR2 = PWM output on MAT2. In PWM mode the output is LOW until the
     * match and HIGH after it, so MR2 = period - high time. */
    CTIMER1->TCR  = CTIMER_TCR_CRST_MASK;                 /* hold in reset */
    CTIMER1->CTCR = 0;                                     /* timer mode    */
    CTIMER1->PR   = (BSP_CTIMER_CLK_HZ / 1000000u) - 1u;   /* 1 us per tick */
    CTIMER1->MR[3] = SERVO_PERIOD_US / SERVO_US_PER_TICK - 1u;
    CTIMER1->MR[2] = CTIMER1->MR[3];                       /* output low = no pulse yet */
    CTIMER1->MCR  = CTIMER_MCR_MR3R_MASK;                  /* reset TC on MR3 */
    CTIMER1->PWMC = CTIMER_PWMC_PWMEN2_MASK;               /* MAT2 in PWM mode */
    CTIMER1->TCR  = CTIMER_TCR_CEN_MASK;                   /* run           */
}

void bsp_init(void)
{
    /* SystemCoreClockUpdate() is empty in the course SDK and SystemCoreClock
     * defaults to 12 MHz, but the core and FRO_HF run at 48 MHz. The servo
     * prescaler below depends on this value. */
    SystemCoreClock = 48000000u;
    clocks_and_resets();
    pins_init();
#if BSP_INIT_LPUART0
    uart_init();
#endif
    servo_init();
}

void bsp_servo_set_angle(uint32_t deg)
{
    if (deg > 180u) deg = 180u;
    uint32_t high_us = SERVO_MIN_PULSE_US +
                       (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * deg / 180u;
    CTIMER1->MR[2] = (CTIMER1->MR[3] + 1u) - high_us / SERVO_US_PER_TICK;
}

void bsp_enable_irqs(void)
{
    /* ISR #1: SW3 falling edge (IRQC = 0xA), clear old flag first. */
    GPIO1->ISFR[0] = 1u << BTN_PIN;
    GPIO1->ICR[BTN_PIN] = GPIO_ICR_IRQC(0xAu);
    NVIC_SetPriority(GPIO1_IRQn, BTN_PRIO);
    NVIC_ClearPendingIRQ(GPIO1_IRQn);
    NVIC_EnableIRQ(GPIO1_IRQn);

    /* ISR #2: LPUART0 receive data register full. */
    LPUART0->CTRL |= LPUART_CTRL_RIE_MASK;
    NVIC_SetPriority(LPUART0_IRQn, RX_PRIO);
    NVIC_EnableIRQ(LPUART0_IRQn);
}

void GPIO1_IRQHandler(void)
{
    uint32_t flags = GPIO1->ISFR[0];
    GPIO1->ISFR[0] = flags;                 /* write 1 to clear */
    if (flags & (1u << BTN_PIN)) {
        app_button_isr();
    }
    __DSB();
}

void LPUART0_IRQHandler(void)
{
    uint32_t stat = LPUART0->STAT;
    if (stat & LPUART_STAT_OR_MASK) {
        /* Clear only the overrun flag: STAT mixes config bits and
         * write-1-to-clear flags (same as the SDK's LPUART_ClearStatusFlags). */
        stat &= ~(LPUART_STAT_LBKDIF_MASK | LPUART_STAT_RXEDGIF_MASK | LPUART_STAT_IDLE_MASK |
                  LPUART_STAT_OR_MASK | LPUART_STAT_NF_MASK | LPUART_STAT_FE_MASK |
                  LPUART_STAT_PF_MASK | LPUART_STAT_MA1F_MASK | LPUART_STAT_MA2F_MASK);
        LPUART0->STAT = stat | LPUART_STAT_OR_MASK;
    }
    while (LPUART0->STAT & LPUART_STAT_RDRF_MASK) {
        app_uart_rx_isr((uint8_t)LPUART0->DATA);
    }
    __DSB();
}

bool bsp_button_is_pressed(void)
{
    return (GPIO1->PDIR & (1u << BTN_PIN)) == 0u;   /* active low */
}

void bsp_led_set(bsp_led_t l, bool on)
{
    if (on) GPIO3->PCOR = 1u << s_ledPin[l];        /* low = on */
    else    GPIO3->PSOR = 1u << s_ledPin[l];
}

void bsp_led_toggle(bsp_led_t l)
{
    GPIO3->PTOR = 1u << s_ledPin[l];
}

void bsp_console_write(const char *s)
{
    while (*s) {
        while (!(LPUART0->STAT & LPUART_STAT_TDRE_MASK)) {}
        LPUART0->DATA = (uint8_t)*s++;
    }
}
