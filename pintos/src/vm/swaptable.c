#include "lib/debug.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "swaptable.h"

const uint32_t SECTOR_NUMBER = PGSIZE / BLOCK_SECTOR_SIZE;

/* FLY's code begin*/

struct block *swap_block;
struct bitmap *swap_map;
swap_index_t tail_index = 0;

void swap_init (void) {
  swap_block = block_get_role(BLOCK_SWAP);
  swap_map = bitmap_create (block_size (swap_block) / SECTOR_NUMBER);
  // uint32_t blocksize = block_size (swap_block);
  // printf ("bitmap_set_all %d %d %d\n", blocksize, SECTOR_NUMBER, blocksize / SECTOR_NUMBER);
  bitmap_set_all (swap_map, false);
}

void swap_free (swap_index_t index) {
  ASSERT (bitmap_test (swap_map, index));
  // printf ("swap_free %d %d\n", index, block_size (swap_block) / SECTOR_NUMBER);
  bitmap_set (swap_map, index, false);
}

swap_index_t swap_in (void *kpage) {
  ASSERT (is_kernel_vaddr (kpage));
  swap_index_t index = bitmap_scan (swap_map, 0, 1, false);
  // printf ("swap_in %u %d\n", index, block_size (swap_block) / SECTOR_NUMBER);
  bitmap_set (swap_map, index, true);
  uint32_t i;
  for (i = 0; i < SECTOR_NUMBER; ++i) {
    block_write (swap_block, index * SECTOR_NUMBER + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  return index;
}

void swap_out (swap_index_t index, void *kpage) {
  ASSERT (is_kernel_vaddr (kpage));
  ASSERT (bitmap_test (swap_map, index));
  uint32_t i;
  for (i = 0; i < SECTOR_NUMBER; ++i) {
    block_read (swap_block, index * SECTOR_NUMBER + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  // printf ("swap_out %d %d\n", index, block_size (swap_block) / SECTOR_NUMBER);
  bitmap_set (swap_map, index, false);
}

/* FLY's code end*/