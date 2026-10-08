# Project plan — Flexible Project (draft)

> Draft structure that follows the "Checklist Assessment Flexible Project".
> Everything marked **TODO(eni)** is personal and must be written by you:
> the checklist asks for *your* motivation and *your* learning goals.

**Student / number:** TODO(eni)  **LT-coach:** TODO(eni)
**Chosen subject:** Automatic pet feeder with FreeRTOS on the NXP
FRDM-MCXA153: servo-driven hatch, feeding schedule, manual button, serial
commands and safety rules (cooldown, daily limit).

## Checklist: subject

| Criterion | How this project meets it |
|-----------|---------------------------|
| Intrinsic motivation | TODO(eni): why you want to do this, in your words |
| 80 hours | see schedule below; hours are tracked in `PROGRESS.md` as they are spent |
| Unique for the student | TODO(eni): what is new for you (first RTOS project on own hardware? combined with …) |
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

## 2. What I want to learn
TODO(eni). Suggestions that match the MIC5 learning goals:
- choose and justify a scheduling approach and task priorities;
- choose between queues, semaphores, mutexes, event groups and notifications;
- integrate FreeRTOS in MCUXpresso on a new MCU and size its memory;
- combine interrupts with an RTOS (deferred interrupt handling, NVIC priorities);
- use the RTOS-aware debugger to find a timing/communication error.

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
TODO(eni): LT-coach (approval, feedback), MIC5 teacher (RTOS questions),
classmates (review / test), anyone from the flexible project.

## 5. Global schedule
TODO(eni): fill in real dates and weeks. Planned effort per milestone (a plan,
not a record; actual hours go in `PROGRESS.md`):

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
high-level design, test scenarios. Plus TODO(eni: anything from your own goals).

## 7. Knowledge sharing / outside world (#OO)
TODO(eni): e.g. presentation at the presentation fair, publishing the
project (with simulator) on GitHub, a short write-up for classmates on
FreeRTOS on the MCXA153, demo to the LT-coach.
