#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

typedef enum { FEED_SCHEDULED = 0, FEED_BUTTON, FEED_COMMAND } feed_source_t;

/* Request to the Feeder task (FeedQ) */
typedef struct {
    feed_source_t source;
    uint8_t       portions;
    uint32_t      tick;          /* when the request was made */
} feed_req_t;

typedef enum { LOG_FED = 0, LOG_REJECTED_COOLDOWN, LOG_REJECTED_LIMIT,
               LOG_NEW_DAY, LOG_TEXT } log_kind_t;

/* Message to the Logger task (LogQ) */
typedef struct {
    log_kind_t    kind;
    feed_source_t source;
    uint8_t       portions;
    uint8_t       today;         /* portions given today after this event */
    uint32_t      tick;
    char          text[40];      /* only for LOG_TEXT */
} log_msg_t;

/* Event group bits */
#define EVT_FEEDING       (1u << 0)   /* servo is moving / hatch open   */
#define EVT_LIMIT         (1u << 1)   /* daily limit reached            */
#define EVT_NEW_DAY       (1u << 2)   /* set by day timer, Feeder clears */
#define EVT_STATE_CHANGE  (1u << 3)   /* wakes the Status (LED) task    */

void app_create(void);   /* creates all RTOS objects and tasks */

#endif
