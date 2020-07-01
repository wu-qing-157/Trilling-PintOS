#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;

/* Opening and closing directories. */
bool dir_create (block_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, block_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

/* yy's code begin */
//bool subdir_create(struct dir* current_dir, char* subdir_name);
struct dir* subdir_lookup(struct dir* current_dir, char* subdir_name);
bool subdir_delete(struct dir* current_dir, char* subdir_name);

bool subfile_create(struct dir* current_dir, char* file_name, off_t initial_size);
struct file* subfile_lookup(struct dir* current_dir, char* file_name);
bool subfile_delete(struct dir* current_dir, char* file_name);

bool inode_isdir(struct inode* inode);
bool is_dirfile(struct file_descriptor* f_desc);
/* yy's code end */


#endif /* filesys/directory.h */
