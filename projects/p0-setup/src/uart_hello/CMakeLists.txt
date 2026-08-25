cmake_minimum_required(VERSION 3.22)

# --- @설명@ ---
# 보드 공용 자산(HAL/CMSIS, startup, 링커스크립트, CubeMX Core, BSP)은
# platform/g431-esc1 에 한 벌만 있다. 여기엔 실험 코드만 둔다.

# platform/ 이 있는 레포 루트를 위로 올라가며 찾는다 (상대경로를 손으로 세지 않기 위해)
set(_d ${CMAKE_CURRENT_SOURCE_DIR})
while(NOT EXISTS ${_d}/platform/g431-esc1/platform.cmake)
    get_filename_component(_p ${_d} DIRECTORY)
    if(_p STREQUAL _d)
        message(FATAL_ERROR "레포 루트(platform/ 상위)를 찾지 못했습니다")
    endif()
    set(_d ${_p})
endwhile()
set(LAB_ROOT ${_d})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(uart_hello C ASM)
include(${LAB_ROOT}/platform/g431-esc1/platform.cmake)

add_lab_firmware(uart_hello SOURCES app_main.c)
