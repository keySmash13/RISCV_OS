## SETUP
Install QEMU: https://qemu.weilnetz.de/w64/  
run qemu-w64-setup-20250826.exe  
In WSL or Linux run:  
`sudo apt-get update`
`sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu binutils-riscv64-linux-gnu`

## HOW TO RUN
In WSL or Linux:
`make clean`
`make`
`./run.sh`

## How to set up kernel on USC lab computers - OUTDATED
First, you'll want to get qemu's risc-v 64-bit emulator so that you have the program qemu-system-riscv64. Then, run ./run.sh. 
This will spawn a window, but you can ignore it. I/O will happen via the terminal emulator you ran the script from.

## How to compile from source
You'll need a GCC toolchain for risc-v ELF binaries to run the makefile.

## Other things
This project is by Adrian Holdcraft, Madison Byrd, and
