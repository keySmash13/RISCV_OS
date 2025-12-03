//==================================================
//                    KERNEL.C
//==================================================

// Basic NULL definition for freestanding C programs
#define NULL ((void*)0)

// Fixed-size integer types (no C standard library available)
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

//--------------------------------------------------
//                BASIC STRING FUNCTIONS
//--------------------------------------------------

// Minimal strlen implementation
static unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (s[len]) len++;       // Count chars until null terminator
    return len;
}

// Minimal strcpy implementation
static void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++)) ; // Copy including '\0'
}

//--------------------------------------------------
//                     UART
//--------------------------------------------------

// UART MMIO register offsets and base address
#define UART0_BASE 0x10000000
#define UART_TX    0x00
#define UART_RX    0x00
#define UART_LSR   0x05         // Line Status Register offset
#define UART_LSR_DR 0x01        // Data Ready bit

// Output one byte to UART transmit register
static inline void uart_putc(char c) {
    volatile uint8_t *tx = (volatile uint8_t *)(UART0_BASE + UART_TX);
    *tx = c;
}

// Output a null-terminated string to UART
static inline void uart_puts(const char *s) {
    for (const char *p = s; *p; ++p) uart_putc(*p);
}

// Read one byte from UART receive register (blocking)
static inline char uart_getc(void) {
    volatile uint8_t *lsr = (volatile uint8_t *)(UART0_BASE + UART_LSR);
    volatile uint8_t *rx  = (volatile uint8_t *)(UART0_BASE + UART_RX);
    while (!(*lsr & UART_LSR_DR)) ; // Wait until data ready
    return *rx;
}

//--------------------------------------------------
//                 STRING UTILITIES
//--------------------------------------------------

// Compare full strings (like strcmp)
int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) a++, b++;
    return (unsigned char)*a - (unsigned char)*b;
}

// Compare up to n characters (like strncmp)
int strncmp(const char *a, const char *b, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0')
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

//--------------------------------------------------
//                     INPUT
//--------------------------------------------------

// Read a line from UART into dest with backspace support
void strin(char dest[], int len) {
    unsigned char chr;
    int i = 0;

    for (;;) {
        chr = uart_getc();

        switch(chr) {
            case '\r':
            case '\n':       // Enter pressed → finish input
                dest[i] = '\0';
                uart_puts("\r\n");
                return;

            case 0x7f:       // Backspace or delete
            case 0x08:
                if (i > 0) {
                    uart_puts("\b \b"); // Remove character visually
                    i--;
                }
                break;

            default:         // Printable character
                if (i < len - 1) {
                    dest[i++] = chr;
                    uart_putc(chr);  // Echo to screen
                }
        }
    }
}

//==================================================
//              FILESYSTEM DEFINITIONS
//==================================================

#define MAX_NAME 16
#define MAX_FILES 16
#define MAX_NODES 64

// Node can be a file or directory
typedef enum { FILE_NODE, DIR_NODE } NodeType;

// Basic filesystem node structure
typedef struct Node {
    char name[MAX_NAME];
    NodeType type;
    char content[128];          // File content area
    struct Node *children[MAX_FILES];
    struct Node *parent;
    unsigned int child_count;
    unsigned int readonly;   // 0 = writable, 1 = read-only
} Node;

//--------------------------------------------------
//            NODE POOL FOR ALLOCATION
//--------------------------------------------------

// Static pool, no malloc in freestanding kernel
Node node_pool[MAX_NODES];
unsigned int node_count = 0;

// Allocate a fresh node from pool
Node* fs_alloc_node(void) {
    if (node_count >= MAX_NODES) return NULL;

    Node *n = &node_pool[node_count++];

    // Init all fields
    for (unsigned int i = 0; i < MAX_FILES; i++) n->children[i] = NULL;
    n->child_count = 0;
    for (unsigned int i = 0; i < MAX_NAME; i++) n->name[i] = 0;
    for (unsigned int i = 0; i < 128; i++) n->content[i] = 0;
    n->parent = NULL;
    n->readonly = 0;  //default: writable

    return n;
}

//--------------------------------------------------
//                 FILESYSTEM CORE
//--------------------------------------------------

Node root;     // The root directory node
Node *cwd;     // Current working directory pointer

// Initialize filesystem: create root directory
void fs_init(void) {
    Node *r = fs_alloc_node();
    r->type = DIR_NODE;
    cwd = r;
    root = *r;     // Copy struct
    cwd = &root;   // Use root as actual object
}

// Search for a child node inside dir
Node *fs_find(Node *dir, const char *name) {
    for (unsigned int i = 0; i < dir->child_count; i++) {
        if (strcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    }
    return NULL;
}

//--------------------------------------------------
//          PATH TRAVERSAL HELPER
//--------------------------------------------------

// Walk through a path (supports /, ., ..)
// If create_missing = 1 → create directories while traversing
Node* fs_traverse_path(const char *path, int create_missing) {
    Node *current = cwd;

    // Absolute path → start at root
    if (*path == '/') current = &root;

    char temp[MAX_NAME];
    int i = 0;

    while (*path) {
        // When hitting slash or end of string → we reached a component
        if (*path == '/' || *(path+1) == '\0') {
            int len = i;

            // Add last character if string ends without slash
            if (*(path+1) == '\0' && *path != '/')
                temp[len++] = *path;

            temp[len] = 0;  // Null-terminate
            i = 0;          // Reset builder

            if (len == 0) { path++; continue; } // Ignore duplicate slashes

            // Handle "." → stay in same directory
            if (strcmp(temp, ".") == 0) {
                // no-op
            }
            // Handle ".." → go up one directory
            else if (strcmp(temp, "..") == 0) {
                if (current->parent)
                    current = current->parent;
            }
            // Normal directory name
            else {
                Node *child = fs_find(current, temp);

                if (!child) {
                    if (create_missing) {
                        // Auto-create directory
                        child = fs_alloc_node();
                        if (!child) {
                            uart_puts("Node limit reached!\n");
                            return NULL;
                        }
                        child->type = DIR_NODE;
                        child->parent = current;
                        strcpy(child->name, temp);
                        current->children[current->child_count++] = child;
                    } else {
                        uart_puts("No such directory in path!\n");
                        return NULL;
                    }
                }

                if (child->type != DIR_NODE) {
                    uart_puts("Path component is not a directory!\n");
                    return NULL;
                }

                current = child;
            }
        } else {
            // Build component name
            if (i < MAX_NAME-1) temp[i++] = *path;
        }

        path++;
    }

    return current;
}

//--------------------------------------------------
//                 FILESYSTEM COMMANDS
//--------------------------------------------------

// Create directory (mkdir)
void fs_mkdir(const char *path) {
    const char *last_slash = 0;

    // Find last slash to separate parent and name
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char new_dir_name[MAX_NAME];
    char parent_path[64];

    // Case: creating inside cwd
    if (!last_slash) {
        for (int j = 0; path[j] && j < MAX_NAME-1; j++)
            new_dir_name[j] = path[j];
        new_dir_name[strlen(path)] = 0;
        strcpy(parent_path, "");
    } else {
        // Split into "parent path" and "new directory name"
        int len = last_slash - path;
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            new_dir_name[j++] = *name_ptr++;
        new_dir_name[j] = 0;
    }

    // Parent directory lookup
    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    // Check existence + capacity
    if (fs_find(parent, new_dir_name)) {
        uart_puts("Name already exists!\n");
        return;
    }
    if (parent->child_count >= MAX_FILES) {
        uart_puts("Directory full!\n");
        return;
    }

    // Create new directory node
    Node *dir = fs_alloc_node();
    if (!dir) { uart_puts("Node limit reached!\n"); return; }

    dir->type = DIR_NODE;
    dir->parent = parent;
    strcpy(dir->name, new_dir_name);

    parent->children[parent->child_count++] = dir;
}

//==================================================
//             STRING TRIMMING UTILITY
//==================================================

// Trim leading and trailing spaces in-place
void trim(char *s) {
    // Trim leading spaces
    char *start = s;
    while (*start == ' ') start++;
    
    // Move the string to the start
    char *dst = s;
    while (*start) *dst++ = *start++;
    *dst = 0;

    // Trim trailing spaces
    if (dst == s) return; // empty string
    dst--;
    while (dst >= s && *dst == ' ') *dst-- = 0;
}

//==================================================
//             FILE CREATION FUNCTIONS
//==================================================


// Create empty file (touch)
void fs_touch(const char *path) {
    char clean_path[64];
    strcpy(clean_path, path);
    trim(clean_path);

    const char *last_slash = NULL;

    for (const char *p = clean_path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    // Case: simple filename
    if (!last_slash) {
        int j;
        for (j = 0; clean_path[j] && j < MAX_NAME-1; j++)
            file_name[j] = clean_path[j];
        file_name[j] = 0;
        strcpy(parent_path, "");
    } else {
        // Split parent path
        int len = last_slash - clean_path;
        for (int i = 0; i < len; i++)
            parent_path[i] = clean_path[i];
        parent_path[len] = 0;

        // Extract filename
        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    if (fs_find(parent, file_name)) {
        uart_puts("Name already exists!\n");
        return;
    }
    if (parent->child_count >= MAX_FILES) {
        uart_puts("Directory full!\n");
        return;
    }

    Node *file = fs_alloc_node();
    if (!file) { uart_puts("Node limit reached!\n"); return; }

    file->type = FILE_NODE;
    file->parent = parent;
    strcpy(file->name, file_name);
    file->readonly = 0; // normal file is writable

    parent->children[parent->child_count++] = file;
}


// Create a read-only file (touchro)
void fs_touch_ro(const char *path) {
    if (!path) return;

    // Skip leading spaces
    while (*path == ' ') path++;

    if (*path == '\0') {
        uart_puts("Error: No filename provided.\n");
        return;
    }

    const char *last_slash = NULL;

    // Find last slash to split parent/name
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    // Case: simple filename
    if (!last_slash) {
        int j = 0;
        while (path[j] && j < MAX_NAME-1) {
            file_name[j] = path[j];
            j++;
        }
        file_name[j] = 0;
        strcpy(parent_path, "");
    } else {
        // Split parent path
        int len = last_slash - path;
        for (int i = 0; i < len; i++)
            parent_path[i] = path[i];
        parent_path[len] = 0;

        // Extract filename
        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    if (fs_find(parent, file_name)) {
        uart_puts("Name already exists!\n");
        return;
    }
    if (parent->child_count >= MAX_FILES) {
        uart_puts("Directory full!\n");
        return;
    }

    // Create file node
    Node *file = fs_alloc_node();
    if (!file) { uart_puts("Node limit reached!\n"); return; }

    file->type = FILE_NODE;
    file->parent = parent;
    strcpy(file->name, file_name);
    file->readonly = 1; // read-only file

    parent->children[parent->child_count++] = file;
}




// List directory contents (ls)
void fs_ls(const char *path) {
    Node *dir;

    // No path → use cwd
    if (!path || *path == '\0') {
        dir = cwd;
    } else {
        dir = fs_traverse_path(path, 0);
        if (!dir) return;
    }

    for (unsigned int i = 0; i < dir->child_count; i++) {
        Node *n = dir->children[i];
        uart_puts(n->name);
        if (n->type == DIR_NODE) uart_puts("/");
        else if (n->readonly) uart_puts(" (ro)");
        uart_puts("  ");
    }
    uart_puts("\n");
}

// Change directory (cd)
void fs_cd(const char *path) {
    Node *target = fs_traverse_path(path, 0);
    if (target) cwd = target;
}

// Print working directory (pwd)
void fs_pwd_recursive(Node *n) {
    if (n->parent == 0) {
        uart_puts("/");
        return;
    }
    fs_pwd_recursive(n->parent);
    if (n != &root) {
        uart_putc('/');
        uart_puts(n->name);
    }
}

void fs_pwd(void) {
    fs_pwd_recursive(cwd);
    uart_puts("\n");
}

//==================================================
//                   PROGRAM COMMANDS
//==================================================

// Write text into file
void fs_write(const char *path, const char *text) {
    const char *last_slash = NULL;

    // Separate path into parent + filename
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    // Case: file is in cwd
    if (!last_slash) {
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++)
            file_name[j] = path[j];
        file_name[j] = 0;
        strcpy(parent_path, "");
    } else {
        int len = last_slash - path;
        for (int i = 0; i < len; i++)
            parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    // Find parent
    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    // File must exist
    Node *file = fs_find(parent, file_name);
    if (!file) { uart_puts("File does not exist!\n"); return; }
    if (file->type != FILE_NODE) { uart_puts("Not a file!\n"); return; }

    if (file->readonly) {
        uart_puts("File is read-only! Write aborted.\n");
        return;
    }

    // Write text to file->content
    int i;
    for (i = 0; i < 127 && text[i]; i++)
        file->content[i] = text[i];
    file->content[i] = 0;

    uart_puts("File written.\n");
}

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
    uart_puts("  touchro <name>    - Create a new read-only file\n");
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

// Print file content
void fs_cat(const char *path) {
    const char *last_slash = NULL;

    // Extract parent and file name
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++)
            file_name[j] = path[j];
        file_name[j] = 0;
        strcpy(parent_path, "");
    } else {
        int len = last_slash - path;
        for (int i = 0; i < len; i++)
            parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    Node *file = fs_find(parent, file_name);
    if (!file) { uart_puts("File does not exist!\n"); return; }
    if (file->type != FILE_NODE) { uart_puts("Not a file!\n"); return; }

    uart_puts(file->content);
    uart_puts("\n");
}

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
    } else if (strncmp(input, "touchro", 7) == 0) {
    // Move pointer past "touchro"
    char *args = input + 7;

    // Skip any leading spaces
    while (*args == ' ') args++;

    // Check if filename is empty
    if (*args == '\0') {
        uart_puts("Error: No filename provided.\n");
    } else {
        // Create the read-only file
        fs_touch_ro(args);
    }


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
