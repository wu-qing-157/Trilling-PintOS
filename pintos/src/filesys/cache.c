#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include <string.h>
#include <debug.h>
#include <stdio.h>

/* GXY's code begin */

static struct lock cache_lock;

static struct cache_entry cache[BLOCK_SECTOR_SIZE];

void cache_init(void) {
  lock_init(&cache_lock);
  lock_acquire(&cache_lock);
  for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    cache[i].occupied = false;
  lock_release(&cache_lock);
}

static void cache_flush_one(struct cache_entry *entry) {
  if (entry->dirty) {
    block_write(fs_device, entry->sector, entry->data);
    entry->dirty = false;
  }
}

// Flush everything in block buffer
void cache_flush(void) {
  lock_acquire(&cache_lock);
  for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    if (cache[i].occupied)
      cache_flush_one(cache + i);
  lock_release(&cache_lock);
}

static struct cache_entry *cache_lookup(block_sector_t sector) {
  for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    if (cache[i].occupied && cache[i].sector == sector)
      return cache + i;
  return NULL;
}

static struct cache_entry *cache_evict(void) {
  static int clock = 0;
  while (true) {
    if (!cache[clock].occupied) return cache + clock;
    if (cache[clock].accessed) cache[clock].accessed = false;
    else break;
    clock = (clock + 1) % BLOCK_CACHE_SIZE;
  }
  cache_flush_one(cache + clock);
  cache[clock].occupied = false;
  return cache + clock;
}

static struct cache_entry *cache_lookup_or_evict(block_sector_t sector, bool need_read) {
  struct cache_entry *entry = cache_lookup(sector);
  if (entry == NULL) {
    entry = cache_evict();
    entry->sector = sector;
    if (need_read) block_read(fs_device, sector, entry->data);
    entry->occupied = true;
    entry->dirty = false;
    entry->accessed = false;
  }
  return entry;
}

void cache_read(block_sector_t sector, void *dest) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = cache_lookup_or_evict(sector, true);
  entry->accessed = true;
  memcpy(dest, entry->data, BLOCK_SECTOR_SIZE);
  lock_release(&cache_lock);
}

void cache_read_at(block_sector_t sector, void *dest, size_t start, size_t cnt) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = cache_lookup_or_evict(sector, true);
  entry->accessed = true;
  memcpy(dest, entry->data + start, cnt);
  lock_release(&cache_lock);
}

void cache_write(block_sector_t sector, const void *src) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = cache_lookup_or_evict(sector, false);
  entry->accessed = true;
  entry->dirty = true;
  memcpy(entry->data, src, BLOCK_SECTOR_SIZE);
  lock_release(&cache_lock);
}

void cache_write_at(block_sector_t sector, const void *src, size_t start, size_t cnt) {
  lock_acquire(&cache_lock);
  struct cache_entry *entry = cache_lookup_or_evict(sector, cnt < BLOCK_SECTOR_SIZE);
  entry->accessed = true;
  entry->dirty = true;
  memcpy(entry->data + start, src, cnt);
  lock_release(&cache_lock);
}

/* GXY's code end */
