/*! ***************************************************************************
 *
 * \brief     Pet feeder starter project (FreeRTOS on FRDM-MCXA153)
 * \file      main.c
 *
 * Three tasks to prove the toolchain, the board and the servo work:
 *  - Led task:    blinks the green LED as a heartbeat
 *  - Button task: polls SW3 and sends a feed request to a queue
 *  - Feeder task: waits on the queue (or a timeout) and opens/closes the
 *                 servo to drop a portion of food
 *
 * Based on the MIC5 week 2 example 01 (HAN University of Applied Sciences).
 *
 *****************************************************************************/
#include <MCXA153.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include "rgb.h"
#include "serial.h"
#include "servo.h"
#include "sw.h"

// -----------------------------------------------------------------------------
// Local type definitions
// -----------------------------------------------------------------------------
typedef enum
{
    FEED_SOURCE_BUTTON = 0,
    FEED_SOURCE_SCHEDULE,
} feed_source_t;

// -----------------------------------------------------------------------------
// Local defines
// -----------------------------------------------------------------------------
#define LED_PERIOD_MS          (500)
#define BUTTON_POLL_MS         (20)
#define FEED_INTERVAL_MS       (30000)  // automatic feed every 30 s for testing
#define SERVO_CLOSED_DEG       (0)
#define SERVO_OPEN_DEG         (90)
#define SERVO_OPEN_TIME_MS     (1000)

// -----------------------------------------------------------------------------
// Local function prototypes
// -----------------------------------------------------------------------------
static void vLedTask(void *pvParameters);
static void vButtonTask(void *pvParameters);
static void vFeederTask(void *pvParameters);

// -----------------------------------------------------------------------------
// Local variables
// -----------------------------------------------------------------------------
static QueueHandle_t xFeedQueue = NULL;

// -----------------------------------------------------------------------------
// Main application
// -----------------------------------------------------------------------------
int main(void)
{
    // SystemCoreClock is not correctly set in the startup files. Do it here,
    // because FreeRTOS is configured to use this variable.
    SystemCoreClock = 48000000;

    rgb_init();
    sw_init();
    servo_init();
    servo_set_angle(SERVO_CLOSED_DEG);
    xSerialPortInit(921600, 128);

    vSerialPutString("\r\nPet feeder starter - FreeRTOS on FRDM-MCXA153\r\n\r\n");

    xFeedQueue = xQueueCreate(4, sizeof(feed_source_t));
    vQueueAddToRegistry(xFeedQueue, "FeedQ");

    xTaskCreate(vLedTask,    "Led",    configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vButtonTask, "Button", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vFeederTask, "Feeder", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

    vTaskStartScheduler();

    // Only reached when there was not enough heap for the idle task
    for( ;; );
}

// -----------------------------------------------------------------------------
// Local function implementation
// -----------------------------------------------------------------------------

void vApplicationMallocFailedHook( void )
{
    // Visualize an erroneous situation by using a red LED
    rgb_red_on(true);
}

/*----------------------------------------------------------------------------*/

static void vLedTask(void *pvParameters)
{
    (void)pvParameters;
    bool on = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        on = !on;
        rgb_green_on(on);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LED_PERIOD_MS));
    }
}

/*----------------------------------------------------------------------------*/

static void vButtonTask(void *pvParameters)
{
    (void)pvParameters;
    bool wasPressed = false;

    for( ;; )
    {
        bool pressed = sw_pressed(SW3);

        // Send one request per press (falling edge)
        if(pressed && !wasPressed)
        {
            feed_source_t src = FEED_SOURCE_BUTTON;
            xQueueSend(xFeedQueue, &src, 0);
        }
        wasPressed = pressed;

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

/*----------------------------------------------------------------------------*/

static void vFeederTask(void *pvParameters)
{
    (void)pvParameters;
    feed_source_t src;
    uint32_t count = 0;
    char str[64];

    for( ;; )
    {
        // Wait for a button request; a timeout means the schedule is due
        if(xQueueReceive(xFeedQueue, &src, pdMS_TO_TICKS(FEED_INTERVAL_MS)) != pdPASS)
        {
            src = FEED_SOURCE_SCHEDULE;
        }

        count++;
        snprintf(str, sizeof(str), "Feed #%lu (%s)\r\n", (unsigned long)count,
            (src == FEED_SOURCE_BUTTON) ? "button" : "schedule");
        vSerialPutString(str);

        rgb_blue_on(true);
        servo_set_angle(SERVO_OPEN_DEG);
        vTaskDelay(pdMS_TO_TICKS(SERVO_OPEN_TIME_MS));
        servo_set_angle(SERVO_CLOSED_DEG);
        rgb_blue_on(false);
    }
}

/*----------------------------------------------------------------------------*/
