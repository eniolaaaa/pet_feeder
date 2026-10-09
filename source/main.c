/*
 * main.c - entry point. Hardware init, create tasks, start the scheduler.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp.h"
#include "app_tasks.h"

int main(void)
{
    bsp_init();
    bsp_console_write("\r\nFreeRTOS flexible project - FRDM-MCXA153 pet feeder (type help)\r\n");

    app_create();
    vTaskStartScheduler();

    /* Only reached if there was not enough heap for the idle/timer task. */
    for (;;) {}
}
