#include <lib/debug.h>
#include "swaptable.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/malloc.h"

#define BLOCK_NUM PGSIZE/BLOCK_SECTOR_SIZE


/* FLY's code begin*/
static struct list swap_free_list;
struct block* swap_block;
swap_index_t tail_index = 0;

void swap_init(void){
  swap_block = block_get_role(BLOCK_SWAP);
  ASSERT(swap_block != NULL);
  list_init(&swap_free_list);
}


void swap_free(swap_index_t index){
  // printf ("swap_free : %d %d %d %d\n", index, BLOCK_NUM, index, index % BLOCK_NUM);
  if(tail_index == index + BLOCK_NUM){
    tail_index = index;
  }
  else{
    struct swap_node* node = malloc(sizeof(struct swap_node));
    node->index = index;
    list_push_back(&swap_free_list, &(node->list_node));
  }
}


swap_index_t swap_in(void* kpage){
  ASSERT(is_kernel_vaddr(kpage));
  swap_index_t free_block = (swap_index_t)-1;
  if(!list_empty(&swap_free_list)){//have a free entry in list
    struct swap_node* res = list_entry(list_front(&swap_free_list),struct swap_node, list_node);
    list_remove(&(res->list_node));
    free_block = res->index;
    free(res);
  }
  else{// no free entry in list 
     if(tail_index + BLOCK_NUM  < block_size(swap_block)){//has enough free sections
       free_block = tail_index;
       tail_index += BLOCK_NUM;
     }
     else{
       return (swap_index_t)-1; //no empty
     }   
  }

  for(int i = 0;i < BLOCK_NUM;++i){
    block_write(swap_block,free_block+i, kpage+i*BLOCK_SECTOR_SIZE);
  }
  return free_block;
}


void swap_out(swap_index_t index, void* frame){
  // printf ("thread: %d\n", thread_current()->tid);
  // printf ("swap_out : %d %d %d %d\n", index, BLOCK_NUM, index, (int)index % BLOCK_NUM);
  ASSERT(is_kernel_vaddr(frame));
  ASSERT (index != (swap_index_t)-1);
  for(int i = 0;i < BLOCK_NUM; ++i){
     // printf ("index: %d\n", index + i);
      block_read(swap_block,index + i, frame+i*BLOCK_SECTOR_SIZE);
  }
  swap_free(index);
}

/* FLY's code end*/