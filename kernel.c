//==================================================
//                    KERNEL.C
//==================================================
#include "io.h"
#include "fs.h"
#include "cmd.h"
#include "libstr.h"

//==================================================
//            INPUT VALIDATION HELPERS
//==================================================

// Validate path - check for dangerous patterns
static int validate_path(const char *path) {
    if (!path || *path == '\0') return 1; // Empty is OK for some commands
    
    // Check path length
    int len = 0;
    for (const char *p = path; *p; p++) len++;
    if (len > 63) {
        uart_puts("Error: Path too long (max 63 chars).\n");
        return 0;
    }
    
    return 1;
}

// Parse a permission number from string (0-7)
static int parse_perm(const char *str, unsigned int *out) {
    if (!str || *str == '\0') return 0;
    if (*str < '0' || *str > '7') return 0;
    if (*(str + 1) != '\0' && *(str + 1) != ' ') return 0; // Must be single digit
    *out = *str - '0';
    return 1;
}

//==================================================
//               COMMAND PARSER / SHELL
//==================================================

// Parse input string and run appropriate command
void run_command(char *input) {
    while (*input == ' ') input++; // Skip leading spaces

    if (strncmp(input, "help", 4) == 0 && (input[4] == '\0' || input[4] == ' ')) {
        cmd_help();
    } 
    else if (strncmp(input, "echo", 4) == 0 && (input[4] == '\0' || input[4] == ' ')) {
        char *args = input + 4;
        while (*args == ' ') args++;
        cmd_echo(args);
    } 
    else if (strncmp(input, "mkdir", 5) == 0 && (input[5] == '\0' || input[5] == ' ')) {
        char *args = input + 5;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: mkdir <dirname>\n");
            return;
        }
        fs_mkdir(args);
    } 
    else if (strncmp(input, "rmdir", 5) == 0 && (input[5] == '\0' || input[5] == ' ')) {
        char *args = input + 5;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        fs_rmdir(args);
    }
    else if (strncmp(input, "touchro", 7) == 0 && (input[7] == '\0' || input[7] == ' ')) {
        char *args = input + 7;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: touchro <filename>\n");
            return;
        }
        // Create read-only file (permission 4 = read only)
        fs_touch_with_perms(args, PERM_READ);
    } 
    else if (strncmp(input, "touch", 5) == 0 && (input[5] == '\0' || input[5] == ' ')) {
        char *args = input + 5;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: touch <filename>\n");
            return;
        }
        fs_touch(args);
    }
    else if (strncmp(input, "rm", 2) == 0 && (input[2] == '\0' || input[2] == ' ')) {
        // Make sure it's not "rmdir"
        if (strncmp(input, "rmdir", 5) == 0) {
            // Already handled above
        } else {
            char *args = input + 2;
            while (*args == ' ') args++;
            if (!validate_path(args)) return;
            fs_rm(args);
        }
    }
    else if (strncmp(input, "ls", 2) == 0 && (input[2] == '\0' || input[2] == ' ')) {
        char *args = input + 2;
        while (*args == ' ') args++;
        
        // Check for -a flag
        if (strncmp(args, "-a", 2) == 0 && (args[2] == '\0' || args[2] == ' ')) {
            args += 2;
            while (*args == ' ') args++;
            fs_ls_all(args);
        } else {
            fs_ls(args);
        }
    } 
    else if (strncmp(input, "cd", 2) == 0 && (input[2] == '\0' || input[2] == ' ')) {
        char *args = input + 2;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: cd <dirname>\n");
            return;
        }
        fs_cd(args);
    } 
    else if (strcmp(input, "pwd") == 0) {
        fs_pwd();
    } 
    else if (strncmp(input, "chmod", 5) == 0 && (input[5] == '\0' || input[5] == ' ')) {
        char *args = input + 5;
        while (*args == ' ') args++;

        // Split into: path + permission
        char *space = args;
        while (*space && *space != ' ') space++;
        
        if (*space) {
            *space++ = 0;        // terminate path
            while (*space == ' ') space++;
            
            unsigned int perms;
            if (!parse_perm(space, &perms)) {
                uart_puts("Invalid permission! Use 0-7.\n");
                return;
            }
            if (!validate_path(args)) return;
            fs_chmod(args, perms);
        } else {
            uart_puts("Usage: chmod <path> <perms>\n");
            uart_puts("  Perms: 0-7 (4=r, 2=w, 1=x)\n");
        }
    }
    else if (strncmp(input, "stat", 4) == 0 && (input[4] == '\0' || input[4] == ' ')) {
        char *args = input + 4;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        fs_stat(args);
    }
    else if (strncmp(input, "write", 5) == 0 && (input[5] == ' ' || input[5] == '\0')) {
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

        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: write <file> <text>\n");
            return;
        }
        fs_write(args, text);
    } 
    else if (strncmp(input, "cat", 3) == 0 && (input[3] == ' ' || input[3] == '\0')) {
        char *args = input + 3;
        while (*args == ' ') args++;
        if (!validate_path(args)) return;
        if (*args == '\0') {
            uart_puts("Usage: cat <filename>\n");
            return;
        }
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
