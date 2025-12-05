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
    uart_puts("  help              - Show this help message\n");
    uart_puts("  echo <text>       - Echo text back\n");
    uart_puts("\n--- File Operations ---\n");
    uart_puts("  touch <name>      - Create file (default: rw permissions)\n");
    uart_puts("  touchro <name>    - Create read-only file\n");
    uart_puts("  cat <file>        - Print file contents\n");
    uart_puts("  write <file> <txt>- Write text to file\n");
    uart_puts("  rm <file>         - Delete a file\n");
    uart_puts("\n--- Directory Operations ---\n");
    uart_puts("  mkdir <name>      - Create directory\n");
    uart_puts("  rmdir <name>      - Delete empty directory\n");
    uart_puts("  ls                - List files (shows permissions)\n");
    uart_puts("  ls -a             - List all files (incl. hidden)\n");
    uart_puts("  cd <name>         - Change directory\n");
    uart_puts("  pwd               - Print working directory\n");
    uart_puts("\n--- Protection & Permissions ---\n");
    uart_puts("  chmod <path> <n>  - Change permissions (0-7)\n");
    uart_puts("  stat <path>       - Show file/dir info\n");
    uart_puts("\nPermission values: 4=read, 2=write, 1=execute\n");
    uart_puts("  Examples: 7=rwx, 6=rw-, 5=r-x, 4=r--, 0=---\n");
    uart_puts("System files (S flag) cannot be deleted or modified.\n");
}

// Echo text back to screen
void cmd_echo(char *args) {
    uart_puts(args);
    uart_puts("\n");
}