#ifndef FS_H
#define FS_H

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
} Node;

Node* fs_alloc_node(void);
void fs_init(void);
Node *fs_find(Node *dir, const char *name);
Node* fs_traverse_path(const char *path, int create_missing);
void fs_mkdir(const char *path);
void fs_touch(const char *path);
void fs_ls(const char *path);
void fs_cd(const char *path);
void fs_pwd_recursive(Node *n);
void fs_pwd(void);
void fs_write(const char *path, const char *text);
void fs_cat(const char *path);


#endif