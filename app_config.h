/*
 * app_config.h - all tunable settings of the pet feeder in one place.
 *
 * This is the "flexible" part of the project: feeding times, portion size,
 * servo angles, priorities, queue sizes and the fault-injection switch used
 * for test scenario 3 are changed here only, never inside the task code.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---------- Demo vs. real timing ----------
 * APP_DEMO_TIMING = 1: everything is fast so it can be shown and tested
 *   (feed every 20 s, "day" of 2 minutes).
 * APP_DEMO_TIMING = 0: real pet feeder (feed every 8 h, 24 h day). */
#ifndef APP_DEMO_TIMING
#define APP_DEMO_TIMING             1
#endif

#if APP_DEMO_TIMING
#define APP_FEED_INTERVAL_MS        20000u
#define APP_DAY_MS                  120000u
#define APP_FEED_COOLDOWN_MS        2000u
#else
#define APP_FEED_INTERVAL_MS        (8u * 3600u * 1000u)
#define APP_DAY_MS                  (24u * 3600u * 1000u)
#define APP_FEED_COOLDOWN_MS        (30u * 60u * 1000u)   /* 30 min between feeds */
#endif

/* ---------- Feeding ---------- */
#define APP_PORTIONS_SCHEDULED      1u     /* portions per scheduled meal        */
#define APP_PORTIONS_BUTTON         1u     /* portions per button press          */
#define APP_MAX_PORTIONS_PER_DAY    6u     /* safety limit: no over-feeding      */
#define APP_MAX_PORTIONS_PER_FEED   3u     /* "feed <n>" is clamped to this      */

/* ---------- Servo (SG90-type: 50 Hz, 1.0..2.0 ms pulse = 0..180 deg) ---------- */
#define SERVO_PERIOD_US             20000u
#define SERVO_MIN_PULSE_US          1000u  /* 0 deg   - check your servo's datasheet */
#define SERVO_MAX_PULSE_US          2000u  /* 180 deg                                */
#define SERVO_ANGLE_CLOSED          0u     /* hatch closed                           */
#define SERVO_ANGLE_OPEN            90u    /* hatch open: food falls                 */
#define SERVO_OPEN_TIME_MS          600u   /* one portion = hatch open this long     */
#define SERVO_SETTLE_MS             300u   /* time for the servo to reach position   */

/* ---------- Misc timing ---------- */
#define APP_BUTTON_DEBOUNCE_MS      30u    /* press must still be down after this; release
                                              must be stable this long before the next press */
#define APP_BUTTON_POLL_MS          10u    /* SW3 poll period while waiting for the release */
#define APP_MONITOR_PERIOD_MS       10000u
#define APP_HEARTBEAT_PERIOD_MS     1000u

/* ---------- Task priorities (configMAX_PRIORITIES = 6) ----------
 * Highest priority = tightest timing requirement (see README, Scheduling). */
#define PRIO_FEEDER                 5u     /* servo timing defines the portion size */
#define PRIO_BUTTON                 4u     /* user feedback within ~50 ms           */
#define PRIO_SCHEDULE               3u
#define PRIO_COMMAND                2u     /* UART commands, human speed            */
#define PRIO_STATUS                 2u     /* LEDs                                  */
#define PRIO_LOGGER                 1u     /* slow UART output, background          */
#define PRIO_MONITOR                1u

/* ---------- Stack sizes in words (4 bytes each on Cortex-M33) ---------- */
#define STACK_FEEDER                256u
#define STACK_BUTTON                256u
#define STACK_SCHEDULE              256u
#define STACK_COMMAND               384u   /* line buffer + parsing              */
#define STACK_STATUS                256u
#define STACK_LOGGER                384u   /* printf-style formatting            */
#define STACK_MONITOR               384u

/* ---------- Queues / buffers ---------- */
#define APP_FEED_QUEUE_LEN          4u
#define APP_LOG_QUEUE_LEN           8u
#define APP_UART_RX_BUF_LEN         64u    /* stream buffer, bytes               */
#define APP_CMD_LINE_LEN            32u

/* ---------- Fault injection for test scenario 3 ----------
 * 0 = normal behaviour
 * 1 = Feeder does not check the cooldown -> a button press during a
 *     scheduled meal gives the pet a second meal (double feeding)
 * 2 = console mutex disabled -> interleaved/garbled output between tasks
 */
#ifndef APP_FAULT_MODE
#define APP_FAULT_MODE              0
#endif

#endif /* APP_CONFIG_H */
