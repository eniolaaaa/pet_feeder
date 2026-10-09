# CROSS COMPILER SETTING
set(CMAKE_SYSTEM_NAME Generic)
cmake_minimum_required(VERSION 3.10.0)

# THE VERSION NUMBER
SET (MCUXPRESSO_CMAKE_FORMAT_MAJOR_VERSION 2)
SET (MCUXPRESSO_CMAKE_FORMAT_MINOR_VERSION 0)

include(ide_overrides.cmake OPTIONAL)

if(CMAKE_SCRIPT_MODE_FILE)
  message("${MCUXPRESSO_CMAKE_FORMAT_MAJOR_VERSION}")
  return()
endif()


set(CMAKE_STATIC_LIBRARY_PREFIX)
set(CMAKE_STATIC_LIBRARY_SUFFIX)

set(CMAKE_EXECUTABLE_LIBRARY_PREFIX)
set(CMAKE_EXECUTABLE_LIBRARY_SUFFIX)

# CURRENT DIRECTORY
set(ProjDirPath ${CMAKE_CURRENT_SOURCE_DIR})

# Build output goes into the build directory, not the source tree
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})


project(pet_feeder)

enable_language(ASM)

set(MCUX_BUILD_TYPES debug release)

set(MCUX_SDK_PROJECT_NAME pet_feeder.elf)

# Location of the MIC5 MCUXpresso SDK. Override with -DSdkRootDirPath=... or
# the MCUX_SDK_ROOT environment variable if the SDK lives somewhere else.
if (NOT DEFINED SdkRootDirPath)
    if (DEFINED ENV{MCUX_SDK_ROOT})
        SET(SdkRootDirPath $ENV{MCUX_SDK_ROOT})
    else()
        SET(SdkRootDirPath "C:/Users/babaj/Downloads/SDK_24_12_00_FRDM-MCXA153_FOR_MIC5_V10")
    endif()
endif()
file(TO_CMAKE_PATH "${SdkRootDirPath}" SdkRootDirPath)
message(STATUS "SDK: ${SdkRootDirPath}")

include(${ProjDirPath}/flags.cmake)

include(${ProjDirPath}/config.cmake)

# Every .c file in source/ (application) and board/ (hardware layer) is
# compiled, so new modules only need to be dropped into one of those folders
# (re-run CMake configure, or .\build.ps1 -Clean, after adding a file).
file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    "${ProjDirPath}/../source/*.c"
    "${ProjDirPath}/../board/*.c"
)

# FreeRTOS kernel V11.1.0, copied from the course SDK into rtos/ so the project
# contains all RTOS sources. Cortex-M33 port without TrustZone (the MCXA153
# has no TrustZone), heap_4 for configTOTAL_HEAP_SIZE.
set(RTOS_DIR "${ProjDirPath}/../rtos/freertos-kernel")
set(RTOS_PORT_DIR "${RTOS_DIR}/portable/GCC/ARM_CM33_NTZ/non_secure")
set(RTOS_SOURCES
    ${RTOS_DIR}/croutine.c
    ${RTOS_DIR}/event_groups.c
    ${RTOS_DIR}/list.c
    ${RTOS_DIR}/queue.c
    ${RTOS_DIR}/stream_buffer.c
    ${RTOS_DIR}/tasks.c
    ${RTOS_DIR}/timers.c
    ${RTOS_PORT_DIR}/mpu_wrappers_v2_asm.c
    ${RTOS_PORT_DIR}/port.c
    ${RTOS_PORT_DIR}/portasm.c
    ${RTOS_DIR}/portable/MemMang/heap_4.c
)

add_executable(${MCUX_SDK_PROJECT_NAME} ${APP_SOURCES} ${RTOS_SOURCES})

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE
    "${ProjDirPath}/../source"
    "${ProjDirPath}/../board"
    "${RTOS_DIR}/include"
    "${RTOS_PORT_DIR}"
)

include(${SdkRootDirPath}/devices/MCXA153/all_lib_device.cmake)

IF(NOT DEFINED TARGET_LINK_SYSTEM_LIBRARIES)
    SET(TARGET_LINK_SYSTEM_LIBRARIES "-lm -lc -lgcc -lnosys")
ENDIF()

TARGET_LINK_LIBRARIES(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--start-group)

target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE ${TARGET_LINK_SYSTEM_LIBRARIES})

TARGET_LINK_LIBRARIES(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--end-group)

ADD_CUSTOM_COMMAND(TARGET ${MCUX_SDK_PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY}
-Obinary ${EXECUTABLE_OUTPUT_PATH}/${MCUX_SDK_PROJECT_NAME} ${EXECUTABLE_OUTPUT_PATH}/pet_feeder.bin)

set_target_properties(${MCUX_SDK_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES "output.map;${EXECUTABLE_OUTPUT_PATH}/pet_feeder.bin")

# wrap all libraries with -Wl,--start-group -Wl,--end-group to prevent link order issue
group_link_libraries()
