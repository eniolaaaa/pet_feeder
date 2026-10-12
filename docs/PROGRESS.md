# Progress log

Hours spent on the flexible project (target: 80). *TODO(eni): the split per
category was proposed with AI help from my total; adjust a category where
I know it was different.*

## Hours per category

| Category | What it covers | Hours |
|----------|----------------|------:|
| Research | FreeRTOS book (Barry) and MIC5 workshops; study guide and assessment criteria; MCXA153 reference manual (clocks, CTIMER, GPIO, LPUART, NVIC); FRDM-MCXA153 board documentation | 10 |
| Environment setup | MCUXpresso for VS Code, SDK, Arm GCC toolchain, CMake/Ninja build; MCU-Link probe (LinkServer vs J-Link firmware); `freertos_hello` on the board; serial console; git and GitHub | 9 |
| Project design | tasks, priorities and scheduling choice; inter-task communication choices; ISRs; high-level design; PC simulator and expected timing diagrams | 13 |
| Implementation | the 7 tasks and RTOS objects; register-level board layer (clocks, servo PWM on CTIMER1, SW3 interrupt, LEDs, LPUART0 console); console commands; safety rules (cooldown, daily limit); fault injection; debounce fix | 20 |
| Configuration and measurement | `FreeRTOSConfig.h` (tick rate, priorities, heap, stack overflow check, debugger support); run-time statistics on CTIMER2; heap and stack measurements; RAM budget | 7 |
| Test and debugging | startup HardFault (fault registers, cause, fix); test scenarios 1–3 on the board with the RTOS-aware debugger; debounce bug found, fixed and re-tested; daily-limit test | 19 |
| Documentation | README (scheduling, inter-task communication, interrupts); design; test scenario write-ups; project plan; progress log | 12 |
| **Total** | | **90** |

## Timeline

From the git history and the work sessions:

| Date | What |
|------|------|
| *before 2026-10-06* | study of the FreeRTOS material, project choice, design and PC simulator |
| 2026-10-06 | project set up on the MIC5 course template: application code, register-level board layer, build scripts |
| 2026-10-07 | first run on the board; startup HardFault found with the debugger and fixed; probe set up for J-Link; console test of the daily limit |
| 2026-10-08 | project put under version control (git) |
| 2026-10-09 | GitHub repository; test scenarios 1–3 on the board with RTOS-aware debugger screenshots; debounce bug found in scenario 2, fixed and re-tested; project restructured for hand-in (RTOS sources in the repo, folders, README); `FreeRTOSConfig.h` documented; RAM budget measured |
| 2026-10-12 | project plan and progress log; daily-limit test conclusion |
| *TODO(eni)* | presentation |

Milestones: see [project_plan.md](project_plan.md), section 3.
