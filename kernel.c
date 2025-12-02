//==================================================
//                    KERNEL.C
//==================================================

// freestanding definitions
#define NULL ((void*)0)

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

// minimal strlen
static unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (s[len]) len++;
    return len;
}

// minimal strcpy
static void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++)) ;
}


//--------------------------------------------------
//                     UART
//--------------------------------------------------

#define UART0_BASE 0x10000000
#define UART_TX    0x00
#define UART_RX    0x00
#define UART_LSR   0x05  /* Line status register */
#define UART_LSR_DR 0x01 /* Data ready */

static inline void uart_putc(char c) {
    volatile uint8_t *tx = (volatile uint8_t *)(UART0_BASE + UART_TX);
    *tx = c;
}

static inline void uart_puts(const char *s) {
    for (const char *p = s; *p; ++p) uart_putc(*p);
}

static inline char uart_getc(void) {
    volatile uint8_t *lsr = (volatile uint8_t *)(UART0_BASE + UART_LSR);
    volatile uint8_t *rx  = (volatile uint8_t *)(UART0_BASE + UART_RX);
    while (!(*lsr & UART_LSR_DR)) ; /* wait until data ready */
    return *rx;
}

//--------------------------------------------------
//                 STRING UTILITIES
//--------------------------------------------------

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) a++, b++;
    return (unsigned char)*a - (unsigned char)*b;
}

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

void strin(char dest[], int len) {
    unsigned char chr;
    int i = 0;

    for (;;) {
        chr = uart_getc();

        switch(chr) {
            case '\r':
            case '\n':
                dest[i] = '\0';
                uart_puts("\r\n");
                return;
            case 0x7f: // backspace
            case 0x08:
                if (i > 0) {
                    uart_puts("\b \b");
                    i--;
                }
                break;
            default:
                if (i < len - 1) {
                    dest[i++] = chr;
                    uart_putc(chr);
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

typedef enum { FILE_NODE, DIR_NODE } NodeType;

typedef struct Node {
    char name[MAX_NAME];
    NodeType type;
    char content[128];
    struct Node *children[MAX_FILES];
    struct Node *parent;
    unsigned int child_count;
} Node;

//--------------------------------------------------
//            NODE POOL FOR ALLOCATION
//--------------------------------------------------

Node node_pool[MAX_NODES];
unsigned int node_count = 0;

Node* fs_alloc_node(void) {
    if (node_count >= MAX_NODES) return NULL;
    Node *n = &node_pool[node_count++];
    for (unsigned int i = 0; i < MAX_FILES; i++) n->children[i] = NULL;
    n->child_count = 0;
    for (unsigned int i = 0; i < MAX_NAME; i++) n->name[i] = 0;
    for (unsigned int i = 0; i < 128; i++) n->content[i] = 0;
    n->parent = NULL;
    return n;
}

//--------------------------------------------------
//                 FILESYSTEM CORE
//--------------------------------------------------

Node root;
Node *cwd;

void fs_init(void) {
    Node *r = fs_alloc_node();
    r->type = DIR_NODE;
    cwd = r;
    root = *r;
    cwd = &root;
}

// find child by name
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

Node* fs_traverse_path(const char *path, int create_missing) {
    Node *current = cwd;

    // Absolute path starts from root
    if (*path == '/') current = &root;

    char temp[MAX_NAME];
    int i = 0;

    while (*path) {
        if (*path == '/' || *(path+1) == '\0') {
            int len = i;
            // include last char if not a slash
            if (*(path+1) == '\0' && *path != '/') temp[len++] = *path;
            temp[len] = 0;

            i = 0; // reset temp index

            if (len == 0) { 
                path++; 
                continue; // skip empty components "//"
            }

            // handle special components
            if (strcmp(temp, ".") == 0) {
                // do nothing, stay in current
            } else if (strcmp(temp, "..") == 0) {
                if (current->parent) current = current->parent;
            } else {
                Node *child = fs_find(current, temp);

                if (!child) {
                    if (create_missing) {
                        child = fs_alloc_node();
                        if (!child) { uart_puts("Node limit reached!\n"); return NULL; }
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
            if (i < MAX_NAME-1) temp[i++] = *path;
        }

        path++;
    }

    return current;
}



//--------------------------------------------------
//                 FILESYSTEM COMMANDS
//--------------------------------------------------

void fs_mkdir(const char *path) {
    const char *last_slash = 0;
    for (const char *p = path; *p; p++) if (*p == '/') last_slash = p;

    char new_dir_name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        // Single directory in cwd
        for (int j = 0; path[j] && j < MAX_NAME-1; j++) new_dir_name[j] = path[j];
        new_dir_name[strlen(path)] = 0;
        new_dir_name[strlen(path)] = 0;
        strcpy(parent_path, ""); // cwd
    } else {
        int len = last_slash - path;
        if (len >= 64) { uart_puts("Path too long!\n"); return; }
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1) new_dir_name[j++] = *name_ptr++;
        new_dir_name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    if (fs_find(parent, new_dir_name)) { uart_puts("Name already exists!\n"); return; }
    if (parent->child_count >= MAX_FILES) { uart_puts("Directory full!\n"); return; }

    Node *dir = fs_alloc_node();
    if (!dir) { uart_puts("Node limit reached!\n"); return; }
    dir->type = DIR_NODE;
    dir->parent = parent;

    for (int k = 0; new_dir_name[k] && k < MAX_NAME-1; k++) dir->name[k] = new_dir_name[k];
    dir->name[strlen(new_dir_name)] = 0;

    parent->children[parent->child_count++] = dir;
}

void fs_touch(const char *path) {
    const char *last_slash = NULL;
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        // No slashes → file in cwd
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++) file_name[j] = path[j];
        file_name[j] = 0;
        strcpy(parent_path, ""); // cwd
    } else {
        // Split path into parent directory and filename
        int len = last_slash - path;
        if (len >= 64) { uart_puts("Path too long!\n"); return; }
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1) file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    // Find parent directory
    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    if (fs_find(parent, file_name)) { uart_puts("Name already exists!\n"); return; }
    if (parent->child_count >= MAX_FILES) { uart_puts("Directory full!\n"); return; }

    Node *file = fs_alloc_node();
    if (!file) { uart_puts("Node limit reached!\n"); return; }
    file->type = FILE_NODE;
    file->parent = parent;

    for (int k = 0; file_name[k] && k < MAX_NAME-1; k++) file->name[k] = file_name[k];
    file->name[strlen(file_name)] = 0;

    parent->children[parent->child_count++] = file;
}


void fs_ls(const char *path) {
    Node *dir;
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
        uart_puts("  ");
    }
    uart_puts("\n");
}


void fs_cd(const char *path) {
    Node *target = fs_traverse_path(path, 0);
    if (target) cwd = target;
}

void fs_pwd_recursive(Node *n) {
    if (n->parent == 0) {
        uart_puts("/");
        return;
    }
    fs_pwd_recursive(n->parent);
    if (n != &root) { uart_putc('/'); uart_puts(n->name); }
}

void fs_pwd(void) {
    fs_pwd_recursive(cwd);
    uart_puts("\n");
}

//==================================================
//                   PROGRAM COMMANDS
//==================================================

void fs_write(const char *path, const char *text) {
    const char *last_slash = NULL;
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        // File in cwd
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++) file_name[j] = path[j];
        file_name[j] = 0;
        strcpy(parent_path, ""); // cwd
    } else {
        int len = last_slash - path;
        if (len >= 64) { uart_puts("Path too long!\n"); return; }
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1) file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    Node *file = fs_find(parent, file_name);
    if (!file) { uart_puts("File does not exist!\n"); return; }
    if (file->type != FILE_NODE) { uart_puts("Not a file!\n"); return; }

    // Copy text into content (truncate if necessary)
    int i;
    for (i = 0; i < 127 && text[i]; i++) file->content[i] = text[i];
    file->content[i] = 0;

    uart_puts("File written.\n");
}

//==================================================
//                   SHELL COMMANDS
//==================================================

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

void cmd_echo(char *args) {
    uart_puts(args);
    uart_puts("\n");
}

void fs_cat(const char *path) {
    const char *last_slash = NULL;
    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char file_name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        // File in cwd
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++) file_name[j] = path[j];
        file_name[j] = 0;
        strcpy(parent_path, ""); // cwd
    } else {
        // Split path into parent directory and filename
        int len = last_slash - path;
        if (len >= 64) { uart_puts("Path too long!\n"); return; }
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1) file_name[j++] = *name_ptr++;
        file_name[j] = 0;
    }

    // Find parent directory
    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return;

    Node *file = fs_find(parent, file_name);
    if (!file) { uart_puts("File does not exist!\n"); return; }
    if (file->type != FILE_NODE) { uart_puts("Not a file!\n"); return; }

    // Print file content
    uart_puts(file->content);
    uart_puts("\n");
}


void run_command(char *input) {
    while (*input == ' ') input++;

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

        // split path from text
        char *space = args;
        while (*space && *space != ' ') space++;
        char *text = "";
        if (*space) {
            *space++ = 0; // terminate path
            while (*space == ' ') space++; // skip spaces before text
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
