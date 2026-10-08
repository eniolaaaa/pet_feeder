/*
 * bsp.h - thin board-support interface.
 *
 * The application (app_tasks.c) only talks to the hardware through these
 * functions, so the same RTOS code runs on the FRDM-MCXA153
 * (board/frdmmcxa153/bsp_mcxa153.c) and in the PC simulator (sim/bsp_posix.c).
 */
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { BSP_LED_RED = 0, BSP_LED_GREEN, BSP_LED_BLUE } bsp_led_t;

void bsp_init(void);                    /* clocks, pins, console, servo PWM     */
void bsp_enable_irqs(void);             /* ISR #1 feed button, ISR #2 UART RX   */
void bsp_servo_set_angle(uint32_t deg); /* 0..180                               */
bool bsp_button_is_pressed(void);
void bsp_led_set(bsp_led_t led, bool on);
void bsp_led_toggle(bsp_led_t led);
void bsp_console_write(const char *s);  /* raw, NOT thread safe                 */

/* Implemented by the application, called from the BSP interrupt handlers. */
void app_button_isr(void);
void app_uart_rx_isr(uint8_t c);

#endif /* BSP_H */
