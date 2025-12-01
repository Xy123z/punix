/**
 * src/fs.c - Lazy Loading Filesystem (Load-On-Demand)
 * Only loads nodes from disk when accessed, not at boot
 */

#include "../include/fs.h"
#include "../include/string.h"
#include "../include/memory.h"
#include "../include/console.h"
#include "../include/ata.h"

// --- Configuration ---
#define FS_MAGIC         0xEF5342
#define FS_MAX_NODES     128
#define FS_ROOT_ID       1
#define SECTOR_SIZE      512

// --- NEW: Cache Management ---
#define FS_CACHE_SIZE    32  // Keep 32 nodes in RAM (adjustable)

typedef struct {
    fs_node_t node;          // The actual node data
    uint32_t  id;            // Node ID (0 = empty slot)
    uint32_t  last_access;   // For LRU eviction
    uint8_t   dirty;         // 1 = modified, needs write-back
} fs_cache_entry_t;

// --- Data Structures ---
typedef struct {
    uint32_t magic;
    uint32_t root_id;
    uint32_t next_free_id;
    uint32_t total_nodes;
    uint32_t used_sectors;
    uint8_t  reserved[492];
} superblock_t;

// --- Globals ---
uint32_t fs_root_id = FS_ROOT_ID;
uint32_t fs_current_dir_id = FS_ROOT_ID;

static superblock_t sb;
static fs_cache_entry_t cache[FS_CACHE_SIZE];  // Cache instead of full table!
static uint32_t access_counter = 0;            // For LRU tracking

// Helper macros
#define NODE_ID_TO_SECTOR(id) (FS_NODE_TABLE_START + (id) - 1)

// --- Internal Helpers ---

static void save_superblock() {
    ata_write_sectors(FS_SUPERBLOCK_SECTOR, 1, &sb);
}

/**
 * @brief Finds a node in the cache by ID
 * @return Index in cache, or -1 if not found
 */
static int cache_find(uint32_t id) {
    for (int i = 0; i < FS_CACHE_SIZE; i++) {
        if (cache[i].id == id) {
            cache[i].last_access = ++access_counter;  // Update LRU
            return i;
        }
    }
    return -1;
}

/**
 * @brief Finds an empty slot in cache, or evicts least recently used
 * @return Index of available slot
 */
static int cache_find_slot() {
    // First, try to find an empty slot
    for (int i = 0; i < FS_CACHE_SIZE; i++) {
        if (cache[i].id == 0) {
            return i;
        }
    }

    // No empty slots - find LRU (least recently used)
    int lru_index = 0;
    uint32_t oldest_access = cache[0].last_access;

    for (int i = 1; i < FS_CACHE_SIZE; i++) {
        if (cache[i].last_access < oldest_access) {
            oldest_access = cache[i].last_access;
            lru_index = i;
        }
    }

    // Write back if dirty
    if (cache[lru_index].dirty) {
        ata_write_sectors(NODE_ID_TO_SECTOR(cache[lru_index].id),
                         1, &cache[lru_index].node);
    }

    return lru_index;
}

/**
 * @brief Loads a node from disk into cache
 * @return Pointer to cached node, or NULL on error
 */
static fs_node_t* cache_load(uint32_t id) {
    if (id == 0 || id >= FS_MAX_NODES) return 0;

    // Check if already in cache
    int idx = cache_find(id);
    if (idx >= 0) {
        return &cache[idx].node;  // Cache hit!
    }

    // Not in cache - load from disk
    int slot = cache_find_slot();

    // Read from disk
    ata_read_sectors(NODE_ID_TO_SECTOR(id), 1, &cache[slot].node);

    // Validate that we got the right node
    if (cache[slot].node.id != id) {
        return 0;  // Node doesn't exist or disk corruption
    }

    // Update cache metadata
    cache[slot].id = id;
    cache[slot].dirty = 0;
    cache[slot].last_access = ++access_counter;

    return &cache[slot].node;
}

/**
 * @brief Marks a cached node as dirty (needs write-back)
 */
static void cache_mark_dirty(uint32_t id) {
    int idx = cache_find(id);
    if (idx >= 0) {
        cache[idx].dirty = 1;
    }
}

/**
 * @brief Writes a node to disk and updates cache
 */
static void save_node(uint32_t id) {
    if (id == 0 || id >= FS_MAX_NODES) return;

    int idx = cache_find(id);
    if (idx >= 0) {
        // Node is in cache - write it
        ata_write_sectors(NODE_ID_TO_SECTOR(id), 1, &cache[idx].node);
        cache[idx].dirty = 0;
    } else {
        // Node not in cache - load it first, then write
        fs_node_t* node = cache_load(id);
        if (node) {
            ata_write_sectors(NODE_ID_TO_SECTOR(id), 1, node);
            cache_mark_dirty(id);
            cache[cache_find(id)].dirty = 0;
        }
    }
}

/**
 * @brief Flushes all dirty nodes to disk
 */
void fs_sync() {
    for (int i = 0; i < FS_CACHE_SIZE; i++) {
        if (cache[i].id != 0 && cache[i].dirty) {
            ata_write_sectors(NODE_ID_TO_SECTOR(cache[i].id),
                            1, &cache[i].node);
            cache[i].dirty = 0;
        }
    }
    console_print_colored("FS: Cache synced to disk.\n", COLOR_GREEN_ON_BLACK);
}

// Formats the disk
static void mkfs() {
    console_print_colored("FS: Formatting drive...\n", COLOR_YELLOW_ON_BLACK);

    // Clear cache
    memset(cache, 0, sizeof(cache));

    // Setup Superblock
    sb.magic = FS_MAGIC;
    sb.root_id = FS_ROOT_ID;
    sb.next_free_id = 2;
    sb.total_nodes = 1;
    sb.used_sectors = 1 + 60 + 1 + 1;  // boot + kernel + superblock + root

    // Create Root Node
    fs_node_t* root = cache_load(FS_ROOT_ID);
    if (!root) {
        int slot = cache_find_slot();
        cache[slot].id = FS_ROOT_ID;
        cache[slot].node.id = FS_ROOT_ID;
        cache[slot].node.parent_id = FS_ROOT_ID;
        cache[slot].node.type = FS_TYPE_DIRECTORY;
        strcpy(cache[slot].node.name, "");
        cache[slot].node.size = 0;
        cache[slot].node.child_count = 0;
        cache[slot].dirty = 1;
        cache[slot].last_access = ++access_counter;
        root = &cache[slot].node;
    }

    save_node(FS_ROOT_ID);

    // Helper function to create directories
     void CREATE_DIR(const char* name, const char* description)
    {
        uint32_t dir_id = sb.next_free_id++;
        sb.total_nodes++;
        sb.used_sectors++;
        int slot = cache_find_slot();
        cache[slot].id = dir_id;
        cache[slot].node.id = dir_id;
        cache[slot].node.parent_id = FS_ROOT_ID;
        cache[slot].node.type = FS_TYPE_DIRECTORY;
        strcpy(cache[slot].node.name, name);
        cache[slot].node.size = 0;
        cache[slot].node.child_count = 0;
        cache[slot].dirty = 1;
        cache[slot].last_access = ++access_counter;
        root->child_ids[root->child_count++] = dir_id;
        cache_mark_dirty(FS_ROOT_ID);
        save_node(dir_id);
        console_print_colored("FS: Created ", COLOR_GREEN_ON_BLACK);
        console_print("/");
        console_print(name);
        console_print(" - ");
        console_print(description);
        console_print("\n");
    }

    // Create standard Unix directories
    CREATE_DIR("bin", "Essential user commands");
    CREATE_DIR("boot", "Boot files (informational)");
    CREATE_DIR("dev", "Device files (future)");
    CREATE_DIR("etc", "System configuration");
    CREATE_DIR("home", "User home directories");
    CREATE_DIR("lib", "Shared libraries (future)");
    CREATE_DIR("mnt", "Mount points");
    CREATE_DIR("opt", "Optional software");
    CREATE_DIR("proc", "Process info (future)");
    CREATE_DIR("root", "Root user home");
    CREATE_DIR("sbin", "System binaries");
    CREATE_DIR("sys", "System info (future)");
    CREATE_DIR("tmp", "Temporary files");
    CREATE_DIR("usr", "User programs");
    CREATE_DIR("var", "Variable data");

    // Create user workspace (your /a directory)
    CREATE_DIR("a", "User workspace");

    // Create history directory
    CREATE_DIR("h", "Command history");

    #undef CREATE_DIR

    // Create system info files in /boot
    uint32_t boot_id = fs_find_node_local_id(FS_ROOT_ID, "boot");
    if (boot_id) {
        // Create version file
        if (fs_create_node(boot_id, "version", FS_TYPE_FILE)) {
            uint32_t version_id = fs_find_node_local_id(boot_id, "version");
            fs_node_t* version_file = fs_get_node(version_id);
            if (version_file) {
                char* content = (char*)version_file->padding;
                strcpy(content, "PUNIX Kernel v1.03\n");
                strcat(content, "Build: 2024-12-01\n");
                strcat(content, "Architecture: x86 (32-bit)\n");
                version_file->size = strlen(content);
                fs_update_node(version_file);
            }
        }

        // Create README
        if (fs_create_node(boot_id, "README", FS_TYPE_FILE)) {
            uint32_t readme_id = fs_find_node_local_id(boot_id, "README");
            fs_node_t* readme_file = fs_get_node(readme_id);
            if (readme_file) {
                char* content = (char*)readme_file->padding;
                strcpy(content, "Boot Directory\n");
                strcat(content, "==============\n\n");
                strcat(content, "This directory contains system information.\n");
                strcat(content, "The actual bootloader and kernel are stored\n");
                strcat(content, "in fixed disk sectors (0-60), not in the\n");
                strcat(content, "filesystem.\n\n");
                strcat(content, "Bootloader: Sector 0 (512 bytes)\n");
                strcat(content, "Kernel:     Sectors 1-60 (~30 KB)\n");
                readme_file->size = strlen(content);
                fs_update_node(readme_file);
            }
        }
    }

    // Create /etc/motd (Message of the Day)
    uint32_t etc_id = fs_find_node_local_id(FS_ROOT_ID, "etc");
    if (etc_id) {
        if (fs_create_node(etc_id, "motd", FS_TYPE_FILE)) {
            uint32_t motd_id = fs_find_node_local_id(etc_id, "motd");
            fs_node_t* motd_file = fs_get_node(motd_id);
            if (motd_file) {
                char* content = (char*)motd_file->padding;
                strcpy(content, "Welcome to PUNIX!\n");
                strcat(content, "Type 'help' for available commands.\n");
                motd_file->size = strlen(content);
                fs_update_node(motd_file);
            }
        }
    }

    save_node(FS_ROOT_ID);
    save_superblock();

    console_print_colored("\nFS: Format complete. ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Standard directory structure created.\n", COLOR_GREEN_ON_BLACK);
}

// --- Public API ---

void fs_init() {
    // Initialize cache
    memset(cache, 0, sizeof(cache));
    access_counter = 0;

    // Read Superblock ONLY (not all nodes!)
    ata_read_sectors(FS_SUPERBLOCK_SECTOR, 1, &sb);

    if (sb.magic != FS_MAGIC) {
        console_print_colored("FS: No filesystem detected.\n", COLOR_LIGHT_RED);
        mkfs();
    } else {
        console_print_colored("FS: Filesystem mounted (lazy loading enabled).\n", COLOR_GREEN_ON_BLACK);
        // NOTE: We DON'T load all nodes here anymore!
        // They will be loaded on-demand when accessed
    }

    fs_root_id = FS_ROOT_ID;

    // Load root and /a into cache for initial access
    cache_load(FS_ROOT_ID);

    fs_node_t* dir_a = fs_find_node("a", fs_root_id);
    if (dir_a && dir_a->type == FS_TYPE_DIRECTORY) {
        fs_current_dir_id = dir_a->id;
        console_print_colored("FS: Working directory set to /a.\n", COLOR_GREEN_ON_BLACK);
    } else {
        fs_current_dir_id = fs_root_id;
    }
}

// Get node - now uses cache with lazy loading!
fs_node_t* fs_get_node(uint32_t id) {
    return cache_load(id);  // Loads from disk if not in cache
}

int fs_update_node(fs_node_t* node) {
    if (!node || node->id == 0) return 0;
    cache_mark_dirty(node->id);
    save_node(node->id);
    return 1;
}

uint32_t fs_find_node_local_id(uint32_t parent_id, char* name) {
    fs_node_t* parent = fs_get_node(parent_id);  // Lazy loads parent
    if (!parent || parent->type != FS_TYPE_DIRECTORY) return 0;

    for (uint32_t i = 0; i < parent->child_count; i++) {
        uint32_t child_id = parent->child_ids[i];
        fs_node_t* child = fs_get_node(child_id);  // Lazy loads child
        if (child && strcmp(child->name, name) == 0) {
            return child_id;
        }
    }
    return 0;
}

fs_node_t* fs_find_node(char* path, uint32_t start_id) {
    if (!path) return 0;

    uint32_t current_id = start_id;

    if (path[0] == '/') {
        current_id = fs_root_id;
        path++;
    }

    char temp_path[128];
    strncpy(temp_path, path, 127);
    temp_path[127] = '\0';
    char* component = temp_path;
    char* next_component = 0;

    while (*component != '\0') {
        int i = 0;
        while (component[i] != '/' && component[i] != '\0') {
            i++;
        }

        if (component[i] == '/') {
            component[i] = '\0';
            next_component = component + i + 1;
        } else {
            next_component = 0;
        }

        if (strlen(component) > 0) {
            if (strcmp(component, "..") == 0) {
                fs_node_t* cur = fs_get_node(current_id);  // Lazy load
                if (cur) current_id = cur->parent_id;
            }
            else if (strcmp(component, ".") == 0) {
                // Do nothing
            }
            else {
                uint32_t next_id = fs_find_node_local_id(current_id, component);
                if (next_id == 0) return 0;
                current_id = next_id;
            }
        }

        if (!next_component) break;
        component = next_component;
    }

    return fs_get_node(current_id);  // Lazy load final node
}

int fs_create_node(uint32_t parent_id, char* name, uint8_t type) {
    if (sb.next_free_id >= FS_MAX_NODES) {
        console_print_colored("FS: Disk full.\n", COLOR_LIGHT_RED);
        return 0;
    }

    fs_node_t* parent = fs_get_node(parent_id);  // Lazy load parent
    if (!parent || parent->type != FS_TYPE_DIRECTORY) return 0;
    if (parent->child_count >= 16) {
        console_print_colored("FS: Directory full.\n", COLOR_LIGHT_RED);
        return 0;
    }

    uint32_t new_id = sb.next_free_id++;
    sb.total_nodes++;
    sb.used_sectors++;
    save_superblock();

    // Create new node in cache
    int slot = cache_find_slot();
    fs_node_t* node = &cache[slot].node;

    memset(node, 0, sizeof(fs_node_t));
    node->id = new_id;
    node->parent_id = parent_id;
    node->type = type;
    strncpy(node->name, name, 63);
    node->name[63] = '\0';
    node->size = 0;
    node->child_count = 0;

    cache[slot].id = new_id;
    cache[slot].dirty = 1;
    cache[slot].last_access = ++access_counter;

    parent->child_ids[parent->child_count++] = new_id;
    cache_mark_dirty(parent_id);

    save_node(new_id);
    save_node(parent_id);

    return 1;
}

int fs_delete_node(uint32_t id) {
    fs_node_t* node = fs_get_node(id);  // Lazy load
    if (!node) return 0;

    if (node->type == FS_TYPE_DIRECTORY && node->child_count > 0) {
        return 0;
    }

    fs_node_t* parent = fs_get_node(node->parent_id);  // Lazy load parent
    if (parent) {
        int found = 0;
        for (uint32_t i = 0; i < parent->child_count; i++) {
            if (parent->child_ids[i] == id) {
                for (uint32_t j = i; j < parent->child_count - 1; j++) {
                    parent->child_ids[j] = parent->child_ids[j+1];
                }
                parent->child_count--;
                found = 1;
                break;
            }
        }
        if (found) {
            cache_mark_dirty(parent->id);
            save_node(parent->id);
        }
    }

    sb.total_nodes--;
    sb.used_sectors--;
    save_superblock();

    memset(node, 0, sizeof(fs_node_t));
    save_node(id);

    // Invalidate cache entry
    int idx = cache_find(id);
    if (idx >= 0) {
        cache[idx].id = 0;
        cache[idx].dirty = 0;
    }

    return 1;
}

void fs_get_disk_stats(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb) {
    *total_kb = 50 * 1024;
    *used_kb = (sb.used_sectors * SECTOR_SIZE) / 1024;
    *free_kb = *total_kb - *used_kb;
}

// NEW: Get cache statistics
void fs_get_cache_stats(uint32_t* cache_size, uint32_t* cached_nodes, uint32_t* dirty_nodes) {
    *cache_size = FS_CACHE_SIZE;
    *cached_nodes = 0;
    *dirty_nodes = 0;

    for (int i = 0; i < FS_CACHE_SIZE; i++) {
        if (cache[i].id != 0) {
            (*cached_nodes)++;
            if (cache[i].dirty) {
                (*dirty_nodes)++;
            }
        }
    }
}
