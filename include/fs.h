#ifndef FS_H
#define FS_H
#include "types.h"

// --- Constants ---
#define FS_TYPE_FILE        0
#define FS_TYPE_DIRECTORY   1
#define FS_MAX_NAME         64
#define FS_MAX_CHILDREN     16

// --- CRITICAL FIX: Correct sector numbers ---
// Disk Layout:
// LBA 0: Bootloader (CHS Sector 1)
// LBA 1-60: Kernel (CHS Sectors 2-61)
// LBA 61: Filesystem Superblock (CHS Sector 62)
// LBA 62+: Node Table (CHS Sector 63+)
#define FS_SUPERBLOCK_SECTOR    61  // Superblock at LBA 61
#define FS_NODE_TABLE_START     62  // Node table starts at LBA 62
// -----------------------------------------------

// --- Data Structures ---
/**
 * @brief The Inode structure representing a file or directory.
 * Sized to fit comfortably within a 512-byte ATA sector.
 * (Approx 152 bytes used + padding).
 */
typedef struct fs_node {
    uint32_t id;
    uint32_t parent_id;
    uint8_t  type;              // FS_TYPE_FILE or FS_TYPE_DIRECTORY
    char     name[FS_MAX_NAME]; // Fixed name buffer
    uint32_t size;              // Size in bytes
    uint32_t child_count;
    uint32_t child_ids[FS_MAX_CHILDREN];
    uint8_t  padding[300];      // Padding to ensure struct size ~512 bytes
} fs_node_t;

// --- Global State ---
// These allow the shell to know "where" it is globally
extern uint32_t fs_root_id;
extern uint32_t fs_current_dir_id;

// --- Function Prototypes ---
/**
 * @brief Initializes the file system.
 * Reads the Superblock from Disk. If invalid, formats the drive.
 */
void fs_init();

/**
 * @brief Retrieves a node pointer by its unique ID.
 * @return Pointer to the node in memory, or 0 if invalid.
 */
fs_node_t* fs_get_node(uint32_t id);

/**
 * @brief Resolves a path string to a node.
 * Supports absolute paths ("/") and relative paths from start_id.
 * Handles "." and "..".
 */
fs_node_t* fs_find_node(char* path, uint32_t start_id);

/**
 * @brief Updates a node on disk.
 */
int fs_update_node(fs_node_t* node);

/**
 * @brief Helper to find a child by name within a specific parent ID.
 * @return The ID of the child, or 0 if not found.
 */
uint32_t fs_find_node_local_id(uint32_t parent_id, char* name);

/**
 * @brief Creates a new node (File or Directory).
 * Automatically persists changes to disk.
 * @return 1 on success, 0 on failure.
 */
int fs_create_node(uint32_t parent_id, char* name, uint8_t type);

/**
 * @brief Deletes a node by ID.
 * Updates the parent directory to remove the link.
 * @return 1 on success, 0 on failure.
 */
int fs_delete_node(uint32_t id);

/**
 * @brief Gets current disk usage statistics.
 * @param total_kb Pointer to store total disk space in KB
 * @param used_kb Pointer to store used disk space in KB
 * @param free_kb Pointer to store free disk space in KB
 */
void fs_get_disk_stats(uint32_t* total_kb, uint32_t* used_kb, uint32_t* free_kb);

/**
 * @brief Gets cache statistics for monitoring performance.
 * @param cache_size Total cache size
 * @param cached_nodes Number of nodes currently in cache
 * @param dirty_nodes Number of nodes pending write-back
 */
void fs_get_cache_stats(uint32_t* cache_size, uint32_t* cached_nodes, uint32_t* dirty_nodes);

/**
 * @brief Flushes all dirty cache entries to disk.
 * Call this periodically or before shutdown to ensure data persistence.
 */
void fs_sync();

#endif // FS_H
