# Project plan — Flexible Project

> Structure follows the "Checklist Assessment Flexible Project".
> *TODO(eni): written from my answers with AI help; read it through and
> adjust where it doesn't sound like me.*

**Student number:** 2114890  **LT-coach:** Remko Welling
**Chosen subject:** Automatic pet feeder with FreeRTOS on the NXP
FRDM-MCXA153: servo-driven hatch, feeding schedule, manual button, serial
commands and safety rules (cooldown, daily limit).

## Checklist: subject

| Criterion | How this project meets it |
|-----------|---------------------------|
| Intrinsic motivation | A friend of mine has to arrange a house-sitter for their cat every time they travel, just so the cat gets fed. An automatic feeder that feeds on a schedule, and later also remotely, solves that. RTOS is something I need to learn for my course, and a device that has to keep exact timing (closing the hatch on time) while handling a button, a console and a schedule is a good reason to use one. |
| 80 hours | see the schedule below; actual hours are tracked in [`PROGRESS.md`](PROGRESS.md) |
| Unique for the student | My first RTOS project, and the first project I version-control myself with git and GitHub. |
| Related to field of study | Embedded systems / MIC5 RTOS learning goals 1–7 |
| Individual or small group | Individual |

## 1. What I am going to do
Design, build and test a FreeRTOS application for the FRDM-MCXA153 with at
least 5 tasks and 2 interrupt handlers that runs a pet feeder: a servo
dispenses portions on a schedule, on a button press or on a serial command,
without overfeeding. Deliverables: source code (incl. FreeRTOS sources),
documented `FreeRTOSConfig.h`, README with scheduling and inter-task
communication choices, high-level design, 3 test scenarios with RTOS-aware
debugger evidence, presentation.

After the MIC5 deliverables, an optional IoT extension (ESP32 + AWS IoT
Core) makes the travel use case complete: feeding and checking on the cat
from anywhere. It is planned separately, on its own git branch, so it doesn't
change the MIC5 hand-in.

## 2. What I want to learn
- **RTOS basics:** tasks, priorities and preemptive scheduling, and choosing
  between queues, semaphores, mutexes, event groups and notifications (MIC5
  goals 1, 2, 5).
- **Debugging with FreeRTOS:** using the RTOS-aware debugger to see task
  states and kernel objects, and to find errors that only show up when tasks
  interact (goal 7).
- **Interrupts with an RTOS:** deferred interrupt handling and NVIC
  priorities (goal 6).
- **Configuring an RTOS for a real system:** sizing the heap and the stacks
  from measurements instead of guesses (goals 3, 4).
- **Register-level programming of a new MCU:** clocks, timers for the servo
  PWM, GPIO interrupts and the UART on the MCXA153, from the reference manual.
- **Real hardware vs simulation:** things a simulator doesn't show, such as
  contact bounce, clock registers that silently ignore writes, and how a
  paused debugger affects timing.
- **Git and GitHub:** committing in small steps, writing useful commit
  messages, branches, and keeping secrets out of a repository.

## 3. Approach
Iterative, one milestone at a time, each ending with something that runs:

| # | Milestone | Result to show |
|---|-----------|----------------|
| M1 | Toolchain + `freertos_hello` running on the board | blinking LED, console output |
| M2 | Design: tasks, ISRs, communication | `docs/design.md` |
| M3 | Implementation of tasks + ISRs (simulator and board) | app running on board |
| M4 | Servo + feeder mechanics (hatch, portion size calibration) | working feeder |
| M5 | Configuration tuning (heap, stacks, priorities) | documented `FreeRTOSConfig.h` |
| M6 | Test scenarios with RTOS-aware debugger, incl. fault scenario | `docs/test_scenarios.md` |
| M7 | Presentation + hand-in | slides, Handin upload |

## 4. Persons / parties involved
| Who | Role |
|-----|------|
| Me | design, implementation, tests, documentation |
| Remko Welling | LT-coach: approval of the plan, feedback during the project |
| Hugo Arends | MIC5 teacher: RTOS questions, assessment of the MIC5 criteria |

## 5. Global schedule
Work on the board started on 2026-10-06; the test scenarios were done on
2026-10-09. *TODO(eni): hand-in deadline and presentation date, once known.*

Planned effort per milestone (a plan, not a record; actual hours go in
`PROGRESS.md`):

| Milestone | Planned hours |
|-----------|--------------:|
| M1 setup + study of FreeRTOS material | 10 |
| M2 design | 8 |
| M3 implementation | 20 |
| M4 servo + mechanics + calibration | 14 |
| M5 configuration + measurement | 6 |
| M6 tests + debugging | 14 |
| M7 presentation + documentation | 8 |
| **Total** | **80** |

## 6. What I want to be assessed on
The MIC5 assessment criteria: scheduling description, inter-task
communication description, source code, documented RTOS configuration,
high-level design, test scenarios. In addition I would like feedback on:
- the **project structure** (folders, separation of application, board layer
  and RTOS, documentation);
- my **debugging approach**: the startup HardFault and the debounce bug found
  on the board, and how I used the RTOS-aware debugger in the test
  scenarios.

## 7. Knowledge sharing / outside world (#OO)
- The project is on GitHub (private while it is being graded), with a README
  that explains the design choices. After grading I want to make it public
  as part of my portfolio.
- The IoT extension (ESP32 + AWS IoT Core) is planned as the next step, as
  proof of IoT experience for future job applications.
- *TODO(eni): the presentation (format and date not known yet).*
