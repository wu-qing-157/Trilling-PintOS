#ifndef SWAP_TABLE_H
#define SWAP_TABLE_H

#include "../devices/block.h"
#include "../lib/kernel/list.h"

/* FLY's code begin*/
typedef block_sector_t swap_index_t;

void swap_init(void);//initial the swap table
void swap_free(swap_index_t index);//free the swap section
swap_index_t swap_in(void* kpage); //write back to disk
void swap_out(swap_index_t index, void* frame);// load from section to frame

/* FLY's code end*/

#endif