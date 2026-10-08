# FreeRTOS Pet Feeder — FRDM-MCXA153 (Flexible Project)

> Status: first draft. The RTOS application is built and tested in the PC
> simulator. The board layer still has to be built and run on the
> FRDM-MCXA153 (see "Building for the board"). Sections marked **TODO(eni)**
> must be written or checked by you, in your own words.

## What it does

An automatic **pet feeder**. A servo motor opens a hatch so that one portion
of food falls into the bowl. FreeRTOS distributes the work over 7 tasks:

- **Scheduled meals:** every 8 h (20 s in demo mode), one portion.
- **Manual feed button (SW3):** one extra portion.
- **Serial commands** (USB serial, 115200 8N1): `feed [n]`, `interval <s>`,
  `status`, `help`.
- **Safety rules:** a cooldown after each meal (no double feeding) and a
  maximum number of portions per day. The counter resets every 24 h.
- **LEDs:** blue = feeding, red = daily limit reached, green blinking = idle.
- **Health report** on the console: free heap, stack usage, dropped messages.

Hardware: FRDM-MCXA153 and an SG90-type servo. The signal goes to J1-2 (P1_4,
CTIMER1 PWM, 50 Hz). Power the servo from 5 V and connect its GND to the
board GND.

Everything you can tune is in [`source/app_config.h`](source/app_config.h):
times, portions, servo angles and pulse widths, priorities, stack sizes,
queue lengths, demo/real timing and fault injection.

## Project structure

```
source/            application, identical for board and simulator
  main.c             init + scheduler start
  app_tasks.c/.h     the 7 tasks, 2 ISR entry points, RTOS objects
  app_config.h       all tunable settings + fault injection switch
  bsp.h              hardware interface used by the application
  FreeRTOSConfig.h   documented kernel configuration for the MCXA153
board/frdmmcxa153/ board layer using the NXP SDK drivers
board/frdmmcxa153_course/ board layer at register level (MIC5 course project)
sim/               PC simulator (FreeRTOS POSIX port), same app code
tools/             trace_to_ascii.py: timing diagram from a simulator trace
docs/              design, test scenarios, project plan, progress log
```

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
  are almost always blocked), so fixed priorities meet every deadline.
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
| Binary semaphore `ButtonSem` | button ISR → Button | ISR-to-task synchronisation. The Button task can also drain extra edges from contact bounce (`xSemaphoreTake(…, 0)`) |
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
  places. With one Feeder task the rules live in one place.
- *Binary semaphore for the console*: no priority inheritance.
- *Queue for the UART bytes*: works, but a stream buffer is meant for exactly
  this and copies bytes with less overhead.
- *Task notification for the button*: possible, but a named semaphore is
  visible in the debugger's queue view, which helps in the test scenarios.
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

## Building for the board

**Using the MIC5 course example project (register level, no SDK drivers)?**
Follow [`docs/course_setup.md`](docs/course_setup.md) and use
`board/frdmmcxa153_course/bsp_mcxa153_regs.c`. The steps below are for a
project based on the NXP SDK.

1. Install MCUXpresso IDE and the MCUXpresso SDK for FRDM-MCXA153
   (with the FreeRTOS component).
2. *Import SDK example* → `frdmmcxa153` → `rtos_examples/freertos_hello`.
   This brings in the FreeRTOS kernel sources, the Cortex-M33 port, startup
   code, `board.c`, `clock_config.c`, `pin_mux.c` and the debug console.
3. Delete the example's `freertos_hello.c`, copy `source/*.c/*.h` and
   `board/frdmmcxa153/bsp_mcxa153.c` into the project's `source` folder and
   replace its `FreeRTOSConfig.h` with ours.
4. Add the SDK drivers `ctimer`, `gpio`, `port`, `lpuart` (*Manage SDK components*)
   and make sure `stream_buffer.c` from the FreeRTOS kernel is in the build.
5. Build, flash, and open a serial terminal at 115200 baud.

The FreeRTOS kernel sources are part of the exported project, so they're
included when you hand it in (study guide requirement). Don't hand in the
`Debug/` folder (build output).

## Running the simulator (no board needed)

```
git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git freertos-kernel
cmake -S sim -B build-sim && cmake --build build-sim
./build-sim/rtos_sim        # scripted run (sim/bsp_posix.c SCRIPT[]), writes sim_trace.csv
python3 tools/trace_to_ascii.py sim_trace.csv 14990 16215
cmake -S sim -B build-f1 -DAPP_FAULT_MODE=1 && cmake --build build-f1 && ./build-f1/rtos_sim
```
