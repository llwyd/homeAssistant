cmake_minimum_required(VERSION 3.13)
include(common/pico_sdk_import.cmake)

project(node_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug )
set(PICO_BOARD pico_w )

pico_sdk_init()

add_executable(node
    src/node_pico.c
    src/node_events.h
    src/environment.c
    src/environment.h
    src/wifi.c
    src/wifi.h
    src/emitter.c
    src/emitter.h
    src/comms.h
    src/comms.c
    ../../stateengine/src/state.h
    ../../stateengine/src/state.c
    ../../stateengine/src/fifo_base.h
    ../../stateengine/src/fifo_base.c
    ../../common/events.h
    ../../common/events.c
    ../../common/bme280.c
    ../../common/bme280.h
    ../../common/msg_fifo.c
    ../../common/msg_fifo.h
    ../../common/mqtt.c
    ../../common/mqtt.h
)

pico_enable_stdio_usb(node 1)
pico_enable_stdio_uart(node 1)
pico_add_extra_outputs(node)

target_include_directories(node PRIVATE ${CMAKE_CURRENT_LIST_DIR} )
include_directories( node PRIVATE src )
include_directories( node PRIVATE common )
include_directories( node PRIVATE ../../stateengine )
include_directories( node PRIVATE ../../common )
include_directories( node PRIVATE ../../stateengine/src )
target_link_libraries(node 
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_stdlib 
    hardware_i2c)
