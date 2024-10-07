#!/bin/sh

sudo openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/rp2040.cfg -c "adapter speed 5000" -s tcl -c "program build/node.elf verify reset exit"

