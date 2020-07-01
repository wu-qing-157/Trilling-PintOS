#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
/* yy's code begin */
#include "filesys/directory.h"
/* yy's code end */

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

/* yy's code begin */
bool is_rootpath(const char* path);
bool check_name(const char* name);
bool parse_path(const char* path, struct dir** parent_dir, char** name, bool* is_dir);
/* yy's code end */

#endif /* filesys/filesys.h */
