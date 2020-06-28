#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* GXY's code begin */
#define DIRECT_CNT 1
#define INDIRECT_CNT 0
#define DOUBLY_CNT 30
#define INDIRECT_LENGTH ((int32_t) (BLOCK_SECTOR_SIZE / sizeof(block_sector_t)))
/* GXY's code end */

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    /* old code begin */
    // block_sector_t start;               /* First data sector. */
    /* old code end */
    /* GXY's code begin */
    block_sector_t direct[DIRECT_CNT];
    block_sector_t indirect[INDIRECT_CNT];
    block_sector_t doubly[DOUBLY_CNT];
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    /* old code begin */
    // uint32_t unused[125];               /* Not used. */
    /* old code end */
    /* GXY's code begin */
    size_t allocated;
    uint32_t unused[(BLOCK_SECTOR_SIZE - (DIRECT_CNT + INDIRECT_CNT + DOUBLY_CNT) * sizeof(block_sector_t) - sizeof(off_t) - sizeof(unsigned) - sizeof(size_t)) / sizeof(uint32_t)];
    /* GXY's code end */
  };

/* GXY's code begin */
struct indirect_disk {
  block_sector_t sectors[INDIRECT_LENGTH];
};
/* GXY's code end */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* GXY's code begin */

// Get the sector based on inode on disk and the position
static block_sector_t inode_member(const struct inode *inode_, off_t pos) {
  const struct inode_disk *inode = &inode_->data;

  if (pos < DIRECT_CNT) return inode->direct[pos];
  pos -= DIRECT_CNT;
  if (pos < INDIRECT_CNT * INDIRECT_LENGTH) {
    int indirect_pos = pos / INDIRECT_LENGTH;
    block_sector_t ret;
    cache_read_at(inode->indirect[indirect_pos], &ret, (pos % INDIRECT_LENGTH) * sizeof(block_sector_t), sizeof(block_sector_t));
    return ret;
  }
  pos -= INDIRECT_CNT * INDIRECT_LENGTH;
  if (pos < DOUBLY_CNT * INDIRECT_LENGTH * INDIRECT_LENGTH) {
    int doubly_pos = pos / INDIRECT_LENGTH / INDIRECT_LENGTH;
    int indirect_pos = pos % (INDIRECT_LENGTH * INDIRECT_LENGTH) / INDIRECT_LENGTH;
    int final_pos = pos % INDIRECT_LENGTH;
    block_sector_t indirect, ret;
    cache_read_at(inode->doubly[doubly_pos], &indirect, indirect_pos * sizeof(block_sector_t), sizeof(block_sector_t));
    cache_read_at(indirect, &ret, final_pos * sizeof(block_sector_t), sizeof(block_sector_t));
    // struct indirect_disk doubly, indirect;
    // cache_read(inode->doubly[doubly_pos], &doubly);
    // cache_read(doubly.sectors[indirect_pos], &indirect);
    return ret;
  }
  ASSERT(false && "too large offset");
}

// Release sectors in [start, start + cnt)
static void sectors_release_at(block_sector_t *start, size_t cnt) {
  for (block_sector_t *i = start; i < start + cnt; i++)
    free_map_release(*i, 1);
}

// Allocate cnt sectors, saving them in [start, start + cnt)
// returns true iff success
static bool sectors_allocate_at(block_sector_t *start, size_t cnt) {
  block_sector_t *cur_start = start;
  size_t single = cnt;
  while (true) {
    if (free_map_allocate(single, cur_start)) {
      static char zeros[BLOCK_SECTOR_SIZE];
      for (size_t i = 0; i < single; i++)
        cache_write(cur_start[i] = cur_start[0] + i, zeros);
      cur_start += single;
      size_t delta = start + cnt - cur_start;
      if (delta == 0) return true;
      if (single > delta) single = delta;
    } else {
      single >>= 1;
      if (single == 0) {
        sectors_release_at(start, cur_start - start);
        return false;
      }
    }
  }
}

// Release sector number [inode->allcated, allocated) in inod
// used when allocation failed
static void inode_undo_allocate(struct inode *inode_, size_t allocated) {
  struct inode_disk *inode = &inode_->data;
  block_sector_t sector = inode_->sector;

  if (inode->allocated < DIRECT_CNT) {
    size_t direct_cnt = allocated < DIRECT_CNT ? allocated : DIRECT_CNT;
    sectors_release_at(inode->direct + inode->allocated, direct_cnt - inode->allocated);
  }

  for (block_sector_t *indirect = inode->indirect; indirect < inode->indirect + INDIRECT_CNT; indirect++) {
    size_t start = DIRECT_CNT + (indirect - inode->indirect) * INDIRECT_LENGTH;
    size_t end = start + INDIRECT_LENGTH;
    if (inode->allocated >= end) continue;
    if (allocated <= start) break;
    size_t from = inode->allocated < start ? 0 : inode->allocated - start;
    size_t to = allocated - start;
    if (to > INDIRECT_LENGTH) to = INDIRECT_LENGTH;
    struct indirect_disk indirect_d;
    cache_read_at(*indirect, indirect_d.sectors + from, from, to - from);
    sectors_release_at(indirect_d.sectors + from, to - from);
    if (inode->allocated <= start) sectors_release_at(indirect, 1);
  }

  for (block_sector_t *doubly = inode->doubly; doubly < inode->doubly + DOUBLY_CNT; doubly++) {
    size_t doubly_start = DIRECT_CNT + INDIRECT_CNT * INDIRECT_LENGTH + (doubly - inode->doubly) * INDIRECT_LENGTH * INDIRECT_LENGTH;
    size_t doubly_end = doubly_start + INDIRECT_LENGTH * INDIRECT_LENGTH;
    if (inode->allocated >= doubly_end) continue;
    if (allocated <= doubly_start) break;
    struct indirect_disk doubly_d;
    cache_read(*doubly, &doubly_d);
    for (block_sector_t *indirect = doubly_d.sectors; indirect < doubly_d.sectors + INDIRECT_LENGTH; indirect++) {
      size_t start = doubly_start + (indirect - doubly_d.sectors) * INDIRECT_LENGTH;
      size_t end = start + INDIRECT_LENGTH;
      if (inode->allocated >= end) continue;
      if (allocated <= start) break;
      size_t from = inode->allocated < start ? 0 : inode->allocated - start;
      size_t to = allocated - start;
      if (to > INDIRECT_LENGTH) to = INDIRECT_LENGTH;
      struct indirect_disk indirect_d;
      cache_read_at(*indirect, indirect_d.sectors + from, from, to - from);
      sectors_release_at(indirect_d.sectors + from, to - from);
      if (inode->allocated <= start) sectors_release_at(indirect, 1);
    }
    if (inode->allocated <= doubly_start) sectors_release_at(doubly, 1);
  }

  cache_write(sector, inode);
}

// If not enough yet, allocate more to ensure inode to have at least target sectors
// returns true iff success
static bool inode_ensure_sectors(struct inode *inode_, size_t target) {
  struct inode_disk *inode = &inode_->data;
  block_sector_t sector = inode_->sector;

  if (inode->allocated >= target) return true;

  size_t allocated = inode->allocated;

  if (inode->allocated < DIRECT_CNT) {
    size_t direct_cnt = target < DIRECT_CNT ? target : DIRECT_CNT;
    if (!sectors_allocate_at(inode->direct + inode->allocated, direct_cnt - inode->allocated))
      return false;
    allocated = direct_cnt;
  }

  if (allocated == target) {
    inode->allocated = target;
    cache_write(sector, inode);
    return true;
  }

  for (block_sector_t *indirect = inode->indirect; indirect < inode->indirect + INDIRECT_CNT && allocated < target; indirect++) {
    size_t start = DIRECT_CNT + (indirect - inode->indirect) * INDIRECT_LENGTH;
    size_t end = start + INDIRECT_LENGTH;
    if (inode->allocated >= end) continue;
    struct indirect_disk indirect_d;
    if (inode->allocated <= start) {
      if (!sectors_allocate_at(indirect, 1)) {
        inode_undo_allocate(inode_, allocated);
        return false;
      }
    }
    size_t from = inode->allocated < start ? 0 : inode->allocated - start;
    size_t to = target - start;
    if (to > INDIRECT_LENGTH) to = INDIRECT_LENGTH;
    if (sectors_allocate_at(indirect_d.sectors + from, to - from)) {
      cache_write_at(*indirect, indirect_d.sectors + from, from * sizeof(block_sector_t), (to - from) * sizeof(block_sector_t));
      allocated = start + to;
    } else {
      if (inode->allocated <= start) sectors_release_at(indirect, 1);
      inode_undo_allocate(inode_, allocated);
      return false;
    }
  }

  if (allocated == target) {
    inode->allocated = target;
    cache_write(sector, inode);
    return true;
  }

  for (block_sector_t *doubly = inode->doubly; doubly < inode->doubly + DOUBLY_CNT && allocated < target; doubly++) {
    size_t doubly_start = DIRECT_CNT + INDIRECT_CNT * INDIRECT_LENGTH + (doubly - inode->doubly) * INDIRECT_LENGTH * INDIRECT_LENGTH;
    size_t doubly_end = doubly_start + INDIRECT_LENGTH * INDIRECT_LENGTH;
    if (inode->allocated >= doubly_end) continue;
    struct indirect_disk doubly_d;
    if (inode->allocated <= doubly_start) {
      if (!sectors_allocate_at(doubly, 1)) {
        inode_undo_allocate(inode_, allocated);
        return false;
      }
    } else {
      cache_read(*doubly, &doubly_d);
    }
    for (block_sector_t *indirect = doubly_d.sectors; indirect < doubly_d.sectors + INDIRECT_LENGTH && allocated < target; indirect++) {
      size_t start = doubly_start + (indirect - doubly_d.sectors) * INDIRECT_LENGTH;
      size_t end = start + INDIRECT_LENGTH;
      if (inode->allocated >= end) continue;
      struct indirect_disk indirect_d;
      if (inode->allocated <= start) {
        if (!sectors_allocate_at(indirect, 1)) {
          if (inode->allocated <= doubly_start) sectors_release_at(doubly, 1);
          inode_undo_allocate(inode_, allocated);
          return false;
        }
      }
      size_t from = inode->allocated < start ? 0 : inode->allocated - start;
      size_t to = target - start;
      if (to > INDIRECT_LENGTH) to = INDIRECT_LENGTH;
      if (sectors_allocate_at(indirect_d.sectors + from, to - from)) {
        cache_write_at(*indirect, indirect_d.sectors + from, from * sizeof(block_sector_t), (to - from) * sizeof(block_sector_t));
        allocated = start + to;
      } else {
        if (inode->allocated <= doubly_start) sectors_release_at(doubly, 1);
        if (inode->allocated <= start) sectors_allocate_at(indirect, 1);
        inode_undo_allocate(inode_, allocated);
        return false;
      }
    }
    cache_write(*doubly, &doubly_d);
  }

  ASSERT(allocated == target);
  inode->allocated = allocated;
  cache_write(sector, inode);
  return true;
}

// If not enough yet, allocate more to ensure inode to have inode->length bytes
static bool inode_ensure_length(struct inode *inode) {
  return inode_ensure_sectors(inode, bytes_to_sectors(inode->data.length));
}

// Release all sectors in this inode
static void inode_release(struct inode *inode) {
  size_t cnt = inode->data.allocated;
  inode->data.allocated = 0;
  inode_undo_allocate(inode, cnt);
}

// Extend inode's length up to specified length
static bool inode_extend(struct inode *inode, off_t length) {
  off_t old_length = inode->data.length;
  if (old_length >= length) return true;
  inode->data.length = length;
  if (inode_ensure_length(inode)) {
    cache_write_at(inode->sector, &length, offsetof(struct inode_disk, length), sizeof(length));
    return true;
  } else {
    inode->data.length = old_length;
    return false;
  }
}

static void inode_print(struct inode *inode) {
  static uint8_t buffer[75678];
  inode_read_at(inode, buffer, inode->data.length, 0);
  puts("");
  printf("current %05x: ", inode->sector);
  for (int i = 0; i < inode->data.length; i++) printf("%02x", buffer[i]);
  puts("");
}

/* GXY's code end */

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    /* old code begin */
    // return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    /* old code end */
    /* GXY's code begin */
    return inode_member(inode, pos / BLOCK_SECTOR_SIZE);
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  //printf("create %05x %d\n", sector, length);
  /* old code begin */
  // struct inode_disk *disk_inode = NULL;
  /* old code end */
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof(struct inode_disk) == BLOCK_SECTOR_SIZE);

  /* old code begin */
  // disk_inode = calloc (1, sizeof *disk_inode);
  /* old code end */
  /* GXY's code begin */
  struct inode *inode = calloc(1, sizeof(struct inode));
  if (inode != NULL && inode) {
    inode->data.length = length;
    inode->data.magic = INODE_MAGIC;
    inode->sector = sector;

    if (inode_ensure_length(inode)) {
      cache_write_at(inode->sector, &length, offsetof(struct inode_disk, length), sizeof(length));
      success = true;
    }
    free(inode);
  }
  return success;
  /* GXY's code end */

  /* old code begin */
  // if (disk_inode != NULL)
  //   {
  //     size_t sectors = bytes_to_sectors (length);
  //     disk_inode->length = length;
  //     disk_inode->magic = INODE_MAGIC;
  //     if (free_map_allocate (sectors, &disk_inode->start)) 
  //       {
  //         /* old code begin */
  //         // block_write (fs_device, sector, disk_inode);
  //         /* old code end */
  //         /* GXY's code begin */
  //         cache_write(sector, disk_inode);
  //         /* GXY's code end */
  //         if (sectors > 0) 
  //           {
  //             static char zeros[BLOCK_SECTOR_SIZE];
  //             size_t i;
              
  //             for (i = 0; i < sectors; i++) 
  //               /* old code begin */
  //               // block_write (fs_device, disk_inode->start + i, zeros);
  //               /* old code end */
  //               /* GXY's code begin */
  //               cache_write(disk_inode->start + 1, zeros);
  //               /* GXY's code end */
  //           }
  //         success = true; 
  //       } 
  //     free (disk_inode);
  //   }
  /* old code end */
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  // printf("open %05x\n", sector);
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /* old code begin */
  // block_read (fs_device, inode->sector, &inode->data);
  /* old code end */
  /* GXY's code begin */
  cache_read(inode->sector, &inode->data);
  /* GXY's code end */
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  // printf("reopen %05x\n", inode->sector);
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  // printf("close %05x\n", inode->sector);
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          /* old code begin */
          // free_map_release (inode->data.start,
                            // bytes_to_sectors (inode->data.length)); 
          /* old code end */
          /* GXY's code begin */
          inode_release(inode);
          /* GXY's code end */
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  // printf("remove %05x\n", inode->sector);
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  //printf("read %05x %d %d \n", inode->sector, size, offset);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  /* old code begin */
  // uint8_t *bounce = NULL;
  /* old code end */

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          /* old code begin */
          // block_read (fs_device, sector_idx, buffer + bytes_read);
          /* old code end */
          /* GXY's code begin */
          cache_read(sector_idx, buffer + bytes_read);
          /* GXY's code end */
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          /* old code begin */
          // if (bounce == NULL) 
          //   {
          //     bounce = malloc (BLOCK_SECTOR_SIZE);
          //     if (bounce == NULL)
          //       break;
          //   }
          // block_read (fs_device, sector_idx, bounce);
          // memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
          /* old code end */
          /* GXY's code begin */
          cache_read_at(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
          /* GXY's code end */
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  /* old code begin */
  // free (bounce);
  /* old code end */

  // for (int i = 0; i < bytes_read; i++) printf("%02x", ((uint8_t *) buffer_)[i]);
  // puts(" ok");
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  // printf("before write  ");
  // inode_print(inode);
  // printf("writing at [%d, %d): ", offset, offset + size);
  // for (int i = 0; i < size; i++) printf("%02x", ((const uint8_t *) buffer_)[i]);
  // puts("");
  // printf("write %05x %d %d: \n", inode->sector, size, offset);
  // for (int i = 0; i < size; i++) printf("%02x", ((const uint8_t *) buffer_)[i]);
  // puts("");
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  /* old code begin */
  // uint8_t *bounce = NULL;
  /* old code end */

  if (inode->deny_write_cnt)
    return 0;

  /* GXY's code begin */
  if (!inode_extend(inode, offset + size))
    return 0;
  /* GXY's code end */

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          /* old code begin */
          // block_write (fs_device, sector_idx, buffer + bytes_written);
          /* old code end */
          /* GXY's code begin */
          cache_write(sector_idx, buffer + bytes_written);
          /* GXY's code end */
        }
      else 
        {
          /* old code begin */
          /* We need a bounce buffer. */
          // if (bounce == NULL) 
          //   {
          //     bounce = malloc (BLOCK_SECTOR_SIZE);
          //     if (bounce == NULL)
          //       break;
          //   }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          // if (sector_ofs > 0 || chunk_size < sector_left) 
            // block_read (fs_device, sector_idx, bounce);
          // else
            // memset (bounce, 0, BLOCK_SECTOR_SIZE);
          // memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // block_write (fs_device, sector_idx, bounce);
          /* old code end */
          /* GXY's code begin */
          cache_write_at(sector_idx, buffer + bytes_written, sector_ofs, chunk_size);
          /* GXY's code end */
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  /* old code begin */
  // free (bounce);
  /* old code end */

  //inode_print(inode);
  //static uint8_t buff[10000];
  //inode_read_at(inode, buff, bytes_written, offset - bytes_written);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
