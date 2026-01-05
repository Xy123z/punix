#ifndef LOADER_H
#define LOADER_H

#include "types.h"

/**
 * @brief Loads a flat binary program from the filesystem into user memory.
 * 
 * @param path The absolute path to the program executable (e.g., "/bin/hello")
 */
void load_user_program(char* path);

#endif // LOADER_H
