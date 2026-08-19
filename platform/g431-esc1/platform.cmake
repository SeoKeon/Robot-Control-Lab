# ---------------------------------------------------------------------------
# platform/g431-esc1 — 보드 공용 계층
#
# 대상 보드 : B-G431B-ESC1 (STM32G431CB)
# Cube FW   : STM32Cube FW_G4 V1.6.3
#
# 실험마다 동일한 것들(HAL/CMSIS, startup, 링커스크립트, CubeMX 생성 Core,
# BSP)은 전부 여기 한 벌만 존재한다. 실험 프로젝트는 이 파일을 include하고
# add_lab_firmware() 를 호출한다.
#
# 규칙: CubeMX는 이 폴더의 g431-esc1.ioc 로만 돌린다.
#       실험 프로젝트 폴더에서 CubeMX를 새로 돌리지 않는다.
# ---------------------------------------------------------------------------

if(DEFINED LAB_PLATFORM_DIR)
    return()
endif()

set(LAB_PLATFORM_DIR   ${CMAKE_CURRENT_LIST_DIR})
set(LAB_PLATFORM_BOARD "B-G431B-ESC1")
set(LAB_PLATFORM_MCU   "STM32G431CB")
set(LAB_CUBE_FW        "STM32Cube FW_G4 V1.6.3")

enable_language(C ASM)

# --- 보드 공용 매크로 -------------------------------------------------------
set(LAB_PLATFORM_DEFINES
    USE_HAL_DRIVER
    STM32G431xx
    $<$<CONFIG:Debug>:DEBUG>
)

# --- 보드 공용 include 경로 -------------------------------------------------
set(LAB_PLATFORM_INCLUDE_DIRS
    ${LAB_PLATFORM_DIR}/Core/Inc
    ${LAB_PLATFORM_DIR}/bsp/Inc
    ${LAB_PLATFORM_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc
    ${LAB_PLATFORM_DIR}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    ${LAB_PLATFORM_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx/Include
    ${LAB_PLATFORM_DIR}/Drivers/CMSIS/Include
)

# --- 보드 공용 소스 ---------------------------------------------------------
# HAL 소스는 glob으로 잡는다. CubeMX에서 주변장치(ADC/TIM/…)를 새로 켜면
# Drivers/…/Src 에 파일이 추가되는데, 목록을 손으로 고칠 필요가 없다.
file(GLOB LAB_HAL_SOURCES CONFIGURE_DEPENDS
    ${LAB_PLATFORM_DIR}/Drivers/STM32G4xx_HAL_Driver/Src/*.c
)
# CubeMX가 만드는 _template.c 는 빌드 대상이 아니다
list(FILTER LAB_HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

file(GLOB LAB_BSP_SOURCES CONFIGURE_DEPENDS ${LAB_PLATFORM_DIR}/bsp/Src/*.c)

set(LAB_PLATFORM_SOURCES
    ${LAB_PLATFORM_DIR}/Core/Src/main.c
    ${LAB_PLATFORM_DIR}/Core/Src/stm32g4xx_it.c
    ${LAB_PLATFORM_DIR}/Core/Src/stm32g4xx_hal_msp.c
    ${LAB_PLATFORM_DIR}/Core/Src/system_stm32g4xx.c
    ${LAB_PLATFORM_DIR}/Core/Src/sysmem.c
    ${LAB_PLATFORM_DIR}/Core/Src/syscalls.c
    ${LAB_PLATFORM_DIR}/startup_stm32g431xx.s
    ${LAB_BSP_SOURCES}
    ${LAB_HAL_SOURCES}
)

# ---------------------------------------------------------------------------
# add_lab_firmware(<타깃> [SOURCES ...] [INCLUDE_DIRS ...] [DEFINES ...] [LIBS ...])
#
# 공용 계층 + 실험 코드를 묶어 .elf 를 만든다. .hex/.bin 과 크기 출력까지.
#
#   add_lab_firmware(blink_test SOURCES app_main.c)
# ---------------------------------------------------------------------------
function(add_lab_firmware TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDE_DIRS;DEFINES;LIBS" ${ARGN})

    add_executable(${TARGET}
        ${LAB_PLATFORM_SOURCES}
        ${ARG_SOURCES}
        ${ARG_UNPARSED_ARGUMENTS}
    )

    target_include_directories(${TARGET} PRIVATE
        ${LAB_PLATFORM_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARG_INCLUDE_DIRS}
    )
    target_compile_definitions(${TARGET} PRIVATE
        ${LAB_PLATFORM_DEFINES}
        ${ARG_DEFINES}
    )
    target_link_libraries(${TARGET} PRIVATE m ${ARG_LIBS})

    # 타깃별 .map — 한 프로젝트에 실험이 여러 개 있어도 이름이 겹치지 않는다
    target_link_options(${TARGET} PRIVATE -Wl,-Map=${TARGET}.map)
    set_target_properties(${TARGET} PROPERTIES
        SUFFIX ".elf"
        ADDITIONAL_CLEAN_FILES "${TARGET}.map;${TARGET}.hex;${TARGET}.bin"
    )

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
        COMMENT "${TARGET}: hex/bin 생성 및 크기 출력"
    )
endfunction()

message(STATUS "lab platform: ${LAB_PLATFORM_BOARD} / ${LAB_PLATFORM_MCU} / ${LAB_CUBE_FW}")
