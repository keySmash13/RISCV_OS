//==================================================
//                    KERNEL.C
//==================================================

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define UART0_BASE 0x10000000
#define UART_TX    0x00
#define UART_RX    0x00
#define UART_LSR   0x05  /* Line status register */
#define UART_LSR_DR 0x01 /* Data ready */

//--------------------------------------------------
//                     UART
//--------------------------------------------------

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
    while (!(*lsr & UART_LSR_DR))
        ; /* wait until data ready */
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
    if (node_count >= MAX_NODES) return 0;
    Node *n = &node_pool[node_count++];
    for (unsigned int i = 0; i < MAX_FILES; i++) n->children[i] = 0;
    n->child_count = 0;
    for (unsigned int i = 0; i < MAX_NAME; i++) n->name[i] = 0;
    for (unsigned int i = 0; i < 128; i++) n->content[i] = 0;
    n->parent = 0;
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
    return 0;
}

//--------------------------------------------------
//                 FILESYSTEM COMMANDS
//--------------------------------------------------

void fs_mkdir(const char *name) {
    if (cwd->child_count >= MAX_FILES) {
        uart_puts("Directory full!\n");
        return;
    }
    if (fs_find(cwd, name)) {
        uart_puts("Name already exists!\n");
        return;
    }
    Node *dir = fs_alloc_node();
    if (!dir) { uart_puts("Node limit reached!\n"); return; }
    dir->type = DIR_NODE;
    dir->parent = cwd;
    int j;
    for (j = 0; name[j] && j < MAX_NAME-1; j++) dir->name[j] = name[j];
    dir->name[j] = 0;
    cwd->children[cwd->child_count++] = dir;
}

void fs_touch(const char *name) {
    if (cwd->child_count >= MAX_FILES) {
        uart_puts("Directory full!\n");
        return;
    }
    if (fs_find(cwd, name)) {
        uart_puts("Name already exists!\n");
        return;
    }
    Node *file = fs_alloc_node();
    if (!file) { uart_puts("Node limit reached!\n"); return; }
    file->type = FILE_NODE;
    file->parent = cwd;
    int j;
    for (j = 0; name[j] && j < MAX_NAME-1; j++) file->name[j] = name[j];
    file->name[j] = 0;
    cwd->children[cwd->child_count++] = file;
}

void fs_ls(void) {
    for (unsigned int i = 0; i < cwd->child_count; i++) {
        Node *n = cwd->children[i];
        uart_puts(n->name);
        if (n->type == DIR_NODE) uart_puts("/");
        uart_puts("  ");
    }
    uart_puts("\n");
}

void fs_cd(const char *name) {
    if (strcmp(name, "..") == 0) {
        if (cwd->parent) cwd = cwd->parent;
        return;
    }
    Node *dir = fs_find(cwd, name);
    if (!dir || dir->type != DIR_NODE) {
        uart_puts("No such directory!\n");
        return;
    }
    cwd = dir;
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
}

void cmd_echo(char *args) {
    uart_puts(args);
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
    } else if (strcmp(input, "ls") == 0) {
        fs_ls();
    } else if (strncmp(input, "cd", 2) == 0) {
        char *args = input + 2;
        while (*args == ' ') args++;
        fs_cd(args);
    } else if (strcmp(input, "pwd") == 0) {
        fs_pwd();
    } else if (*input != '\0') {
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
