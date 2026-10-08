# Pet feeder (FreeRTOS on FRDM-MCXA153)

The pet feeder application from the design thread (`app_tasks.c`,
`app_config.h`, `bsp.h`, register-level board file `bsp_mcxa153_regs.c`),
built on the MIC5 course project setup (course `FreeRTOSConfig.h` and
`runtime_stats.c`). Design, test scenarios and plan are in `docs/`.

The first starter version (LED/button/servo demo with the course `rgb.c`,
`sw.c`, `serial.c`) is kept in `starter_backup/`. That folder is not compiled.

## Hardware

| What | Where |
|------|-------|
| Servo signal | P1_4, header J1-2 (CTIMER1 match 2, 50 Hz PWM) |
| Servo + / GND | 5 V / GND (use a separate 5 V supply for bigger servos, shared GND) |
| Feed button | SW3 (P1_7) |
| LEDs | on-board RGB |
| Console | MCU-Link VCOM, 115200 8N1, type `help` |

## Changes made to the course files

`FreeRTOSConfig.h` (course version) was changed to what the application needs:
`configMAX_PRIORITIES` 6, `configTOTAL_HEAP_SIZE` 14 KB,
`configTIMER_TASK_PRIORITY` 2, `configCHECK_FOR_STACK_OVERFLOW` 2,
`INCLUDE_xTaskGetHandle` 1. The timer task stack stays at
`configMINIMAL_STACK_SIZE` (256 words), which is more than the design needs.

`bsp_mcxa153_regs.c` sets `SystemCoreClock` to 48 MHz (the course SDK leaves it
at 12 MHz) and enables `CTIMERGLOBALSTARTEN[CTIMER1_CLK_EN]`, like the course
`runtime_stats.c` does for CTIMER2.

## Build

Uses the Arm GNU toolchain and Ninja from `%USERPROFILE%\.mcuxpressotools`,
LinkServer from `C:\NXP`, CMake, and the MIC5 SDK in
`Downloads\SDK_24_12_00_FRDM-MCXA153_FOR_MIC5_V10` (set `MCUX_SDK_ROOT` if it
moves).

```
.\build.ps1            # debug build -> build\debug\pet_feeder.elf
.\build.ps1 -Flash     # build and flash with LinkServer
.\build.ps1 -Release   # release build
.\build.ps1 -Clean     # rebuild from scratch
```

In VS Code: Ctrl+Shift+B builds; "Run Task > Build and flash" flashes.
For debugging with the FreeRTOS task view, import the folder with the
MCUXpresso for VS Code extension (Import Project, pick `armgcc`).

Every `.c` file in this folder is compiled automatically.
