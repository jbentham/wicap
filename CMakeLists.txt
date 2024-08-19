# Pico WiCap project

cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)

project(wicap C CXX ASM)

pico_sdk_init()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set (FW_FILE firmware/fw_43439.c)

add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c picocap.c picocap.pio
    mg_wifi.c mongoose.c
    picowi/picowi_event.c picowi/picowi_init.c picowi/picowi_join.c
    picowi/picowi_pico.c picowi/picowi_pio.c picowi/picowi_wifi.c
    picowi/picowi_ioctl.c ${FW_FILE})

# Mongoose build flags
add_definitions(-DMG_ENABLE_TCPIP=1)
add_definitions(-DMG_ENABLE_PACKED_FS=0)
add_definitions(-DMG_ENABLE_MBEDTLS=0)
add_definitions(-DMG_ENABLE_CUSTOM_RANDOM=0)
add_definitions(-DMG_ENABLE_FILE=0)
add_definitions(-DMG_ARCH=MG_ARCH_RP2040)

target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_spi pico_rand hardware_pio 
    hardware_dma hardware_pwm hardware_flash)

pico_add_extra_outputs(${PROJECT_NAME})

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/picowi/picowi_pio.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/picocap.pio)

# Console output on UART1
add_definitions(-DPICO_DEFAULT_UART=1)

# Enable uart output, disable usb output
pico_enable_stdio_usb( ${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# EOF