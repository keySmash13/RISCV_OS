#include "stdint.h"
#include "io.h"
#include "libstr.h"
#include "fs.h"

#define NULL ((void*)0)


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
    n->permissions = PERM_RW;  // Default: read + write
    n->flags = 0;              // No special flags

    return n;
}

//--------------------------------------------------
//          PERMISSION CHECKING HELPERS
//--------------------------------------------------

int fs_can_read(Node *node) {
    return (node->permissions & PERM_READ) != 0;
}

int fs_can_write(Node *node) {
    return (node->permissions & PERM_WRITE) != 0;
}

int fs_can_exec(Node *node) {
    return (node->permissions & PERM_EXEC) != 0;
}

int fs_is_system(Node *node) {
    return (node->flags & FLAG_SYSTEM) != 0;
}

int fs_is_hidden(Node *node) {
    return (node->flags & FLAG_HIDDEN) != 0;
}

//--------------------------------------------------
//                 FILESYSTEM CORE
//--------------------------------------------------

Node root;     // The root directory node
Node *cwd;     // Current working directory pointer

// Initialize filesystem: create root directory and system directories
void fs_init(void) {
    Node *r = fs_alloc_node();
    r->type = DIR_NODE;
    r->permissions = PERM_RWX;      // Full access to root
    r->flags = FLAG_SYSTEM;         // Root is protected
    cwd = r;
    root = *r;     // Copy struct
    cwd = &root;   // Use root as actual object

    // Create protected system directories
    // /bin - system binaries (protected)
    Node *bin = fs_alloc_node();
    bin->type = DIR_NODE;
    bin->permissions = PERM_RX;     // Read + execute only
    bin->flags = FLAG_SYSTEM;       // Cannot be deleted
    bin->parent = &root;
    strcpy(bin->name, "bin");
    root.children[root.child_count++] = bin;

    // /etc - system configuration (protected)
    Node *etc = fs_alloc_node();
    etc->type = DIR_NODE;
    etc->permissions = PERM_RX;     // Read + execute only
    etc->flags = FLAG_SYSTEM;       // Cannot be deleted
    etc->parent = &root;
    strcpy(etc->name, "etc");
    root.children[root.child_count++] = etc;

    // /home - user directory (full access)
    Node *home = fs_alloc_node();
    home->type = DIR_NODE;
    home->permissions = PERM_RWX;   // Full access
    home->flags = 0;                // Not system protected
    home->parent = &root;
    strcpy(home->name, "home");
    root.children[root.child_count++] = home;

    // /tmp - temporary files (full access)
    Node *tmp = fs_alloc_node();
    tmp->type = DIR_NODE;
    tmp->permissions = PERM_RWX;    // Full access
    tmp->flags = 0;                 // Not system protected
    tmp->parent = &root;
    strcpy(tmp->name, "tmp");
    root.children[root.child_count++] = tmp;

    // Create a sample protected config file in /etc
    Node *passwd = fs_alloc_node();
    passwd->type = FILE_NODE;
    passwd->permissions = PERM_READ; // Read-only
    passwd->flags = FLAG_SYSTEM;     // Cannot be deleted
    passwd->parent = etc;
    strcpy(passwd->name, "passwd");
    strcpy(passwd->content, "root:x:0:0:root:/root:/bin/sh");
    etc->children[etc->child_count++] = passwd;
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

    // PROTECTION: Check write permission on parent directory
    if (!fs_can_write(parent)) {
        uart_puts("Permission denied: cannot write to this directory.\n");
        return;
    }

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
    dir->permissions = PERM_RWX;  // Default: full permissions for new directories
    dir->flags = 0;
    strcpy(dir->name, new_dir_name);

    parent->children[parent->child_count++] = dir;
}

// Internal helper to create a file with specific permissions
static void fs_touch_internal(const char *path, unsigned int perms) {
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
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++)
            file_name[j] = path[j];
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

    // PROTECTION: Check write permission on parent directory
    if (!fs_can_write(parent)) {
        uart_puts("Permission denied: cannot write to this directory.\n");
        return;
    }

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
    file->permissions = perms;
    file->flags = 0;
    strcpy(file->name, file_name);

    parent->children[parent->child_count++] = file;
}

// Create empty file with default permissions (touch)
void fs_touch(const char *path) {
    fs_touch_internal(path, PERM_RW);  // Default: read + write
}

// Create file with specific permissions
void fs_touch_with_perms(const char *path, unsigned int perms) {
    fs_touch_internal(path, perms);
}

// Internal ls helper
static void fs_ls_internal(const char *path, int show_hidden) {
    Node *dir;

    // No path → use cwd
    if (!path || *path == '\0') {
        dir = cwd;
    } else {
        dir = fs_traverse_path(path, 0);
        if (!dir) return;
    }

    // PROTECTION: Check read permission on directory
    if (!fs_can_read(dir)) {
        uart_puts("Permission denied: cannot read this directory.\n");
        return;
    }

    for (unsigned int i = 0; i < dir->child_count; i++) {
        Node *n = dir->children[i];
        
        // Skip hidden files unless show_hidden is true
        if (fs_is_hidden(n) && !show_hidden) continue;

        // Show permission indicators
        uart_putc(fs_can_read(n) ? 'r' : '-');
        uart_putc(fs_can_write(n) ? 'w' : '-');
        uart_putc(fs_can_exec(n) ? 'x' : '-');
        uart_putc(' ');
        
        // Show system/hidden flags
        if (fs_is_system(n)) uart_putc('S');
        else if (fs_is_hidden(n)) uart_putc('H');
        else uart_putc(' ');
        uart_putc(' ');

        uart_puts(n->name);
        if (n->type == DIR_NODE) uart_puts("/");
        uart_puts("\n");
    }
}

// List directory contents (ls) - hides hidden files
void fs_ls(const char *path) {
    fs_ls_internal(path, 0);
}

// List all files including hidden (ls -a)
void fs_ls_all(const char *path) {
    fs_ls_internal(path, 1);
}

// Change directory (cd)
void fs_cd(const char *path) {
    Node *target = fs_traverse_path(path, 0);
    if (target) {
        // PROTECTION: Check execute permission (required to enter directory)
        if (!fs_can_exec(target)) {
            uart_puts("Permission denied: cannot enter this directory.\n");
            return;
        }
        cwd = target;
    }
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

    // PROTECTION: Check write permission
    if (!fs_can_write(file)) {
        uart_puts("Permission denied: file is not writable.\n");
        return;
    }

    // Write text to file->content
    int i;
    for (i = 0; i < 127 && text[i]; i++)
        file->content[i] = text[i];
    file->content[i] = 0;

    uart_puts("File written.\n");
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

    // PROTECTION: Check read permission
    if (!fs_can_read(file)) {
        uart_puts("Permission denied: file is not readable.\n");
        return;
    }

    uart_puts(file->content);
    uart_puts("\n");
}

//==================================================
//           DELETE OPERATIONS (NEW)
//==================================================

// Helper to find a node and its parent
static Node* fs_find_with_parent(const char *path, Node **out_parent) {
    const char *last_slash = NULL;

    for (const char *p = path; *p; p++)
        if (*p == '/') last_slash = p;

    char name[MAX_NAME];
    char parent_path[64];

    if (!last_slash) {
        int j;
        for (j = 0; path[j] && j < MAX_NAME-1; j++)
            name[j] = path[j];
        name[j] = 0;
        strcpy(parent_path, "");
    } else {
        int len = last_slash - path;
        for (int i = 0; i < len; i++)
            parent_path[i] = path[i];
        parent_path[len] = 0;

        int j = 0;
        const char *name_ptr = last_slash + 1;
        while (*name_ptr && j < MAX_NAME-1)
            name[j++] = *name_ptr++;
        name[j] = 0;
    }

    Node *parent = (*parent_path) ? fs_traverse_path(parent_path, 0) : cwd;
    if (!parent) return NULL;

    *out_parent = parent;
    return fs_find(parent, name);
}

// Remove a file (rm)
void fs_rm(const char *path) {
    if (!path || *path == '\0') {
        uart_puts("Usage: rm <filename>\n");
        return;
    }

    Node *parent;
    Node *file = fs_find_with_parent(path, &parent);

    if (!file) {
        uart_puts("File does not exist!\n");
        return;
    }

    if (file->type != FILE_NODE) {
        uart_puts("Not a file! Use rmdir for directories.\n");
        return;
    }

    // PROTECTION: Check if system file
    if (fs_is_system(file)) {
        uart_puts("Permission denied: cannot delete system file.\n");
        return;
    }

    // PROTECTION: Check write permission on parent directory
    if (!fs_can_write(parent)) {
        uart_puts("Permission denied: cannot modify this directory.\n");
        return;
    }

    // Remove from parent's children array
    for (unsigned int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == file) {
            // Shift remaining children
            for (unsigned int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            uart_puts("File removed.\n");
            return;
        }
    }
}

// Remove empty directory (rmdir)
void fs_rmdir(const char *path) {
    if (!path || *path == '\0') {
        uart_puts("Usage: rmdir <dirname>\n");
        return;
    }

    Node *parent;
    Node *dir = fs_find_with_parent(path, &parent);

    if (!dir) {
        uart_puts("Directory does not exist!\n");
        return;
    }

    if (dir->type != DIR_NODE) {
        uart_puts("Not a directory! Use rm for files.\n");
        return;
    }

    // PROTECTION: Check if system directory
    if (fs_is_system(dir)) {
        uart_puts("Permission denied: cannot delete system directory.\n");
        return;
    }

    // PROTECTION: Check write permission on parent directory
    if (!fs_can_write(parent)) {
        uart_puts("Permission denied: cannot modify parent directory.\n");
        return;
    }

    // Check if directory is empty
    if (dir->child_count > 0) {
        uart_puts("Directory not empty!\n");
        return;
    }

    // Remove from parent's children array
    for (unsigned int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == dir) {
            for (unsigned int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            uart_puts("Directory removed.\n");
            return;
        }
    }
}

//==================================================
//         PERMISSION MANAGEMENT (NEW)
//==================================================

// Helper to convert permission number to string
static void perm_to_str(unsigned int perms, char *buf) {
    buf[0] = (perms & PERM_READ)  ? 'r' : '-';
    buf[1] = (perms & PERM_WRITE) ? 'w' : '-';
    buf[2] = (perms & PERM_EXEC)  ? 'x' : '-';
    buf[3] = '\0';
}

// Change file/directory permissions (chmod)
void fs_chmod(const char *path, unsigned int perms) {
    if (!path || *path == '\0') {
        uart_puts("Usage: chmod <path> <perms>\n");
        return;
    }

    // Validate permissions (0-7)
    if (perms > 7) {
        uart_puts("Invalid permissions! Use 0-7.\n");
        return;
    }

    Node *parent;
    Node *node = fs_find_with_parent(path, &parent);

    if (!node) {
        uart_puts("File/directory does not exist!\n");
        return;
    }

    // PROTECTION: Cannot change permissions on system nodes
    if (fs_is_system(node)) {
        uart_puts("Permission denied: cannot modify system file permissions.\n");
        return;
    }

    node->permissions = perms;
    
    char perm_str[4];
    perm_to_str(perms, perm_str);
    uart_puts("Permissions changed to: ");
    uart_puts(perm_str);
    uart_puts("\n");
}

// Show file/directory information (stat)
void fs_stat(const char *path) {
    if (!path || *path == '\0') {
        uart_puts("Usage: stat <path>\n");
        return;
    }

    Node *parent;
    Node *node = fs_find_with_parent(path, &parent);

    if (!node) {
        uart_puts("File/directory does not exist!\n");
        return;
    }

    uart_puts("  Name: ");
    uart_puts(node->name);
    uart_puts("\n");

    uart_puts("  Type: ");
    uart_puts(node->type == DIR_NODE ? "directory" : "file");
    uart_puts("\n");

    char perm_str[4];
    perm_to_str(node->permissions, perm_str);
    uart_puts("  Perms: ");
    uart_puts(perm_str);
    uart_puts(" (");
    uart_putc('0' + node->permissions);
    uart_puts(")\n");

    uart_puts("  Flags: ");
    if (fs_is_system(node)) uart_puts("[SYSTEM] ");
    if (fs_is_hidden(node)) uart_puts("[HIDDEN] ");
    if (!fs_is_system(node) && !fs_is_hidden(node)) uart_puts("(none)");
    uart_puts("\n");

    if (node->type == FILE_NODE) {
        uart_puts("  Size: ");
        int len = strlen(node->content);
        // Simple number printing
        if (len == 0) uart_putc('0');
        else {
            char num[12];
            int i = 0;
            while (len > 0) {
                num[i++] = '0' + (len % 10);
                len /= 10;
            }
            while (i > 0) uart_putc(num[--i]);
        }
        uart_puts(" bytes\n");
    } else {
        uart_puts("  Children: ");
        int cnt = node->child_count;
        if (cnt == 0) uart_putc('0');
        else {
            char num[12];
            int i = 0;
            while (cnt > 0) {
                num[i++] = '0' + (cnt % 10);
                cnt /= 10;
            }
            while (i > 0) uart_putc(num[--i]);
        }
        uart_puts("\n");
    }
}