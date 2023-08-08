cmake_minimum_required(VERSION 3.2)

project( daemon )

set (CMAKE_C_STANDARD 11 )
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/ )
set (CMAKE_BUILD_TYPE Debug )

include_directories( daemon.out PRIVATE src )
include_directories( daemon.out PRIVATE common )
include_directories( daemon.out PRIVATE ../common )
include_directories( daemon.out PRIVATE ../stateengine/src )

add_executable( daemon.out
                src/mqtt.c
                src/mqtt.h
                src/sensor.c
                src/sensor.h
                src/daemon.c
                src/timestamp.h
                src/timestamp.c
                ../common/events.h
                ../common/events.c
                src/timer.h
                src/timer.c
                ../stateengine/src/fifo_base.h
                ../stateengine/src/fifo_base.c
                ../stateengine/src/state.h
                ../stateengine/src/state.c)

target_compile_options( daemon.out
                        PUBLIC
                        -Wfatal-errors
                        -Wall
                        -Wextra
                        -Werror
                        -Wpedantic
                        -Wpointer-arith
                        -g)

