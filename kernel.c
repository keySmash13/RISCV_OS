//==================================================
//                    KERNEL.C
//==================================================
#include "io.h"
#include "fs.h"
#include "cmd.h"
#include "libstr.h"

//==================================================
//               COMMAND PARSER / SHELL
//==================================================

// Parse input string and run appropriate command
void run_command(char *input) {
    while (*input == ' ') input++; // Skip leading spaces

    if (strncmp(input, "help", 4) == 0) {
        cmd_help();
    } else if (strncmp(input, "echo", 4) == 0) {
        char *args = input + 4;
        while (*args == ' ') args++;
        cmd_echo(args);
    } else if (strncmp(input, "mkdir", 5) == 0) {
        char *args = input + 5;
        while (*args == ' ') args++;
        fs_mkdir(args);
    } else if (strncmp(input, "touch", 5) == 0) {
        char *args = input + 5;
        while (*args == ' ') args++;
        fs_touch(args);
    } else if (strncmp(input, "ls", 2) == 0) {
        char *args = input + 2;
        while (*args == ' ') args++;
        fs_ls(args);
    } else if (strncmp(input, "cd", 2) == 0) {
        char *args = input + 2;
        while (*args == ' ') args++;
        fs_cd(args);
    } else if (strcmp(input, "pwd") == 0) {
        fs_pwd();
    } else if (strncmp(input, "write", 5) == 0 && (input[5] == ' ' || input[5] == '\0')) {
        char *args = input + 5;
        while (*args == ' ') args++;

        // Split into: path + text
        char *space = args;
        while (*space && *space != ' ') space++;
        char *text = "";
        if (*space) {
            *space++ = 0;        // terminate path
            while (*space == ' ') space++;
            text = space;
        }

        fs_write(args, text);
    } else if (strncmp(input, "cat", 3) == 0 && (input[3] == ' ' || input[3] == '\0')) {
        char *args = input + 3;
        while (*args == ' ') args++;
        fs_cat(args);
    }
    else if (*input != '\0') {
        uart_puts("Unknown command. Type 'help' for a list.\n");
    }
}



//==================================================
//                   KERNEL MAIN
//==================================================

void kmain(void) {
    uart_puts("Please look at this window for input/output!\n");
    uart_puts("tiny-rv64-kernel: ready!\n");

    fs_init();

    char buffer[100];
    for (;;) {
        uart_puts("> ");
        strin(buffer, 100);
        run_command(buffer);
    }
}
