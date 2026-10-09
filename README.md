# Pet feeder: FreeRTOS on the FRDM-MCXA153

MIC5 (RTOS) flexible project. An automatic pet feeder: a servo opens a hatch
so that one portion of food falls into the bowl. FreeRTOS spreads the work
over 7 tasks and 2 interrupt handlers.

- **Scheduled meals:** every 8 h (every 20 s in demo mode), one portion.
- **Feed button (SW3):** one extra portion.
- **Console commands** (MCU-Link VCOM, 115200 8N1): `feed [n]`,
  `interval <s>`, `status`, `help`.
- **Safety rules:** a cooldown after each meal (no double feeding) and a
  maximum number of portions per day. The counter resets every 24 h.
- **LEDs:** blue = feeding, red = daily limit reached, green blinking = idle.
- **Health report** on the console every 10 s: free heap, stack usage,
  dropped messages.

Everything you can tune is in [`source/app_config.h`](source/app_config.h):
times, portions, servo angles, priorities, stack sizes, demo/real timing and
the fault injection switch for test scenario 3.

## Project structure

```
source/               application (hardware-independent)
  main.c                bsp_init(), app_create(), start the scheduler
  app_tasks.c/.h        the 7 tasks, 2 ISR entry points, queues, mutex, timers, hooks
  app_config.h          all tunable settings + fault injection switch
  bsp.h                 hardware interface used by the application
  FreeRTOSConfig.h      documented kernel configuration
board/                hardware layer, register level (no SDK drivers)
  bsp_mcxa153_regs.c    clocks, pins, servo PWM, LEDs, UART, both ISRs
  runtime_stats.c/.h    CTIMER2 at 1 kHz for the run-time statistics (course file)
  mcux_config.h         SDK build configuration header
rtos/freertos-kernel/ FreeRTOS kernel V11.1.0 sources (from the course SDK)
armgcc/               CMake project, compiler flags, linker script
docs/                 high-level design, test scenarios, project plan
build.ps1             build / flash script
```

The application only reaches the hardware through `bsp.h`. The same
application code also ran in a PC simulator (FreeRTOS POSIX port) during the
design, which produced the expected timing diagrams in the test scenarios.

## Hardware

| What | Where |
|------|-------|
| Servo signal | P1_4 = CTIMER1 match 2, header J1-2, 50 Hz PWM, 1–2 ms pulse |
| Servo power | 5 V + GND (separate supply for bigger servos, shared GND) |
| Feed button | SW3 = P1_7 (GPIO1, falling-edge interrupt) |
| LEDs | on-board RGB: red P3_12, green P3_13, blue P3_0 (active low) |
| Console | LPUART0 P0_2/P0_3 via MCU-Link VCOM, 115200 8N1, type `help` |
| Timers | CTIMER1 = servo PWM, CTIMER2 = run-time statistics |

## Scheduling

**Chosen: fixed-priority preemptive scheduling with time slicing**
(`configUSE_PREEMPTION = 1`, `configUSE_TIME_SLICING = 1`). Priorities are
assigned by how strict each task's timing is: the stricter the timing, the
higher the priority.

| Task     | Prio | Activation                      | Timing requirement / reason                         |
|----------|-----:|---------------------------------|-----------------------------------------------------|
| Feeder   | 5    | request in FeedQ                | hatch open time sets the portion size: must close on time |
| Button   | 4    | button ISR (sporadic)           | user feedback within ~50 ms                          |
| Schedule | 3    | timeout of its interval / notification | meal time, seconds of jitter are fine         |
| Command  | 2    | bytes in the UART stream buffer | human typing speed                                   |
| Status   | 2    | event group bit                 | LEDs, visual only                                    |
| Tmr Svc  | 2    | Day timer (24 h), Heartbeat (1 s) | not critical                                       |
| Logger   | 1    | message in LogQ                 | background, UART output is slow                      |
| Monitor  | 1    | every 10 s                      | background                                           |

**Why preemptive:** the Feeder opens the hatch, waits `SERVO_OPEN_TIME_MS` and
closes it. If it closes late, the pet gets too much food. With preemption the
Feeder runs as soon as its delay expires, even when the Logger is busy
printing. The UART RX interrupt and the button are handled straight away too.
Test scenario 1 shows this on the board: the Feeder preempts the Command task
inside the `xQueueSend` call that hands it the request.

**Why time slicing is on:** Command/Status and Logger/Monitor share a priority.
Slicing makes sure neither task in a pair can starve the other. The Feeder and
Button tasks have a priority of their own, so slicing doesn't affect them.

**Why not the alternatives:**

- *Cooperative scheduling* (`configUSE_PREEMPTION = 0`): a task only gives
  up the CPU when it blocks or yields. A long `printf` or a command being
  parsed would delay closing the hatch, so the portion size would vary.
- *Round-robin with equal priorities*: every task would get the same share of
  CPU, whatever its urgency. Closing the hatch would wait for the time slices
  of up to 6 other tasks.
- *Earliest Deadline First (dynamic priorities)*: FreeRTOS doesn't support it
  natively and it is harder to analyse. The CPU load is very low (the tasks
  are almost always blocked, IDLE runs over 99 % of the time), so fixed
  priorities meet every deadline.
- *Super-loop / no RTOS*: the feeder has to wait for the servo, the UART, the
  button and the clock at the same time. In one loop that becomes a
  hand-written state machine full of timers. With tasks, each part is a
  simple blocking sequence.

## Inter-task communication

| Mechanism | Used for | Why this one |
|-----------|----------|--------------|
| Queue `FeedQ` (by copy) | Schedule, Button, Command, Day timer → Feeder | 4 producers, 1 consumer. Requests are **serialised**, so the servo is only ever driven by one task, and the request data (source, portions) travels with it |
| Queue `LogQ` (by copy) | Feeder, Schedule → Logger | the slow UART output is decoupled from the time-critical tasks. They never block on it (timeout 0, drops are counted) |
| Mailbox `StatsBox` (1-item queue, `xQueueOverwrite`/`xQueuePeek`) | Feeder → Command (`status`) | always holds the latest statistics. The writer never blocks, readers don't remove the value, and nothing is shared without protection |
| Stream buffer | UART RX ISR → Command | made for a byte stream with exactly one writer (the ISR) and one reader. Cheaper than a queue per byte |
| Binary semaphore `ButtonSem` | button ISR → Button | ISR-to-task synchronisation. The Button task drains extra edges from contact bounce (`xSemaphoreTake(…, 0)`) and waits for a stable release, so one press gives one request (test scenario 2) |
| Task notification with value | Command → Schedule (new interval in ms) | one known receiver and one 32-bit value. Fastest and needs no RAM. `eSetValueWithOverwrite` keeps only the latest interval |
| Event group `xAppEvents` | Feeder → Status (FEEDING, LIMIT), Heartbeat timer | state made of independent flags that several tasks read. A task can wait for any combination of them |
| Mutex `ConsoleMtx` | UART console shared by Logger, Command, Monitor | mutual exclusion with **priority inheritance**, which prevents unbounded priority inversion |
| Software timers | 24 h day rollover, heartbeat LED | periodic actions that don't need a task (saves a stack each) |

**Why not the alternatives:**

- *Global variables shared between tasks* (for example the portion counter):
  race conditions. Only the Feeder changes feeder state. Others only see a
  copy (StatsBox) or send requests (FeedQ). Even the day reset arrives as a
  request in FeedQ.
- *A mutex around the servo instead of FeedQ*: every task would drive the
  servo itself and the cooldown and limit rules would be spread over 3
  places. With one Feeder task the rules live in one place (test scenario 3
  shows what happens when that one rule is missing).
- *Binary semaphore for the console*: no priority inheritance.
- *Queue for the UART bytes*: works, but a stream buffer is meant for exactly
  this and copies bytes with less overhead.
- *Task notification for the button*: possible, but a semaphore is a named
  kernel object (`vQueueAddToRegistry`), so its count can be inspected in the
  debugger, which the test scenarios use.
- *Queue sets / message buffers*: not needed. No task waits on more than one
  object, and there are no variable-length messages.

## Interrupts

| ISR | Source | NVIC priority | Defers to |
|-----|--------|--------------:|-----------|
| `LPUART0_IRQHandler` | console UART, byte received | 3 | Command (stream buffer) |
| `GPIO1_IRQHandler`   | SW3 (P1_7), falling edge    | 4 | Button (semaphore)      |

Both priorities are numerically ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
(2). This is required for ISRs that call `…FromISR()` functions. The ISRs only
clear the flag and pass the event on to a task ("deferred interrupt
processing"). The servo PWM is generated by CTIMER1 in hardware, with no ISR
and no CPU load.

## RTOS configuration

[`source/FreeRTOSConfig.h`](source/FreeRTOSConfig.h) is the course file,
adjusted for this application. Every changed value has a comment that says
why. The main changes: `configMAX_PRIORITIES` 6, `configTOTAL_HEAP_SIZE`
14 KB, `configTIMER_TASK_PRIORITY` 2, `configCHECK_FOR_STACK_OVERFLOW` 2,
`INCLUDE_xTaskGetHandle` 1.

## Build

Needs the MIC5 course SDK
(`SDK_24_12_00_FRDM-MCXA153_FOR_MIC5_V10`, in `Downloads` or set
`MCUX_SDK_ROOT`) for the device startup code and register headers, the Arm
GNU toolchain and Ninja (installed by the MCUXpresso for VS Code extension
in `%USERPROFILE%\.mcuxpressotools`), CMake, and LinkServer or J-Link for
flashing. The FreeRTOS kernel is part of this repository (`rtos/`).

```
.\build.ps1            # debug build -> build\debug\pet_feeder.elf
.\build.ps1 -Release   # release build
.\build.ps1 -Clean     # rebuild from scratch (needed after adding a .c file)
.\build.ps1 -Flash     # build and flash with LinkServer
```

In VS Code: Ctrl+Shift+B builds, F5 debugs ("Debug pet_feeder (LinkServer)"
or "Debug pet_feeder (J-Link)", depending on the probe firmware). Every `.c`
file in `source/` and `board/` is compiled automatically.

## Documentation

| Document | Content |
|----------|---------|
| [`docs/design.md`](docs/design.md) | high-level design: tasks, ISRs and inter-task communication |
| [`docs/test_scenarios.md`](docs/test_scenarios.md) | 3 test scenarios with timing diagrams and RTOS-aware debugger screenshots, plus a startup HardFault found on the board |
| [`docs/project_plan.md`](docs/project_plan.md) | project plan (flexible project checklist) |
| [`docs/course_setup.md`](docs/course_setup.md) | running the application inside the MIC5 course example project instead |
