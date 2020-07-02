#include "../lib/kernel/hash.h"
#include "swaptable.h"
#include "frametable.h"
#include"../threads/thread.h"
#include"../userprog/pagedir.h"
#include"../threads/malloc.h"
#include"../lib/debug.h"
#include "pagetable.h"
#include "../lib/stddef.h"
#include "../lib/string.h"
#include "devices/block.h"
#include "../threads/vaddr.h"
#include "../lib/stdio.h"
#include "../userprog/syscall.h"
#include "../threads/synch.h"

/*FLY's code begin*/

/* all user process pages are saved in the frame_table*/
static struct hash frame_table;
/* those frames to be substituted*/
static struct list frame_clock;
static struct lock frame_lock;
struct frame_table_node*  clock_hand;


unsigned frame_table_hash (const struct hash_elem *e, void *aux);
bool frame_table_hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void* pick_frame_to_eviction(void);
void frame_table_clock_hand_inc(void);
void frame_table_clock_hand_dec(void);


/* find the frame in hash table*/
void* frame_search(void* frame){
  struct frame_table_node to_search;
  struct hash_elem * hash_node;
  to_search.frame  = frame;
  hash_node = hash_find(&frame_table, &(to_search.hash_node));
  if(hash_node == NULL)
    return NULL;
  return hash_entry(hash_node, struct frame_table_node, hash_node);
}


/*initial the static frame table*/
void frame_table_init(void){
  hash_init(&frame_table, frame_table_hash, frame_table_hash_less,NULL);
  list_init(&frame_clock);
  lock_init(&frame_lock);
  clock_hand = NULL;
}


/*free a frame in the table */
void frame_table_free_frame(void* frame){
  lock_acquire(&frame_lock);
  struct frame_table_node* frame_to_free = frame_search(frame);
  if(frame_to_free == NULL)
    PANIC("cannot find the frame to free~");

  if(!frame_to_free->referenced){
    if(clock_hand == frame_to_free){
      if(list_size(&frame_clock) == 1){
        clock_hand = NULL;
      }
      else frame_table_clock_hand_inc();
    }
    list_remove(&frame_to_free->list_node);
  }

  hash_delete(&frame_table, &(frame_to_free->hash_node));
  free(frame_to_free);
  palloc_free_page(frame);
  lock_release(&frame_lock);
}


/*get page from user pool*/
void* frame_table_get_frame(enum palloc_flags flag, void* upage){
  lock_acquire(&frame_lock);
  void* new_frame = palloc_get_page(PAL_USER | flag);
  if(new_frame == NULL){
    new_frame = pick_frame_to_eviction();
    if(flag & PAL_ZERO) {// flag == pal_zero
      memset(new_frame,0,PGSIZE);
    }
    else if(flag & PAL_ASSERT){
      PANIC("palloc assertion when getting frame in frame table.");
    }
  }
  if(new_frame == NULL){
    return NULL;
  }

  struct frame_table_node* item = (struct frame_table_node*) malloc(sizeof(struct frame_table_node));
  item->frame = new_frame;
  item->upage = upage;
  item->thr = thread_current();
  item->referenced = true;

  hash_insert(&frame_table, &(item->hash_node));
  lock_release(&frame_lock);
  return new_frame;
}


bool frame_set_not_referenced(void* frame){
  lock_acquire(&frame_lock); /* when writting data we  should give it a lock*/
  struct frame_table_node* node = frame_search(frame);
  if(node == NULL){
    lock_release(&frame_lock);
    return false;
  }
  if(node->referenced == false){
    lock_release(&frame_lock);
    return true;
  }

  node->referenced = false;
  
  list_push_back(&frame_clock, &node->list_node);
  if (list_size (&frame_clock) == 1) {
    clock_hand = node;
  }
  
  lock_release(&frame_lock);
  return true;
}


/* use the replace strategy to get a frame */
void* pick_frame_to_eviction(void){
  ASSERT(clock_hand != NULL); //else we needn't to replace

  /* find the page to be replaced */
  while(pagedir_is_accessed(clock_hand->thr->pagedir,clock_hand->upage)){
    pagedir_set_accessed(clock_hand->thr->pagedir,clock_hand->upage,false);
    frame_table_clock_hand_inc();
    ASSERT(clock_hand != NULL);
  } 

  struct frame_table_node *get_frame_node = clock_hand;
  void* get_frame = get_frame_node->frame;
  swap_index_t index = (swap_index_t)-1;
  struct page_table_node* node = page_search(get_frame_node->thr->page_table, get_frame_node->upage);
  ASSERT(node != NULL);
  if(node->mmap_f == NULL|| 
  ((node->mmap_f))->static_data){
    index = swap_in(get_frame_node->frame);
   // // printf ("pick one to swap  %d\n", index);
    ASSERT(evict_page_to_swap(get_frame_node->thr, get_frame_node->upage, index));
  }
  else{
    write_page_to_file(node->mmap_f, get_frame_node->upage, get_frame);
    ASSERT(evict_page_to_file(get_frame_node->thr,get_frame_node->upage));
  }

  list_remove(&get_frame_node->list_node);
  if(list_empty(&frame_clock))
    clock_hand = NULL;
  else frame_table_clock_hand_inc();
  hash_delete(&frame_table, &get_frame_node->hash_node);
  free(get_frame_node);
  return get_frame;
}


/*clock hand dec and inc */
void frame_table_clock_hand_dec(void){//point to previous frame
  ASSERT(clock_hand != NULL);
  if(list_size(&frame_clock) == 1) return;
  if(&clock_hand->list_node == list_front(&frame_clock)){
    clock_hand = list_entry(
      list_tail(&frame_clock),struct frame_table_node, list_node); //tail in a list does not contains data
  }
  clock_hand = list_entry(
    list_prev(&clock_hand->list_node),struct frame_table_node, list_node);
}


void frame_table_clock_hand_inc(void){//point to next frame
  ASSERT(clock_hand != NULL);
  if(list_size(&frame_clock) == 1) return;
  if(&clock_hand->list_node == list_back(&frame_clock)){
    clock_hand = list_entry(
      list_front(&frame_clock),struct frame_table_node, list_node); //tail in a list does not contains data
    return;
  }
  clock_hand = list_entry(
    list_next(&clock_hand->list_node),struct frame_table_node, list_node);
}


/*hashtable util function*/
unsigned frame_table_hash (const struct hash_elem *e, void *aux){
  struct frame_table_node* node = hash_entry(e, struct frame_table_node, hash_node);
  return hash_bytes(&node->frame, sizeof(node->frame));
}


bool frame_table_hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux){
  struct frame_table_node* nodea = hash_entry(a, struct frame_table_node, hash_node);
  struct frame_table_node* nodeb = hash_entry(b, struct frame_table_node, hash_node);
  return nodea->frame < nodeb->frame;
}

/*FLY's code end*/