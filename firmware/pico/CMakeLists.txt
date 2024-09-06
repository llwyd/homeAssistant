cmake_minimum_required(VERSION 3.13)
include(common/pico_sdk_import.cmake)

project(node_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug )
set(PICO_BOARD pico_w )

pico_sdk_init()

add_custom_command(
    PRE_BUILD
    COMMAND ../../../scripts/generate_meta.sh ../../../common/
    OUTPUT ../../common/meta.h)

add_custom_target(meta ALL
    COMMAND ../../../scripts/generate_meta.sh ../../../common/
    DEPENDS ../../common/meta.h)

add_executable(node
    src/node_pico.c
    src/daemon.c
    src/daemon.h
    src/node_events.h
    src/environment.c
    src/environment.h
    src/wifi.c
    src/wifi.h
    src/emitter.c
    src/emitter.h
    src/comms.h
    src/comms.c
    src/i2c.c
    src/i2c.h
    src/accelerometer.h
    src/accelerometer.c
    src/udp.c
    src/udp.h
    src/ntp.c
    src/ntp.h
    src/gpio.h
    src/gpio.c
    src/alarm.h
    src/alarm.c
    src/config.h
    src/config.c
    src/eeprom.c
    src/eeprom.h
    src/cli.c
    src/cli.h
    src/watchdog.c
    src/watchdog.h
    ../../boilerplate/src/state.h
    ../../boilerplate/src/state.c
    ../../boilerplate/src/fifo_base.h
    ../../boilerplate/src/fifo_base.c
    ../../common/events.h
    ../../common/events.c
    ../../common/bme280.c
    ../../common/bme280.h
    ../../common/MCP9808.h
    ../../common/MCP9808.c
    ../../common/msg_fifo.c
    ../../common/msg_fifo.h
    ../../common/mqtt.c
    ../../common/mqtt.h
    ../../common/meta.h
)

set_source_files_properties(
    src/node_pico.c
    src/daemon.c
    src/daemon.h
    src/node_events.h
    src/environment.c
    src/environment.h
    src/wifi.c
    src/wifi.h
    src/emitter.c
    src/emitter.h
    src/comms.h
    src/comms.c
    src/i2c.c
    src/i2c.h
    src/accelerometer.h
    src/accelerometer.c
    src/udp.c
    src/udp.h
    src/ntp.c
    src/ntp.h
    src/gpio.h
    src/gpio.c
    src/alarm.h
    src/alarm.c
    src/config.h
    src/config.c
    src/eeprom.c
    src/eeprom.h
    src/cli.c
    src/cli.h
    ../../boilerplate/src/state.h
    ../../boilerplate/src/state.c
    ../../boilerplate/src/fifo_base.h
    ../../boilerplate/src/fifo_base.c
    ../../common/events.h
    ../../common/events.c
    ../../common/bme280.c
    ../../common/bme280.h
    ../../common/MCP9808.h
    ../../common/MCP9808.c
    ../../common/msg_fifo.c
    ../../common/msg_fifo.h
    ../../common/mqtt.c
    ../../common/mqtt.h
    PROPERTIES
    COMPILE_FLAGS "-Wall -Werror -Wfatal-errors -Wextra -Wpointer-arith"
)

pico_enable_stdio_usb(node 1)
pico_enable_stdio_uart(node 1)
pico_add_extra_outputs(node)

#target_compile_definitions(node PRIVATE SENSOR_MCP9808 )
target_compile_definitions(node PRIVATE ACCELEROMETER_ENABLED )

target_compile_options(node PRIVATE -Wfatal-errors
                        -Wall
                        -Wextra
                        #-Werror
                        -Wpointer-arith)

target_include_directories(node PRIVATE ${CMAKE_CURRENT_LIST_DIR} )
include_directories( node PRIVATE src )
include_directories( node PRIVATE common )
include_directories( node PRIVATE ../../boilerplate )
include_directories( node PRIVATE ../../common )
include_directories( node PRIVATE ../../boilerplate/src )
target_link_libraries(node 
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_stdlib 
    hardware_i2c
    hardware_rtc)
