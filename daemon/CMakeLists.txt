cmake_minimum_required(VERSION 3.2)

project( daemon )

set (CMAKE_C_STANDARD 11 )
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/ )
set (CMAKE_BUILD_TYPE Debug )

include_directories( daemon.out PRIVATE src )
include_directories( daemon.out PRIVATE ../common )
include_directories( daemon.out PRIVATE ../boilerplate/src )

add_custom_command(
    PRE_BUILD
    COMMAND ../scripts/generate_meta.sh ../common/
    OUTPUT ../common/meta.h)

add_custom_target(meta ALL
    COMMAND ../scripts/generate_meta.sh ../common/
    DEPENDS ../common/meta.h)

add_executable( outside.out
                src/sensor.c
                src/sensor.h
                ../common/mqtt.h
                ../common/mqtt.c
                ../common/msg_fifo.h
                ../common/msg_fifo.c
                src/outside.c
                src/timestamp.h
                src/timestamp.c
                src/comms.h
                src/comms.c
                ../common/events.h
                ../common/events.c
                src/timer.h
                src/timer.c
                ../boilerplate/src/fifo_base.h
                ../boilerplate/src/fifo_base.c
                ../boilerplate/src/state.h
                ../boilerplate/src/state.c
                ../common/meta.h
                )

add_executable( daemon.out
                src/sensor.c
                src/sensor.h
                ../common/mqtt.h
                ../common/mqtt.c
                ../common/msg_fifo.h
                ../common/msg_fifo.c
                src/daemon.c
                src/timestamp.h
                src/timestamp.c
                src/comms.h
                src/comms.c
                ../common/events.h
                ../common/events.c
                src/timer.h
                src/timer.c
                ../boilerplate/src/fifo_base.h
                ../boilerplate/src/fifo_base.c
                ../boilerplate/src/state.h
                ../boilerplate/src/state.c
                ../common/meta.h
                )

target_compile_options( daemon.out
                        PUBLIC
                        -Wfatal-errors
                        -Wall
                        -Wextra
                        -Werror
                        -Wpedantic
                        -Wpointer-arith
                        -g)

