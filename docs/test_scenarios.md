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

**Actual:** breakpoint in `CommandTask` on `request_feed(FEED_COMMAND …)`,
then in `FeederTask` after `xQueueReceive`. At the first stop the Task List
shows Command *Running* and Feeder *Blocked* on FeedQ. At the second, Feeder
is *Running* and Command is *Ready*, because it was preempted. Queue List:
FeedQ 1/4 → 0/4.
*TODO(eni): screenshots + comparison.*

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
