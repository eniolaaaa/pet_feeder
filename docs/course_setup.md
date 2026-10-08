# Using the code in the MIC5 course project (register level, no SDK drivers)

The course-example project compiles every `.c` file in its folder. Copy these
files into it:

| Copy | Notes |
|------|-------|
| `source/app_tasks.c`, `app_tasks.h`, `app_config.h`, `bsp.h` | the application, unchanged |
| `board/frdmmcxa153_course/bsp_mcxa153_regs.c` | register-level board layer |
| `source/main.c` | **replace** the course `main.c` (two `main()` functions will not link), or call `bsp_init(); app_create(); vTaskStartScheduler();` from the course `main()` |

Do **not** copy `board/frdmmcxa153/bsp_mcxa153.c`, because it needs the NXP SDK
drivers. Do not copy `sim/` either.

## Check for duplicates
- `vApplicationStackOverflowHook` and `vApplicationMallocFailedHook` are
  defined in `app_tasks.c`. If the course project defines them too, delete
  one copy.
- `GPIO1_IRQHandler` and `LPUART0_IRQHandler` are defined in the board file.
  If the course code has its own, keep one.
- If the course code already sets up LPUART0 (pins, clock, baud rate), build
  with `BSP_INIT_LPUART0=0`. The RX interrupt is still enabled.

## The course FreeRTOSConfig.h must have
Keep the course file, but check these values (the reasons are in
`source/FreeRTOSConfig.h`):

```c
#define configUSE_PREEMPTION                  1
#define configMAX_PRIORITIES                  6      /* or more            */
#define configTOTAL_HEAP_SIZE                 (14 * 1024)  /* at least     */
#define configUSE_MUTEXES                     1
#define configUSE_TIMERS                      1
#define configTIMER_TASK_PRIORITY             2
#define configTIMER_TASK_STACK_DEPTH          (configMINIMAL_STACK_SIZE * 2)
#define configUSE_TASK_NOTIFICATIONS          1
#define configQUEUE_REGISTRY_SIZE             8
#define configUSE_TRACE_FACILITY              1      /* debugger task view */
#define configCHECK_FOR_STACK_OVERFLOW        2
#define configUSE_MALLOC_FAILED_HOOK          1
#define INCLUDE_xTaskDelayUntil               1
#define INCLUDE_xTaskGetHandle                1
#define INCLUDE_uxTaskGetStackHighWaterMark   1
#define INCLUDE_vTaskSuspend                  1
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  2  /* ISRs use 3 and 4 */
```

The kernel files `timers.c`, `event_groups.c` and `stream_buffer.c` must be in
the build, next to `tasks.c`, `queue.c` and `list.c`.

## Hardware
- Servo signal → J1-2 (P1_4). Servo + → 5 V, servo GND → board GND.
- Feed button: SW3. LEDs: on-board RGB.
- Serial terminal: MCU-Link VCOM, 115200 8N1. Type `help`.
