#ifndef FS_H
#define FS_H

#define MAX_NAME 16
#define MAX_FILES 16
#define MAX_NODES 64

// Node can be a file or directory
typedef enum { FILE_NODE, DIR_NODE } NodeType;

//--------------------------------------------------
//          PERMISSION FLAGS (Unix-like)
//--------------------------------------------------
// Permission bits: rwx (read, write, execute)
#define PERM_READ    0x04    // 100 binary - can read file/list directory
#define PERM_WRITE   0x02    // 010 binary - can write file/modify directory
#define PERM_EXEC    0x01    // 001 binary - can execute file/enter directory

// Common permission combinations
#define PERM_RW      (PERM_READ | PERM_WRITE)           // 6 - read + write
#define PERM_RX      (PERM_READ | PERM_EXEC)            // 5 - read + execute
#define PERM_RWX     (PERM_READ | PERM_WRITE | PERM_EXEC) // 7 - full access
#define PERM_NONE    0x00                                // 0 - no access

// Node flags
#define FLAG_SYSTEM  0x10    // System file/directory - cannot be deleted
#define FLAG_HIDDEN  0x20    // Hidden from normal ls listing

// Basic filesystem node structure
typedef struct Node {
    char name[MAX_NAME];
    NodeType type;
    char content[128];          // File content area
    struct Node *children[MAX_FILES];
    struct Node *parent;
    unsigned int child_count;
    unsigned int permissions;   // Permission bits (PERM_READ, PERM_WRITE, PERM_EXEC)
    unsigned int flags;         // Special flags (FLAG_SYSTEM, FLAG_HIDDEN)
} Node;

// Permission checking helpers
int fs_can_read(Node *node);
int fs_can_write(Node *node);
int fs_can_exec(Node *node);
int fs_is_system(Node *node);
int fs_is_hidden(Node *node);

// Core functions
Node* fs_alloc_node(void);
void fs_init(void);
Node *fs_find(Node *dir, const char *name);
Node* fs_traverse_path(const char *path, int create_missing);

// Directory operations
void fs_mkdir(const char *path);
void fs_ls(const char *path);
void fs_ls_all(const char *path);      // Show hidden files too
void fs_cd(const char *path);
void fs_pwd_recursive(Node *n);
void fs_pwd(void);

// File operations
void fs_touch(const char *path);
void fs_touch_with_perms(const char *path, unsigned int perms);
void fs_write(const char *path, const char *text);
void fs_cat(const char *path);

// Delete operations (with permission checking)
void fs_rm(const char *path);           // Remove file
void fs_rmdir(const char *path);        // Remove empty directory

// Permission management
void fs_chmod(const char *path, unsigned int perms);
void fs_stat(const char *path);         // Show file info and permissions

// Program execution support
// Returns pointer to file content if file exists and is executable, NULL otherwise
const char* fs_get_executable(const char *path);

#endif