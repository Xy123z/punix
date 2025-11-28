#ifndef FS_H
#define FS_H

#include "memory.h"// For size_t, etc.

// Max name length for files and directories
#define FS_MAX_NAME 40
// Initial number of entries a directory can hold before reallocation
#define FS_INITIAL_CHILDREN 4

// Node types (File, Directory)
typedef enum {
    FS_FILE,
    FS_DIRECTORY
} fs_node_type_t;

// The core File System Node structure (our RAMFS "inode")
typedef struct fs_node {
    char name[FS_MAX_NAME];       // Name of the file/directory
    fs_node_type_t type;          // Is this a file or a directory?

    // Pointers for directory structure
    struct fs_node* parent;
    struct fs_node** children;    // NEW: Pointer to dynamically allocated array of child pointers
    int num_children;
    int children_capacity;        // NEW: Tracks the current size of the children array

    // File content storage (if type is FS_FILE)
    char* content_data;           // Pointer to the actual file content on the heap
    size_t size;                  // Size of the file content in bytes
    size_t allocated_size;        // Size of the memory block allocated on the heap (content_data)

} fs_node_t;

// Global state tracking
extern fs_node_t* fs_root;           // The root of the file system
extern fs_node_t* fs_current_dir;    // The current working directory

// Function Prototypes
void fs_init();
fs_node_t* fs_find_node(const char* path, fs_node_t* start_node);
int fs_change_dir(const char* path);

// CRUD operations
fs_node_t* fs_create_node(const char* name, fs_node_type_t type, fs_node_t* parent);
int fs_delete_node(fs_node_t* node); // Function to delete a node and free its memory
fs_node_t* fs_find_node_local(const char* name, fs_node_t* start_node);

#endif // FS_H
