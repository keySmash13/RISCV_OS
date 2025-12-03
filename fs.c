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

// Create empty file (touch)
void fs_touch(const char *path) {
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

    uart_puts(file->content);
    uart_puts("\n");
}