# FreeRTOS kernel V11.1.0

Unchanged copy of the FreeRTOS kernel from the MIC5 course SDK
(`SDK_24_12_00_FRDM-MCXA153_FOR_MIC5_V10/rtos/freertos/freertos-kernel`),
reduced to the files this project builds:

| Files | Why |
|-------|-----|
| `tasks.c`, `queue.c`, `list.c` | kernel core |
| `timers.c` | software timers (day rollover, heartbeat) |
| `event_groups.c` | `xAppEvents` (Feeder → Status) |
| `stream_buffer.c` | UART RX ISR → Command task |
| `croutine.c` | part of the standard kernel build, unused (`configUSE_CO_ROUTINES = 0`) |
| `include/` | kernel headers |
| `portable/GCC/ARM_CM33_NTZ/non_secure/` | Cortex-M33 port without TrustZone (the MCXA153 has no TrustZone) |
| `portable/MemMang/heap_4.c` | heap with free and coalescing, size `configTOTAL_HEAP_SIZE` |

The project's configuration is in `source/FreeRTOSConfig.h`. The list of
compiled files is in `armgcc/CMakeLists.txt`. License: MIT, see `LICENSE.md`.
