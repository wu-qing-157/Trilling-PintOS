#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"

/* GXY's code begin */

#define BLOCK_CACHE_SIZE 64

struct cache_entry {
  block_sector_t sector;
  uint8_t data[BLOCK_SECTOR_SIZE];
  bool occupied;
  bool dirty;
  // used in clock replacement
  bool accessed;
};

void cache_init(void);
void cache_flush(void);
void cache_read(block_sector_t sector, void *dest);
void cache_read_at(block_sector_t sector, void *dest, size_t start, size_t cnt);
void cache_write(block_sector_t sector, const void *src);
void cache_write_at(block_sector_t sector, const void *src, size_t start, size_t cnt);

/* GXY's code end */

#endif
