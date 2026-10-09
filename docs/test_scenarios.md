# Test scenarios — pet feeder

Each scenario has an **expected behaviour** (timing diagram) and the **actual
behaviour** from the RTOS-aware debugger.

How the diagrams were made: the expected diagrams follow from the design.
They were checked in the PC simulator, which runs the same application code,
injects the button presses and UART commands from a script
(`sim/bsp_posix.c`) and logs every context switch to `sim_trace.csv`.
`tools/trace_to_ascii.py` turns a time window into a diagram.
Tick = 1 ms. Demo timing (`APP_DEMO_TIMING = 1`).

**TODO(eni), actual behaviour on the board:** for each scenario, set the
breakpoints listed, trigger the event, and take screenshots of the task and
queue tables in the *RTOS Details* view (MCUXpresso for VS Code, debug
configuration "Debug pet_feeder (J-Link)"). Put them under "Actual" and
write down where they match the expected diagram and where they differ. The
study guide asks for this debugger evidence specifically. Simulator output
doesn't replace it.

---

## Scenario 1: UART command → meal (ISR, stream buffer, queue, event group)

**Goal:** typed characters arrive through an interrupt, are assembled into a
command by a low-priority task, and the resulting request preempts it at
once, because the Feeder has a higher priority.

**Input:** `feed 2` + Enter at t = 2000 ms.

**Expected** (simulator, 2000–2010 ms, 20 switches):

```
tick      | 2001 2002 2003 2004 2005 2006 2007
Command   |  #    #    #    #    #    #    # #     one RX ISR per char -> stream buffer
Feeder    |  .    .    .    .    .    .    #       '\n' -> FeedQ -> Feeder preempts Command
Status    |  .    .    .    .    .    .    #       FEEDING bit -> blue LED on
order @2007: Command -> Feeder (servo 90 deg, then blocks in vTaskDelay)
             -> Status -> Command (finishes) -> IDLE
then: 2907 servo 0 deg, 3207 servo 90 deg, 4107 servo 0 deg, 4407 blue LED off,
      Logger prints "FED 2 portion(s) (command), today 2/6"
```

**Actual (board, 2026-10-09):** debug build, "Debug pet_feeder (J-Link)",
RTOS Details view of MCUXpresso for VS Code. That view only lists tasks, so
the FeedQ fill level is read in the Watch window from the kernel's queue
struct: `((struct QueueDefinition *)xFeedQueue)->uxMessagesWaiting` /
`->uxLength`. Console log:
[test_logs/2026-10-09_scenario1_console.txt](test_logs/2026-10-09_scenario1_console.txt).

*Stop 1:* breakpoint at `request_feed(FEED_COMMAND …)` in `CommandTask`
(`app_tasks.c:237`), after typing `feed 2`.

![Stop 1, tasks](screenshots/s1_a_tasks.png)
![Stop 1, FeedQ](screenshots/s1_a_queue.png)

| | Expected | Observed |
|---|---|---|
| Command | Running | Running |
| Feeder | Blocked on FeedQ | Suspended |
| FeedQ | 0 / 4 | 0 / 4 |

Feeder, Button, Status and Logger show *Suspended*, not *Blocked*: they wait
with `portMAX_DELAY`, and with `INCLUDE_vTaskSuspend = 1` FreeRTOS keeps such a
task on the suspended list (no timeout to track). Schedule, Monitor and Tmr Svc
wait with a timeout and show *Blocked*.

*Stop 2:* breakpoint on the first line after `xQueueReceive` in `FeederTask`
(`app_tasks.c:140`), reached with Continue (F5) from stop 1.

![Stop 2, tasks](screenshots/s1_b_tasks.png)
![Stop 2, FeedQ](screenshots/s1_b_queue.png)

| | Expected | Observed |
|---|---|---|
| Feeder | Running | Running |
| Command | Ready (preempted) | Ready |
| FeedQ | request in and taken out at once | 0 / 4 |

FeedQ is never seen at 1/4 with these breakpoints: the Feeder (priority 5)
preempts the Command task inside `xQueueSend` and empties the queue before
any breakpoint in the Command task can be reached. The Runtime column is not
meaningful across breakpoints: CTIMER2 keeps counting while the core is
halted, and that time is charged to the task that was running at the stop
(Command 80 % at stop 2).

*Outcome:* the `feed 2` of this run was `REJECTED command: cooldown`
(tick 221 218). It arrived 18 ms after the scheduled meal of tick 220 000
ended (1 portion = 1.2 s), inside `APP_FEED_COOLDOWN_MS` (2 s). A second try
at tick 302 557 was rejected the same way (1.36 s after a scheduled meal).
The simulator run sends `feed 2` at 2000 ms, far from any scheduled meal, so
it never meets the cooldown. Repeated without breakpoints after
`interval 60` (next scheduled meal 60 s away): `FED 2 portion(s) (command),
today 6/6` at tick 578 167. The servo opened and closed twice, and the
day counter went from 4/6 to 6/6.

**Comparison:**

| Aspect | Expected | Actual | Verdict |
|---|---|---|---|
| Preemption | Feeder (prio 5) takes over from Command (prio 2) as soon as the request is queued | Stop 2: Feeder Running, Command Ready | match |
| FeedQ | 1/4 → 0/4 | 0/4 at both stops | the 1/4 state can't be seen: the Feeder empties the queue inside the same `xQueueSend` call. Confirms the immediate preemption |
| Waiting tasks | Feeder *Blocked* on FeedQ | Feeder *Suspended* | naming only: an indefinite wait is kept on the suspended list. Expectation should read "waiting (Suspended)" |
| Run-time % | not part of the expectation | Command 80 % at stop 2 | debugger artefact: CTIMER2 runs while the core is halted |
| `feed 2` outcome | `FED 2 portion(s)` | first two tries `REJECTED command: cooldown`, `FED 2` after `interval 60` | behaviour correct (cooldown requirement), but the test input timing matters: on the board the schedule keeps running, in the simulator script it was not near a meal |

The RTOS mechanisms (ISR → stream buffer → Command task → FeedQ → Feeder,
priority preemption) behave as designed. The deviations come from how the
debugger shows kernel state and from the test setup, not from the application.
To make the scenario repeatable, set `interval 60` before sending `feed 2`.

**Conclusion:** the test confirmed that a console command reaches the Feeder
through the UART interrupt, the stream buffer and FeedQ, and that the Feeder
takes over the CPU as soon as the request is queued, because it has the
higher priority. Two things differed from what I expected. The debugger shows
tasks that wait forever as Suspended, not Blocked. And my first `feed 2` was
refused, because a scheduled meal had just finished and the 2 s cooldown was
still running. That showed me the safety rule works for every source, and
that on the real board the schedule keeps running during a test, unlike in my
simulator script. Next time I set `interval 60` first, so the test is
repeatable.

*TODO(eni): draft written with AI help, edit into your own words.*

---

## Scenario 2: button during a meal, debounce and cooldown

**Goal:** a sporadic interrupt is deferred to a task and debounced, a bounce
does not cause a second request, and a request that arrives while the servo
is moving is rejected by the cooldown rule.

**Input:** SW3 pressed at t = 3000 ms with a bounce at 3005 ms, while the
`feed 2` meal from scenario 1 is still running.

**Expected** (simulator, 2900–4410 ms, 19 switches):

```
2907 Feeder   servo closed after portion 1, blocks (settle)
3001 Button   takes ButtonSem, sleeps 30 ms (debounce); bounce @3005 gives it again
3031 Button   drains the extra give, pin still low -> FeedQ (1 request waiting)
3207 Feeder   portion 2, servo open
4107 Feeder   servo closed
4407 Feeder   meal done -> takes the button request from FeedQ ->
              0 ms since the meal ended < 2000 ms cooldown -> REJECTED
     Status, Logger: "FED 2 …", "REJECTED button: cooldown"
Monitor later: btnIrq=2 (2 edges), 1 request
```

**Actual:** breakpoint in `ButtonTask` after the first `xSemaphoreTake`.
Press SW3 while the blue LED is on. Task List: Button *Running*, Feeder
*Blocked* (delay). Then set a breakpoint on `st.rejected_total++` (cooldown
branch) in `FeederTask`.
*TODO(eni): screenshots + comparison.*

---

## Scenario 3: double feeding (deliberate mismatch)

**Goal:** a comprehensive situation (more than 10 task switches, several
communication paths) where the actual behaviour does **not** match the
requirement, and finding the cause.

**Requirement:** a button press during a scheduled meal must not give the pet
a second meal ("max 1 meal per cooldown period").

Build with `APP_FAULT_MODE=1`. The Feeder no longer checks the cooldown.
Input: `interval 10` at 5000 ms, so the next scheduled meal comes at
~15 000 ms. SW3 is pressed at 15 300 ms, during that meal.

**Expected (normal build, 14990–16215 ms, 20 switches):**

```
15012 Schedule -> FeedQ -> Feeder (servo 90) -> Schedule -> Status (blue on)
15302 Button (debounce) ... 15332 Button -> FeedQ (waits, Feeder busy)
15912 Feeder servo 0
16212 Feeder meal done; takes button request -> REJECTED (cooldown)
      -> Status (blue off) -> Logger
```

**Actual (fault build, 14990–17420 ms, 28 switches):**

```
15013 Schedule -> Feeder (servo 90) -> Schedule -> Status
15301 Button ... 15331 Button -> FeedQ
15913 Feeder servo 0
16213 Feeder: takes button request, no cooldown check -> servo 90 again
      -> Status -> Logger ("FED 1 (schedule)")
17113 Feeder servo 0
17413 Feeder done -> Status -> Logger ("FED 1 (button), today 5/6")
```

Mismatch: the pet got 2 meals 0 ms apart (`today` 4 → 5), and the blue LED
stays on for 2.4 s instead of 1.2 s.

**Analysis:** tasks involved: Button (producer), Schedule (producer), Feeder
(consumer) and FeedQ. The queue works correctly. It stores the button request
while the Feeder is busy and hands it over afterwards. The error is in the
Feeder's rules: with no cooldown, a request that was queued during a meal is
carried out right after it. Because the Feeder has the highest priority, it
starts the second meal straight away, before the Logger has even printed the
first one. You can see this in the trace: Logger runs only after Feeder
blocks again.

**Solution:** the cooldown check in `FeederTask` (normal build), measured from
the *end* of the previous meal. Alternatives considered:
1. Flush FeedQ after each meal (`xQueueReset`): this also drops legitimate
   requests such as the new-day marker, so it's rejected.
2. Ignore the button while `FEEDING` is set (check in ButtonTask): this
   doesn't cover the `feed` command, so the rule belongs in the Feeder, which
   owns the servo.

**Actual on the board:** build with `APP_FAULT_MODE=1` and
`APP_DEMO_TIMING=1`, type `interval 10`, and press SW3 while the blue LED is
on after a scheduled meal. Breakpoint on `m.kind = LOG_FED`. It hits twice in
a row, and Queue List shows FeedQ 1/4 at the first hit.
*TODO(eni): screenshots.*

---

## Extra test: daily limit and console commands (board, console log)

**Goal:** the Feeder stops feeding once `APP_MAX_PORTIONS_PER_DAY` (6) is
reached, from every source, until the day timer starts a new day. Unknown
commands get the help text.

**Log:** [test_logs/2026-10-07_console_daily_limit.txt](test_logs/2026-10-07_console_daily_limit.txt)
(normal build, demo timing, timestamps in ticks of 10 ms).

| Tick (s) | Event | Expected | Observed |
|---|---|---|---|
| 14000 → 20000 (140–200 s) | 4 scheduled + 2 `feed` commands | fed, `today` counts 1/6 → 6/6 | as expected |
| 17424 / 18000 | `feed`, then scheduled meal 5.8 s later | both fed (more than 2 s cooldown) | as expected |
| 22000 (220 s) | scheduled meal at 6/6 | rejected, red LED on | `REJECTED schedule: daily limit` |
| 22568 | `feed 2` at 6/6 | rejected | `REJECTED command: daily limit` |
| 24000 (240 s) | day timer | `NEW DAY`, counter 0, red LED off | `NEW DAY` |
| — | `f`, `nn`, `interval` (no value) | help text | help text |
| whole run | Monitor | no drops, heap stable | `dropRx/dropFeed/dropLog=0`, `heapFree=872` constant |

*TODO(eni): conclusion in your own words, and whether you saw the red LED
between 220 s and 240 s.*

---

*Optional extra (fault mode 2):* without the console mutex, the output of the
Command `status` reply and the Logger can interleave. This shows why the mutex
is needed.

---

## Found on the board: HardFault at startup (2026-10-07)

Not a planned scenario. This was a real fault found the first time the
application ran on the FRDM-MCXA153, after it had only been run in the PC
simulator. It is listed here because it is a mismatch between expected and
actual behaviour that was found and fixed with the debugger.

**Expected:** after reset the banner is printed, the scheduler starts, and
`help` / `status` on the console get a reply.

**Actual:** the banner was printed, then nothing. No reply to any command, no
LED activity. Normal build, `APP_FAULT_MODE = 0`.

**How it was found** (SEGGER J-Link Commander, because the MCU-Link probe has
J-Link firmware instead of the NXP firmware LinkServer needs):

1. Halt the core a few seconds after reset: `PC = 0x00000318`, which `nm`
   shows is `HardFault_Handler`, and `IPSR = 3` (HardFault). `PSP` holds no
   valid task stack (`0x04002F28`), so no task had run yet: the fault came
   before or during `vTaskStartScheduler()`.
2. Read the fault status registers (Cortex-M33, System Control Block):

   | Register | Address | Value | Meaning |
   |---|---|---|---|
   | CFSR | `0xE000ED28` | `0x00008200` | BusFault: `PRECISERR` (bit 9) + `BFARVALID` (bit 15) |
   | HFSR | `0xE000ED2C` | `0x40000000` | `FORCED`: the BusFault was escalated to a HardFault |
   | BFAR | `0xE000ED38` | `0x4000600C` | Faulting address |

3. `0x4000600C` = `CTIMER2_BASE` (`0x40006000`) + `0xC` = `CTIMER2->PR`.
   The only code that writes it is `vConfigureTimerForRunTimeStats()` in the
   course `runtime_stats.c`, which FreeRTOS calls from
   `vTaskStartScheduler()` through `portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()`.
4. Read the MRCC registers after the fault: CTIMER2 was never set up.

   | Register | Value | CTIMER2 part |
   |---|---|---|
   | `MRCC_GLB_RST0` | `0x60080008` | bit 4 = 0: CTIMER2 still in reset |
   | `MRCC_GLB_CC0` | `0x60480008` | bit 4 = 0: CTIMER2 bus clock off |
   | `MRCC_CTIMER2_CLKSEL` / `CLKDIV` | `0x7` / `0x40000000` | reset values, divider halted |

   `runtime_stats.c` does write these registers, so the writes were
   being ignored.

**Cause:** `clocks_and_resets()` in `bsp_mcxa153_regs.c` unlocked the clock
registers (`SYSCON->CLKUNLOCK`), set up CTIMER1, LPUART0 and the ports, and then
**locked them again** with `clk_lock()`. When `runtime_stats.c` ran later, its
MRCC writes for CTIMER2 were silently ignored. CTIMER2 stayed in reset with no
clock, so its first register access (`CTIMER2->PR = 1000-1`) gave a precise
BusFault. With no BusFault handler enabled, it was escalated to a HardFault.
Two board-layer parts that each worked on their own failed together.

**Fix:** removed the `clk_lock()` call, so the clock registers stay unlocked,
as they are after reset and as the course examples expect. The reason is
written as a comment next to `clk_unlock()` in `bsp_mcxa153_regs.c`.

**Check after the fix:** rebuilt (flash 26 320 B, RAM 18 360 B) and flashed.
`help` and `status` reply on the console, `feed` gives
`FED 1 portion(s) (command), today 1/6`, and a scheduled meal right after it
is `REJECTED schedule: cooldown`, as designed.

**Why the simulator missed it:** the PC simulator replaces the board layer
(`bsp.h` is the only seam), so register-level clock setup is never run
there. Faults in the board layer only appear on the real hardware.

*TODO(eni): what you learned from this in your own words, and, if you want,
reproduce it (put `clk_lock();` back at the end of `clocks_and_resets()`) and
take a screenshot of the fault registers in the debugger's peripheral/register
view.*
