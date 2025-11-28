#include "../include/fs.h"
#include "../include/console.h"
#include "../include/vga.h"
#include "../include/memory.h" // kmalloc, kfree
#include "../include/interrupt.h"
#include "../include/shell.h" // For ROOT_ACCESS_GRANTED global
#include "../include/string.h" // strcmp, strcpy, strlen, memset, memcpy
#include "../include/types.h"

// Global state tracking variables
fs_node_t* fs_root = 0;
fs_node_t* fs_current_dir = 0;

// Internal helpers
static int fs_add_child(fs_node_t* parent, fs_node_t* child);
static int fs_remove_child(fs_node_t* parent, fs_node_t* child);
static fs_node_t* fs_recursive_find_node(fs_node_t* current_node, const char* path);
static void fs_recursive_delete(fs_node_t* node);


/**
 * @brief Allocates and initializes a new file system node, including dynamic children array for directories.
 */
static fs_node_t* fs_node_create(const char* name, fs_node_type_t type, fs_node_t* parent) {
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) return 0;

    memset(node, 0, sizeof(fs_node_t)); // Clear the node structure

    // Copy and truncate name
    if (strlen(name) < FS_MAX_NAME) {
        strcpy(node->name, name);
    } else {
        strncpy(node->name, name, FS_MAX_NAME - 1);
        node->name[FS_MAX_NAME - 1] = '\0';
    }

    node->type = type;
    node->parent = parent;
    node->num_children = 0;

    // Initialize dynamic children array for directories
    if (type == FS_DIRECTORY) {
        node->children_capacity = FS_INITIAL_CHILDREN;
        node->children = (fs_node_t**)kmalloc(node->children_capacity * sizeof(fs_node_t*));
        if (!node->children) {
            kfree(node);
            return 0; // Failed to allocate children array
        }
        memset(node->children, 0, node->children_capacity * sizeof(fs_node_t*));
    } else {
        node->children_capacity = 0;
        node->children = 0;
    }

    return node;
}

/**
 * @brief Attempts to add a child node to a directory node, resizing the array if necessary.
 * @return 0 on success, -1 on failure.
 */
static int fs_add_child(fs_node_t* parent, fs_node_t* child) {
    if (parent->type != FS_DIRECTORY) {
        return -1; // Not a directory
    }

    // Check if resize is needed (dynamic expansion)
    if (parent->num_children >= parent->children_capacity) {
        int new_capacity = parent->children_capacity * 2;
        if (new_capacity == 0) new_capacity = FS_INITIAL_CHILDREN;

        fs_node_t** new_children = (fs_node_t**)kmalloc(new_capacity * sizeof(fs_node_t*));
        if (!new_children) {
            console_print_colored("Error: Failed to reallocate directory children.\n", COLOR_YELLOW_ON_BLACK);
            return -1;
        }

        // Copy existing children to the new array
        memcpy(new_children, parent->children, parent->num_children * sizeof(fs_node_t*));

        // Free the old array and update pointers
        kfree(parent->children);
        parent->children = new_children;
        parent->children_capacity = new_capacity;
    }

    // Add the new child
    parent->children[parent->num_children] = child;
    parent->num_children++;
    child->parent = parent;
    return 0;
}

/**
 * @brief Removes a child node from a directory node's list.
 * @return 0 on success, -1 if child not found.
 */
static int fs_remove_child(fs_node_t* parent, fs_node_t* child) {
    if (parent->type != FS_DIRECTORY) return -1;

    for (int i = 0; i < parent->num_children; ++i) {
        if (parent->children[i] == child) {
            // Shift elements left to fill the gap
            for (int j = i; j < parent->num_children - 1; ++j) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->num_children--;
            parent->children[parent->num_children] = 0; // Clear the last pointer
            return 0;
        }
    }
    return -1; // Child not found
}

/**
 * @brief Recursively deletes a node and all its contents (files and subdirectories).
 * This function should only be called by fs_delete_node.
 */
static void fs_recursive_delete(fs_node_t* node) {
    if (!node) return;

    // 1. If it's a directory, recursively delete all children
    if (node->type == FS_DIRECTORY) {
        for (int i = 0; i < node->num_children; ++i) {
            fs_recursive_delete(node->children[i]);
        }
        // Free the dynamic children array itself
        if (node->children) {
            kfree(node->children);
        }
    }
    // 2. If it's a file, free its content data
    else if (node->type == FS_FILE) {
        if (node->content_data) {
            kfree(node->content_data);
        }
    }

    // 3. Free the node structure itself
    kfree(node);
}

/**
 * @brief Deletes a file system node. It is only safe to delete a directory if it is empty.
 * @param node The node to delete.
 * @return 0 on success, -1 on failure (e.g., node is root or not empty).
 */
int fs_delete_node(fs_node_t* node) {
    if (!node || node == fs_root) return -1; // Cannot delete root

    // If it's a directory, ensure it's empty
    if (node->type == FS_DIRECTORY && node->num_children > 0) {
        console_print_colored("Error: Directory not empty.\n", COLOR_YELLOW_ON_BLACK);
        return -1;
    }

    // 1. Remove node from its parent's children list
    if (node->parent) {
        fs_remove_child(node->parent, node);
    }

    // 2. Recursively free the node's memory
    fs_recursive_delete(node);

    return 0;
}


/**
 * @brief Initializes the RAMFS, creating the root directory and initial subdirectories.
 */
void fs_init() {
    fs_root = fs_node_create("/", FS_DIRECTORY, 0); // Root has no parent
    if (!fs_root) {
        console_print_colored("FS Init Error: Failed to create root node.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    fs_current_dir = fs_root; // Start in root

    // Create /a and /h directories
    fs_node_t* dir_a = fs_node_create("a", FS_DIRECTORY, fs_root);
    fs_node_t* dir_h = fs_node_create("h", FS_DIRECTORY, fs_root);

    if (dir_a) fs_add_child(fs_root, dir_a);
    if (dir_h) fs_add_child(fs_root, dir_h);

    console_print_colored("RAMFS initialized (Dynamic/Recursive VFS enabled).\n", COLOR_GREEN_ON_BLACK);
}

/**
 * @brief Locates a node by name within a starting directory (relative lookup).
 * @return Pointer to the node if found, 0 otherwise.
 */
fs_node_t* fs_find_node_local(const char* name, fs_node_t* start_node) {
    if (!start_node || start_node->type != FS_DIRECTORY) return 0;

    for (int i = 0; i < start_node->num_children; ++i) {
        if (strcmp(start_node->children[i]->name, name) == 0) {
            return start_node->children[i];
        }
    }
    return 0;
}

/**
 * @brief Internal recursive function to resolve multi-component paths.
 */
static fs_node_t* fs_recursive_find_node(fs_node_t* current_node, const char* path) {
    if (!current_node || *path == '\0') {
        return current_node;
    }

    // Find the end of the current component (up to '/' or '\0')
    char component[FS_MAX_NAME];
    int len = 0;
    while (*path != '/' && *path != '\0' && len < FS_MAX_NAME - 1) {
        component[len++] = *path++;
    }
    component[len] = '\0';

    // Move path past the separator, if any
    if (*path == '/') path++;

    // Handle special components
    if (strcmp(component, ".") == 0) {
        // Current directory: recurse with the rest of the path
        return fs_recursive_find_node(current_node, path);
    } else if (strcmp(component, "..") == 0) {
        // Parent directory: recurse from parent
        return fs_recursive_find_node(current_node->parent ? current_node->parent : fs_root, path);
    } else if (len == 0) {
        // Handles trailing slashes (e.g., /a/b/) by returning the current node
        return current_node;
    }

    // Look for the component locally
    fs_node_t* next_node = fs_find_node_local(component, current_node);

    if (!next_node) {
        return 0; // Component not found
    }

    // If there are more path components, and the next node is a directory, continue recursion
    if (*path != '\0') {
        if (next_node->type == FS_DIRECTORY) {
            return fs_recursive_find_node(next_node, path);
        } else {
            return 0; // Cannot traverse into a file
        }
    }

    return next_node; // End of path reached
}

/**
 * @brief Finds a file system node based on a full or relative path.
 * The entry point for path resolution.
 * @param path The path string (e.g., "a", "../b/c", "/usr/bin").
 * @param start_node The directory to begin the search from.
 * @return Pointer to the found node, or 0.
 */
fs_node_t* fs_find_node(const char* path, fs_node_t* start_node) {
    if (!start_node) return 0;
    if (path == 0 || *path == '\0') return start_node; // Empty path means current directory

    fs_node_t* current = start_node;
    const char* path_ptr = path;

    // Determine the starting point: absolute path starts at root
    if (path[0] == '/') {
        current = fs_root;
        path_ptr++; // Skip the leading slash
    }

    return fs_recursive_find_node(current, path_ptr);
}

/**
 * @brief Changes the current working directory.
 * @param path The path to the new directory.
 * @return 0 on success, -1 on root access denied (original kernel logic), -2 on not found/not a directory.
 */
int fs_change_dir(const char* path) {
    fs_node_t* next_dir = fs_find_node(path, fs_current_dir);

    if (next_dir && next_dir->type == FS_DIRECTORY) {
        // Preservation of original root access denial logic
        if (next_dir == fs_root && !ROOT_ACCESS_GRANTED && fs_current_dir != fs_root) {
             return -1; // Specific code for root access denial
        }

        fs_current_dir = next_dir;
        return 0;
    }

    return -2; // Not found or not a directory
}

/**
 * @brief Creates a new file or directory node within the current directory.
 * @param name The name of the new entry.
 * @param type The type (FS_FILE or FS_DIRECTORY).
 * @param parent The parent directory to create the node in.
 * @return Pointer to the newly created node, or 0 on failure.
 */
fs_node_t* fs_create_node(const char* name, fs_node_type_t type, fs_node_t* parent) {
    // 1. Check if name already exists
    if (fs_find_node_local(name, parent)) {
        console_print_colored("Error: Name already exists in directory.\n", COLOR_YELLOW_ON_BLACK);
        return 0;
    }

    // Simple validation for names that could confuse the path resolver
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strchr(name, '/') != 0) {
        console_print_colored("Error: Invalid name (cannot be '.', '..', or contain '/').\n", COLOR_YELLOW_ON_BLACK);
        return 0;
    }

    // 2. Create and add node
    fs_node_t* new_node = fs_node_create(name, type, parent);
    if (!new_node) {
        console_print_colored("Error: Failed to allocate new node.\n", COLOR_YELLOW_ON_BLACK);
        return 0;
    }

    if (fs_add_child(parent, new_node) != 0) {
        // If adding failed (e.g., due to memory reallocation failure)
        kfree(new_node);
        console_print_colored("Error: Failed to add node to parent (memory full?).\n", COLOR_YELLOW_ON_BLACK);
        return 0;
    }
    return new_node;
}
