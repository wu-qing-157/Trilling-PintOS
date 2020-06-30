#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include "../lib/stdbool.h"
#include "../threads/palloc.h"
#include "../lib/kernel/hash.h"

/*FLY's code begin*/

struct frame_table_node{
  void* frame; 
  void* upage;
  struct thread* thr; //every thread processes a frame_table
  bool referenced; /* referenced: this round will not be replaced */
  struct hash_elem hash_node;
  struct list_elem list_node;
};
/* replacement strategy: clock*/

void frame_table_init(void);//init in thread_init
void* frame_table_get_frame(enum palloc_flags flag, void* upage);
void frame_table_free_frame(void* frame);
void* frame_search(void* frame);
bool frame_set_not_referenced(void* frame);

/*FLY's code end*/
#endif