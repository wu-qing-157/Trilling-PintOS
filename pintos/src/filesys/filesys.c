#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* yy's code begin */
#include "threads/thread.h"
#include "user/syscall.h"
#include "lib/string.h"
/* yy's code end */

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  /* GXY's code begin */
  cache_init();
  /* GXY's code end */

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();

  /* GXY's code begin */
  cache_flush();
  /* GXY's code end */
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  /*
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;*/

  /* yy's code begin */
  struct dir *dir;
  char *file_name = malloc(READDIR_MAX_LEN + 1);
  bool is_dir;

  if (strlen(name) > 0 && parse_path(name, &dir, &file_name, &is_dir)) {
    ASSERT(dir != NULL);
    ASSERT(file_name != NULL);
    if (is_dir) {
      dir_close(dir);
      free(file_name);
      return false;
    }
    bool suc = subfile_create(dir, file_name, initial_size);
    dir_close(dir);
    free(file_name);
    return suc;
  } else {
    free(file_name);
    return false;
  }
  /* yy's code end */
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  /* yy's code begin */
  struct dir *dir;
  char *file_name = malloc(READDIR_MAX_LEN + 1);
  bool is_dir;

  if (strlen(name) > 0 && is_rootpath(name)) {
    struct file* file = file_open(inode_open(ROOT_DIR_SECTOR));
    file_set_dir(file, dir_open_root());
    free(file_name);
    return file;
  } else if (strlen(name) > 0 && parse_path(name, &dir, &file_name, &is_dir)) {
    ASSERT(dir != NULL);
    ASSERT(file_name != NULL);
    
    struct file* tmp;
    if (is_dir) {
      struct dir* target_dir;
      target_dir = subdir_lookup(dir, file_name);
      tmp = file_open(inode_reopen(dir_get_inode(target_dir))); // I did not understand.
      file_set_dir(tmp, dir_reopen(dir));
      dir_close(target_dir);
    } else {
      struct file* target_file;
      target_file = subfile_lookup(dir, file_name);
      tmp = target_file;
    }
    dir_close(dir);
    free(file_name);
    return tmp;
  } else {
    free(file_name);
    return NULL;
  }
  /* yy's code end */

  /*
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
  */
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char *file_name = malloc(READDIR_MAX_LEN + 1);
  bool is_dir;

  if (strlen(name) > 0 && parse_path(name, &dir, &file_name, &is_dir)) {
    ASSERT(dir != NULL);
    ASSERT(file_name != NULL);
    if (is_dir) {
      bool suc = subdir_delete(dir, file_name);
      dir_close(dir);
      free(file_name);
      return suc;
    } else {
      bool suc = /* subdir_delete(dir, file_name) ||*/ subfile_delete(dir, file_name); // What's the first half ?
      dir_close(dir);
      free(file_name);
      return suc;
    }
  } else {
    free(file_name);
    return false;
  }
  
  /*
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
  */
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* yy's code begin */

/* Check whether a path is rootpath. */
// NOTEHERE: this function did not deal with such symbols as .., ., or //
bool
is_rootpath(const char* path) {
  if (path == NULL) return false;
  if (path[0] == '/' && path[1] == '\0') return true;
  return false;
}

/* Check whether name of a file is of valid length
   and does not contain '/' */
bool
check_name(const char* name) {
  if (name == NULL) return false;
  for (int i = 0; i < READDIR_MAX_LEN + 1; ++i) {
    if (name[i] == '\0') return true;
    if (name[i] == '/') return false;
  }
  return false;
}


/* Readin a path(not a root path), judge whether it is a directory or a file.
   Also find its parent directory and its name. */
// NOTEHERE: this function did not deal with such symbols as .. . //
// and I am curious about where they are handeled in ymt's code
bool
parse_path(const char* path, struct dir** parent_dir, char** name, bool* is_dir) {
  *is_dir = false;

  if (is_rootpath(path)) 
    return false;

  int l = strlen(path);
  if (l == 0) return false;
  if (l > 1 && path[l - 1] == '/') {
    *is_dir = true;
  }

  if (path[0] == '/')
    *parent_dir = dir_open_root();
  else
    *parent_dir = dir_reopen(thread_current()->current_dir);

  char* cpy_path = malloc(l + 1);
  strlcpy(cpy_path, path, l + 1);

  char* ptr;
  char* result = strtok_r(cpy_path, "/", &ptr);
  while (result != NULL) {
    if (!check_name(result)) {
      *is_dir = false;
      free(cpy_path);
      return false;
    }
    char* next = strtok_r(NULL, "/", &ptr);
    if (next == NULL) {
      strlcpy(*name, result, READDIR_MAX_LEN + 1);
      break;      
    } else {
      struct dir* tmp = *parent_dir;
      *parent_dir = subdir_lookup(*parent_dir, result);
      dir_close(tmp);
      if (*parent_dir == NULL){
        free(cpy_path);
        return false;
      }
    }
  }
  free(cpy_path);
  return true;
}

/* yy's code end */