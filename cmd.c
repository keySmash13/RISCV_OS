#include "stdint.h"
#include "fs.h"
#include "libstr.h"
#include "io.h"

//==================================================
//                   SHELL COMMANDS
//==================================================

// Print help menu
void cmd_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help             - Show this help message\n");
    uart_puts("  echo <text>      - Echo text back\n");
    uart_puts("  mkdir <name>     - Create a new directory\n");
    uart_puts("  touch <name>     - Create a new empty file\n");
    uart_puts("  ls               - List files and directories\n");
    uart_puts("  cd <name>        - Change directory\n");
    uart_puts("  pwd              - Show current directory path\n");
    uart_puts("  write            - Write text to an existing file\n");
    uart_puts("  cat              - Print text from a file\n");
}

// Echo text back to screen
void cmd_echo(char *args) {
    uart_puts(args);
    uart_puts("\n");
}