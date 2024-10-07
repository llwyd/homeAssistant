# PICO Firmware

## Programming command
`sudo openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/rp2040.cfg -c "adapter speed 5000" -s tcl -c "program node.elf verify reset exit"`

## Minicom
`sudo minicom -b 115200 -D /dev/ttyACM0`

## Debug
`arm-none-eabi-gdb -ex "target remote localhost:3333" build/node.elf`
