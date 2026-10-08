/*! ***************************************************************************
 *
 * \brief     Hobby servo driver (50 Hz PWM on P1_4 / CT1_MAT2)
 * \file      servo.h
 *
 * The servo signal is on P1_4, which is header pin J1-2 on the FRDM-MCXA153.
 * CTIMER1 runs from the 1 MHz clock, so one timer tick is 1 us.
 *
 *****************************************************************************/
#ifndef SERVO_H
#define SERVO_H

#include <MCXA153.h>
#include <stdint.h>

/// PWM period in microseconds (50 Hz)
#define SERVO_PERIOD_US    (20000U)

/// Pulse width for 0 degrees in microseconds
#define SERVO_MIN_PULSE_US (1000U)

/// Pulse width for 180 degrees in microseconds
#define SERVO_MAX_PULSE_US (2000U)

void servo_init(void);
void servo_set_pulse_us(uint32_t pulse_us);
void servo_set_angle(uint32_t degrees);

#endif // SERVO_H
