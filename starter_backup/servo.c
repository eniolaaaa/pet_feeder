/*! ***************************************************************************
 *
 * \brief     Hobby servo driver (50 Hz PWM on P1_4 / CT1_MAT2)
 * \file      servo.c
 *
 * \see       NXP. (2024). MCX A153, A152, A143, A142 Reference Manual. Rev. 4,
 *            01/2024. From:
 *            https://www.nxp.com/docs/en/reference-manual/MCXAP64M96FS3RM.pdf
 *
 *****************************************************************************/
#include "servo.h"

// -----------------------------------------------------------------------------
// Local function implementation
// -----------------------------------------------------------------------------
void servo_init(void)
{
    // Clock source for CTIMER1
    // MUX: [101] = CLK_1M, so one timer tick is 1 us
    MRCC0->MRCC_CTIMER1_CLKSEL = MRCC_MRCC_CTIMER1_CLKSEL_MUX(0b101);
    MRCC0->MRCC_CTIMER1_CLKDIV = 0;

    // CTIMER1_CLK_EN: [1] = CTIMER 1 function clock enabled
    SYSCON->CTIMERGLOBALSTARTEN |= SYSCON_CTIMERGLOBALSTARTEN_CTIMER1_CLK_EN(1);

    // Enable modules and leave others unchanged
    MRCC0->MRCC_GLB_CC0_SET = MRCC_MRCC_GLB_CC0_CTIMER1(1);
    MRCC0->MRCC_GLB_CC0_SET = MRCC_MRCC_GLB_CC0_PORT1(1);

    // Release modules from reset and leave others unchanged
    MRCC0->MRCC_GLB_RST0_SET = MRCC_MRCC_GLB_RST0_CTIMER1(1);
    MRCC0->MRCC_GLB_RST0_SET = MRCC_MRCC_GLB_RST0_PORT1(1);

    // Configure P1_4
    // LK : [1] = Locks this PCR
    // MUX: [0100] = Alternative 4 - CT1_MAT2
    PORT1->PCR[4] = PORT_PCR_LK(1) | PORT_PCR_MUX(4);

    // Hold the counter in reset while configuring
    CTIMER1->TCR = CTIMER_TCR_CRST(1);

    // No prescaler: 1 MHz timer clock
    CTIMER1->PR = 0;

    // MR3 sets the period: reset TC when it matches MR3
    CTIMER1->MR[3] = SERVO_PERIOD_US - 1;
    CTIMER1->MCR = CTIMER_MCR_MR3R(1);

    // PWM mode on match output 2. The output is low at the start of each
    // period and goes high when TC reaches MR2, so the high time is
    // (period - MR2).
    CTIMER1->PWMC = CTIMER_PWMC_PWMEN2(1);
    servo_set_pulse_us(SERVO_MIN_PULSE_US);

    // Release reset and start counting
    CTIMER1->TCR = CTIMER_TCR_CEN(1);
}

void servo_set_pulse_us(uint32_t pulse_us)
{
    if(pulse_us < SERVO_MIN_PULSE_US)
    {
        pulse_us = SERVO_MIN_PULSE_US;
    }
    else if(pulse_us > SERVO_MAX_PULSE_US)
    {
        pulse_us = SERVO_MAX_PULSE_US;
    }

    CTIMER1->MR[2] = SERVO_PERIOD_US - pulse_us;
}

void servo_set_angle(uint32_t degrees)
{
    if(degrees > 180)
    {
        degrees = 180;
    }

    servo_set_pulse_us(SERVO_MIN_PULSE_US +
        ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * degrees) / 180);
}
