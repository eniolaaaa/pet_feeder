/*
 * app_tasks.c - FreeRTOS pet feeder.
 *
 * Tasks (7):  Feeder, Button, Schedule, Command, Status, Logger, Monitor
 * ISRs   (2): feed button SW3   -> Button   (binary semaphore)
 *             UART RX (console) -> Command  (stream buffer)
 * Inter-task communication:
 *             FeedQ         Schedule/Button/Command/DayTimer -> Feeder (queue)
 *             LogQ          Feeder/Schedule/Command -> Logger (queue)
 *             StatsBox      Feeder -> Command "status" (1-item queue, mailbox)
 *             xAppEvents    Feeder -> Status (event group)
 *             notification  Command -> Schedule (new feed interval as value)
 *             xConsoleMutex shared UART console (mutex, priority inheritance)
 *             DayTimer      software timer, starts a new day (portion counter)
 *             Heartbeat     software timer, green LED blink while idle
 * See docs/design.md for the diagram.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"
#include "stream_buffer.h"

#include "app_config.h"
#include "app_tasks.h"
#include "bsp.h"

/* ---------- RTOS objects ---------- */
static TaskHandle_t         xScheduleTask;
static QueueHandle_t        xFeedQueue;
static QueueHandle_t        xLogQueue;
static QueueHandle_t        xStatsBox;
static SemaphoreHandle_t    xButtonSem;
static SemaphoreHandle_t    xConsoleMutex;
static EventGroupHandle_t   xAppEvents;
static StreamBufferHandle_t xUartRx;
static TimerHandle_t        xDayTimer;
static TimerHandle_t        xHeartbeatTmr;

/* Published by the Feeder through xStatsBox (xQueueOverwrite / xQueuePeek). */
typedef struct {
    uint32_t portions_today;
    uint32_t meals_total;
    uint32_t rejected_total;
    uint32_t last_feed_tick;
    uint32_t interval_ms;
} feeder_stats_t;

/* ---------- Debug counters (watch these in the debugger) ---------- */
volatile uint32_t g_buttonIrqs;
volatile uint32_t g_uartRxBytes;
volatile uint32_t g_uartRxDropped;   /* stream buffer full     */
volatile uint32_t g_feedReqDropped;  /* FeedQ full             */
volatile uint32_t g_logsDropped;     /* LogQ full              */

static const char *const SRC_NAME[] = { "schedule", "button", "command", "day" };

/* ---------- Thread-safe console ---------- */
static void app_printf(const char *fmt, ...)
{
    char buf[100];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

#if APP_FAULT_MODE == 2
    /* Fault: no mutex. Write char by char and yield in between, so another
     * task can interleave its output (what an unprotected UART looks like). */
    for (const char *p = buf; *p; p++) {
        char c[2] = { *p, 0 };
        bsp_console_write(c);
        taskYIELD();
    }
#else
    xSemaphoreTake(xConsoleMutex, portMAX_DELAY);
    bsp_console_write(buf);
    xSemaphoreGive(xConsoleMutex);
#endif
}

static void log_text(const char *text)
{
    log_msg_t m = { .kind = LOG_TEXT, .tick = xTaskGetTickCount() };
    strncpy(m.text, text, sizeof m.text - 1);
    if (xQueueSend(xLogQueue, &m, 0) != pdPASS) g_logsDropped++;
}

static void request_feed(feed_source_t src, uint8_t portions)
{
    feed_req_t r = { src, portions, xTaskGetTickCount() };
    if (xQueueSend(xFeedQueue, &r, 0) != pdPASS) g_feedReqDropped++;
}

/* =====================================================================
 * Interrupt handlers (called by the BSP). Keep them short: defer work.
 * ===================================================================== */
void app_button_isr(void)
{
    BaseType_t woken = pdFALSE;
    g_buttonIrqs++;
    xSemaphoreGiveFromISR(xButtonSem, &woken);
    portYIELD_FROM_ISR(woken);
}

void app_uart_rx_isr(uint8_t c)
{
    BaseType_t woken = pdFALSE;
    g_uartRxBytes++;
    if (xStreamBufferSendFromISR(xUartRx, &c, 1, &woken) != 1) g_uartRxDropped++;
    portYIELD_FROM_ISR(woken);
}

/* =====================================================================
 * Tasks
 * ===================================================================== */

/* Feeder: the only task that drives the servo. Serialises all feed requests
 * and enforces the cooldown and the daily limit. */
static void FeederTask(void *arg)
{
    (void)arg;
    feeder_stats_t st = { .interval_ms = APP_FEED_INTERVAL_MS,
                          .last_feed_tick = 0 };
    bool fed_before = false;
    feed_req_t r;

    bsp_servo_set_angle(SERVO_ANGLE_CLOSED);
    xQueueOverwrite(xStatsBox, &st);

    for (;;) {
        xQueueReceive(xFeedQueue, &r, portMAX_DELAY);
        TickType_t now = xTaskGetTickCount();
        log_msg_t m = { .source = r.source, .portions = r.portions, .tick = now };

        if (r.portions == 0) {                       /* new-day marker from DayTimer */
            st.portions_today = 0;
            xEventGroupClearBits(xAppEvents, EVT_LIMIT);
            xEventGroupSetBits(xAppEvents, EVT_STATE_CHANGE);
            m.kind = LOG_NEW_DAY;
        }
#if APP_FAULT_MODE != 1
        else if (fed_before &&
                 (now - st.last_feed_tick) < pdMS_TO_TICKS(APP_FEED_COOLDOWN_MS)) {
            st.rejected_total++;
            m.kind = LOG_REJECTED_COOLDOWN;
        }
#endif
        else if (st.portions_today + r.portions > APP_MAX_PORTIONS_PER_DAY) {
            st.rejected_total++;
            m.kind = LOG_REJECTED_LIMIT;
            xEventGroupSetBits(xAppEvents, EVT_LIMIT | EVT_STATE_CHANGE);
        } else {
            fed_before = true;
            xEventGroupSetBits(xAppEvents, EVT_FEEDING | EVT_STATE_CHANGE);
            for (uint8_t p = 0; p < r.portions; p++) {
                bsp_servo_set_angle(SERVO_ANGLE_OPEN);
                vTaskDelay(pdMS_TO_TICKS(SERVO_SETTLE_MS + SERVO_OPEN_TIME_MS));
                bsp_servo_set_angle(SERVO_ANGLE_CLOSED);
                vTaskDelay(pdMS_TO_TICKS(SERVO_SETTLE_MS));
            }
            st.portions_today += r.portions;
            st.meals_total++;
            st.last_feed_tick = xTaskGetTickCount();  /* cooldown counts from the END of a meal */
            xEventGroupClearBits(xAppEvents, EVT_FEEDING);
            xEventGroupSetBits(xAppEvents, EVT_STATE_CHANGE);
            m.kind = LOG_FED;
        }
        m.today = (uint8_t)st.portions_today;
        xQueueOverwrite(xStatsBox, &st);             /* publish latest stats */
        if (xQueueSend(xLogQueue, &m, 0) != pdPASS) g_logsDropped++;
    }
}

/* Button: deferred handling of the SW3 interrupt, with debounce. One press
 * gives one request: after a request the task waits until SW3 has been
 * released for APP_BUTTON_DEBOUNCE_MS without a break, and only then drops
 * the edges collected so far. Test scenario 2 showed that SW3 can bounce for
 * longer than the debounce time, and that each late bounce then started a
 * new debounce cycle that still read the button as pressed. */
static void ButtonTask(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(xButtonSem, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(APP_BUTTON_DEBOUNCE_MS));
        if (bsp_button_is_pressed()) {
            request_feed(FEED_BUTTON, APP_PORTIONS_BUTTON);
            uint32_t released_ms = 0;
            while (released_ms < APP_BUTTON_DEBOUNCE_MS) {
                vTaskDelay(pdMS_TO_TICKS(APP_BUTTON_POLL_MS));
                released_ms = bsp_button_is_pressed() ? 0 : released_ms + APP_BUTTON_POLL_MS;
            }
        }
        xSemaphoreTake(xButtonSem, 0);      /* drop edges caused by bounce */
    }
}

/* Schedule: requests a meal every interval. A task notification from the
 * Command task carries a new interval (ms) and restarts the wait. */
static void ScheduleTask(void *arg)
{
    (void)arg;
    uint32_t interval = APP_FEED_INTERVAL_MS;
    for (;;) {
        uint32_t new_interval;
        if (xTaskNotifyWait(0, UINT32_MAX, &new_interval,
                            pdMS_TO_TICKS(interval)) == pdTRUE) {
            interval = new_interval;
            char t[40];
            snprintf(t, sizeof t, "interval now %lu s", (unsigned long)(interval / 1000u));
            log_text(t);
            continue;                       /* restart the wait */
        }
        request_feed(FEED_SCHEDULED, APP_PORTIONS_SCHEDULED);
    }
}

/* Command: line-based UART commands from the stream buffer. */
static void CommandTask(void *arg)
{
    (void)arg;
    char line[APP_CMD_LINE_LEN];
    size_t len = 0;
    for (;;) {
        uint8_t c;
        xStreamBufferReceive(xUartRx, &c, 1, portMAX_DELAY);
        if (c != '\r' && c != '\n') {
            if (len < sizeof line - 1) line[len++] = (char)c;
            continue;
        }
        if (len == 0) continue;
        line[len] = '\0';
        len = 0;

        if (strncmp(line, "feed", 4) == 0) {
            int n = (line[4] == ' ') ? atoi(&line[5]) : 1;
            if (n < 1) n = 1;
            if (n > (int)APP_MAX_PORTIONS_PER_FEED) n = APP_MAX_PORTIONS_PER_FEED;
            request_feed(FEED_COMMAND, (uint8_t)n);
        } else if (strncmp(line, "interval ", 9) == 0) {
            long s = atol(&line[9]);
            if (s >= 5) {
                xTaskNotify(xScheduleTask, (uint32_t)s * 1000u, eSetValueWithOverwrite);
            } else {
                app_printf("interval must be >= 5 s\r\n");
            }
        } else if (strcmp(line, "status") == 0) {
            feeder_stats_t st;
            xQueuePeek(xStatsBox, &st, 0);
            app_printf("STATUS today=%lu/%u meals=%lu rejected=%lu last=%lu %s\r\n",
                       (unsigned long)st.portions_today, APP_MAX_PORTIONS_PER_DAY,
                       (unsigned long)st.meals_total, (unsigned long)st.rejected_total,
                       (unsigned long)st.last_feed_tick,
                       (xEventGroupGetBits(xAppEvents) & EVT_FEEDING) ? "FEEDING" : "idle");
        } else {
            app_printf("commands: feed [n] | interval <s> | status\r\n");
        }
    }
}

/* Status: LEDs. blue = feeding, red = daily limit, green blink = idle (timer). */
static void StatusTask(void *arg)
{
    (void)arg;
    for (;;) {
        EventBits_t b = xEventGroupWaitBits(xAppEvents, EVT_STATE_CHANGE,
                                            pdTRUE, pdFALSE, portMAX_DELAY);
        bsp_led_set(BSP_LED_BLUE, (b & EVT_FEEDING) != 0);
        bsp_led_set(BSP_LED_RED,  (b & EVT_LIMIT) != 0);
        if (b & EVT_FEEDING) bsp_led_set(BSP_LED_GREEN, false);
    }
}

/* Logger: lowest priority, prints events on the UART console. */
static void LoggerTask(void *arg)
{
    (void)arg;
    log_msg_t m;
    for (;;) {
        xQueueReceive(xLogQueue, &m, portMAX_DELAY);
        unsigned long t = (unsigned long)m.tick;
        switch (m.kind) {
        case LOG_FED:
            app_printf("[%7lu] FED %u portion(s) (%s), today %u/%u\r\n", t,
                       m.portions, SRC_NAME[m.source], m.today, APP_MAX_PORTIONS_PER_DAY);
            break;
        case LOG_REJECTED_COOLDOWN:
            app_printf("[%7lu] REJECTED %s: cooldown\r\n", t, SRC_NAME[m.source]);
            break;
        case LOG_REJECTED_LIMIT:
            app_printf("[%7lu] REJECTED %s: daily limit\r\n", t, SRC_NAME[m.source]);
            break;
        case LOG_NEW_DAY:
            app_printf("[%7lu] NEW DAY\r\n", t);
            break;
        default:
            app_printf("[%7lu] %s\r\n", t, m.text);
            break;
        }
    }
}

/* Monitor: periodic health report (heap, stack high-water marks, drops). */
static void MonitorTask(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(APP_MONITOR_PERIOD_MS));
        app_printf("MON heapFree=%u minEver=%u btnIrq=%lu rx=%lu dropRx=%lu dropFeed=%lu dropLog=%lu\r\n",
                   (unsigned)xPortGetFreeHeapSize(), (unsigned)xPortGetMinimumEverFreeHeapSize(),
                   (unsigned long)g_buttonIrqs, (unsigned long)g_uartRxBytes,
                   (unsigned long)g_uartRxDropped, (unsigned long)g_feedReqDropped,
                   (unsigned long)g_logsDropped);
        app_printf("MON stackFree(words) feeder=%u command=%u logger=%u monitor=%u\r\n",
                   (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("Feeder")),
                   (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("Command")),
                   (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("Logger")),
                   (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
}

/* ---------- Software timer callbacks (run in the timer service task) ---------- */
static void DayTimerCallback(TimerHandle_t t)
{
    (void)t;
    feed_req_t r = { FEED_SCHEDULED, 0, xTaskGetTickCount() };  /* 0 = new day */
    if (xQueueSend(xFeedQueue, &r, 0) != pdPASS) g_feedReqDropped++;  /* never block here */
}

static void HeartbeatCallback(TimerHandle_t t)
{
    (void)t;
    if (!(xEventGroupGetBits(xAppEvents) & EVT_FEEDING)) bsp_led_toggle(BSP_LED_GREEN);
}

/* ===================================================================== */
void app_create(void)
{
    xFeedQueue    = xQueueCreate(APP_FEED_QUEUE_LEN, sizeof(feed_req_t));
    xLogQueue     = xQueueCreate(APP_LOG_QUEUE_LEN, sizeof(log_msg_t));
    xStatsBox     = xQueueCreate(1, sizeof(feeder_stats_t));
    xButtonSem    = xSemaphoreCreateBinary();
    xConsoleMutex = xSemaphoreCreateMutex();
    xAppEvents    = xEventGroupCreate();
    xUartRx       = xStreamBufferCreate(APP_UART_RX_BUF_LEN, 1);
    xDayTimer     = xTimerCreate("Day", pdMS_TO_TICKS(APP_DAY_MS), pdTRUE, NULL, DayTimerCallback);
    xHeartbeatTmr = xTimerCreate("Heartbeat", pdMS_TO_TICKS(APP_HEARTBEAT_PERIOD_MS),
                                 pdTRUE, NULL, HeartbeatCallback);
    configASSERT(xFeedQueue && xLogQueue && xStatsBox && xButtonSem && xConsoleMutex &&
                 xAppEvents && xUartRx && xDayTimer && xHeartbeatTmr);

    /* Names show up in the RTOS-aware debugger (Queue List view). */
    vQueueAddToRegistry(xFeedQueue,    "FeedQ");
    vQueueAddToRegistry(xLogQueue,     "LogQ");
    vQueueAddToRegistry(xStatsBox,     "StatsBox");
    vQueueAddToRegistry(xButtonSem,    "ButtonSem");
    vQueueAddToRegistry(xConsoleMutex, "ConsoleMtx");

    BaseType_t ok = pdPASS;
    ok &= xTaskCreate(FeederTask,   "Feeder",   STACK_FEEDER,   NULL, PRIO_FEEDER,   NULL);
    ok &= xTaskCreate(ButtonTask,   "Button",   STACK_BUTTON,   NULL, PRIO_BUTTON,   NULL);
    ok &= xTaskCreate(ScheduleTask, "Schedule", STACK_SCHEDULE, NULL, PRIO_SCHEDULE, &xScheduleTask);
    ok &= xTaskCreate(CommandTask,  "Command",  STACK_COMMAND,  NULL, PRIO_COMMAND,  NULL);
    ok &= xTaskCreate(StatusTask,   "Status",   STACK_STATUS,   NULL, PRIO_STATUS,   NULL);
    ok &= xTaskCreate(LoggerTask,   "Logger",   STACK_LOGGER,   NULL, PRIO_LOGGER,   NULL);
    ok &= xTaskCreate(MonitorTask,  "Monitor",  STACK_MONITOR,  NULL, PRIO_MONITOR,  NULL);
    configASSERT(ok == pdPASS);

    xTimerStart(xDayTimer, 0);
    xTimerStart(xHeartbeatTmr, 0);

    /* Interrupts last: their handlers use the objects created above. */
    bsp_enable_irqs();
}

/* ---------- FreeRTOS hooks ---------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    bsp_console_write("STACK OVERFLOW: ");
    bsp_console_write(pcTaskName);
    bsp_console_write("\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    bsp_console_write("MALLOC FAILED\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
