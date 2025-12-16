## By Abigail Holdcraft, Madison Byrd, and Katherine Turner

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

## Tiny RISC-V 64 Kernel

This is a minimal, educational RISC-V 64-bit kernel written in C, designed to run on bare-metal hardware or an emulator. The kernel provides basic I/O, a filesystem with Unix-like permissions, protection mechanisms, program execution, and an interactive shell.

### Features

- **UART I/O:** Minimal routines for sending and receiving characters over the serial port.  
- **String Utilities:** Lightweight implementations of `strlen`, `strcpy`, `strcmp`, and `strncmp`.  
- **Shell:** Interactive command-line interface via UART. Supports commands like:
  - `help` — show available commands
  - `echo <text>` — print text back
  - `exit` — shutdown the system
  - `mkdir <name>` — create a directory
  - `rmdir <name>` — delete an empty directory
  - `touch <name>` — create an empty file
  - `touchro <name>` — create a read-only file
  - `rm <file>` — delete a file
  - `ls` — list files and directories (shows permissions)
  - `ls -a` — list all files including hidden
  - `cd <name>` — change directory
  - `pwd` — print current path
  - `write <file> <text>` — write text to a file
  - `cat <file>` — display file contents
  - `chmod <path> <0-7>` — change file/directory permissions
  - `stat <path>` — show file/directory information
  - `exec <file>` — execute commands from a script file
- **Minimal Filesystem:**  
  - Supports directories and files with fixed-size names and content  
  - Keeps an in-memory node pool for fast allocation  
  - Path traversal and creation (`/` for root, `.` and `..` supported)
- **Unix-like Permission System:**
  - Read (r=4), Write (w=2), Execute (x=1) permissions
  - Permission checking on all file/directory operations
  - Examples: 7=rwx, 6=rw-, 5=r-x, 4=r--, 0=---
- **System Protection:**
  - Protected system directories (`/bin`, `/etc`) marked with `S` flag
  - System files cannot be deleted or modified
  - Initial structure: `/bin`, `/etc` (protected), `/home`, `/tmp` (user access)
- **Program Execution:**
  - Run script files with the `exec` command
  - Scripts contain shell commands separated by `;` or newlines
  - Lines starting with `#` are comments
  - Requires execute permission (`chmod file 5`)
  - Nested script execution supported (max depth: 4)
- **Main Loop:**  
  Continuously reads commands from UART, executes them, and prints results.

### Usage

1. Compile the kernel with a RISC-V cross-compiler.
2. Run it on QEMU or RISC-V hardware.
3. Interact with the shell via the serial console.

```c
void kmain(void) {
    fs_init();
    char buffer[100];
    for (;;) {
        uart_puts("> ");
        strin(buffer, 100);
        run_command(buffer);
    }
}
```

## boot.S

This is the minimal bootloader for the RISC-V 64 kernel:

- **Entry point `_start`**: sets up the stack pointer and jumps to the C kernel (`kmain`).  
- **Spin loop**: if `kmain` ever returns, the CPU waits indefinitely (`wfi`).  
- **Stack allocation**: reserves 8 KB of stack space in the `.bss` section with `_stack` and `_stack_top` symbols.

Serves as the initial setup before the kernel runs in a bare-metal environment.


## linker.ld

This is a minimal RISC-V 64 linker script for the kernel. Key points:

- Places the kernel at **0x80200000**, typical for simple RV64 examples.  
- Defines sections:  
  - `.text` for code (including `.text.boot`)  
  - `.rodata` for read-only data  
  - `.data` for initialized data  
  - `.bss` for zero-initialized data, with `__bss_start` and `__bss_end` markers  
- Small stack allocated immediately after BSS.  
- Discards `.eh_frame` (no exception handling).

Used to control memory layout and entry point for bare-metal execution.

## Other .c and .h files
Different command libraries
### cmd.c
Contains cmd line commands like `help` and `echo`
### fs.c
Controls the filesystem so one can make files, directories, cd, etc. Also handles Unix-like permissions (rwx), system file protection, and program execution support.
### fs.h
Defines filesystem structures, permission constants (`PERM_READ`, `PERM_WRITE`, `PERM_EXEC`), and flags (`FLAG_SYSTEM`, `FLAG_HIDDEN`).
### io.c
Controls terminal input/output
### kernel.c
Main kernel with command parser, shell loop, input validation, script execution engine, and SBI shutdown support.
### libstr.c
A small library of string commands to add string functionality to other files
### stdint.h
Small list of declarations for uint coding.
