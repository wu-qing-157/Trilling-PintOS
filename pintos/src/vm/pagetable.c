#include "pagetable.h"
#include "frametable.h"
#include "swaptable.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../userprog/syscall.h"
#include "../lib/stddef.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../threads/vaddr.h"
#include  "../threads/synch.h" //for lock

#define INST_LENGTH       32
#define PAGE_STACK_SIZE	  0x800000  //limit the stack size be 8MB
#define STACK_BOTTOM_LINE (PHYS_BASE - PAGE_STACK_SIZE)

unsigned page_table_hash(const  struct hash_elem* e, void *aux);
bool page_table_less(const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux);
bool page_table_accessible(page_table_type* page_table, void* upage);
void page_table_destroy_frames (struct hash_elem *e, void *aux);              


/*FLY's code begin */
static struct lock page_table_lock;
void page_table_lock_init(void){
  //printf ("# page_table_lock_init.\n");
  lock_init(&page_table_lock);
 // printf ("lock_init %d %d\n", page_table_lock.semaphore.value, list_size(&(page_table_lock.semaphore.waiters)));
}


/* Basic life cycle. */
page_table_type *page_table_create(void){
  //printf ("# page_table_create.\n");
  //printf("current_thread: %d\n", thread_current()->tid);
  //if (page_table_lock.holder != NULL)
  //printf ("lock_holder %d\n", (page_table_lock.holder)->tid);
 
  lock_acquire(&page_table_lock);
  page_table_type *table = malloc(sizeof(page_table_type));
  if(table != NULL){
    if(hash_init(table, page_table_hash , page_table_less, NULL)) {
      lock_release(&page_table_lock);
      return table;
    }
    else {
      free(table);
      lock_release(&page_table_lock);
      return NULL;
    }
  }
  lock_release(&page_table_lock);
  return NULL;
}


void page_table_destroy(page_table_type* page_table){
 // printf ("# page_table_destory.\n");
  lock_acquire(&page_table_lock);
 // printf ("page_table_destroy:%0x\n", page_table);
  hash_destroy(page_table, page_table_destroy_frames);
 // printf ("hash_destroy end.\n");
  lock_release(&page_table_lock);
  //printf ("page_table_destroy end.\n");
}


/* Search */
struct page_table_node* page_search(page_table_type* page_table, void* upage){
 // printf ("# page_search.\n");
  struct hash_elem *element;
  struct page_table_node ptn;

  ASSERT(page_table != NULL);
  ptn.key = upage;
  element = hash_find(page_table, &(ptn.hash_node));
  if(element != NULL)
    return hash_entry(element, struct page_table_node, hash_node);
  else 
    return NULL;
}


struct page_table_node* page_search_with_lock(page_table_type* page_table, void* upage){
  //printf ("# page_search_with_lock.\n");
  lock_acquire(&page_table_lock);
  struct page_table_node* res = page_search(page_table,upage);
  lock_release(&page_table_lock);
  return res;
}


bool page_table_available(page_table_type *page_table, void *upage){
  return upage < STACK_BOTTOM_LINE && page_search(page_table,upage) == NULL;
}


/*if upage entry is enpty, then kpage is the value*/
bool page_table_install_frame(void* upage, void* kpage, bool writable){
 // printf ("page_table_install_frame.\n");
  struct thread* thr = thread_current();
  page_table_type *page_table = thr->page_table;
  
  bool success = false;

  lock_acquire(&page_table_lock);
  struct page_table_node* node = page_search(page_table, upage);
  if(node == NULL){
    node = malloc(sizeof(*node));
    //printf ("page_table_create: %0x %0x\n", upage, kpage);
    node->key = upage;
    node->value = kpage;
    node->status = Frame;
    node->mmap_f = NULL;
    node->writable = writable;
    hash_insert(page_table,&(node->hash_node));
    success = true;
  }
  lock_release(&page_table_lock);
  if(success){
    uint32_t* pagedir = thr->pagedir;
    ASSERT(pagedir_set_page(pagedir, upage, kpage, writable));
  }
  return success;
}


bool page_table_install_file(page_table_type *page_table, struct mmap_file *mmap_f, void *upage){
 // printf ("page_table_install_file.\n");
  //struct thread* thr = thread_current();
  //printf ("held %d\n", lock_held_by_current_thread(&page_table_lock));
  bool success = false;
 // printf("current_thread: %d\n", thread_current()->tid);
 // if (page_table_lock.holder != NULL)
 // printf ("lock_holder %d\n", (page_table_lock.holder)->tid);
  //printf ("waiter %d %d\n", page_table_lock.semaphore.value, list_size(&(page_table_lock.semaphore.waiters)));
  lock_acquire(&page_table_lock);
  //printf("install lock_acquire\n");
  if(page_table_available(page_table,upage)){
    struct page_table_node *node =  malloc(sizeof(struct page_table_node));
    //printf("page_table_available %d\n", sizeof(*node));
    //struct page_table_node *
    //printf("malloc end.\n");
    node->key = upage;
    node->value =  mmap_f;
    node->status = File;
    node->writable = mmap_f->writable;
    node->mmap_f =  mmap_f;
    //printf("hash_insert begin\n");
    hash_insert(page_table, &(node->hash_node));
    //printf("hash_insert end.\n");
    success = true;
  }
  lock_release(&page_table_lock);
  //printf("install lock_release %d\n", (page_table_lock.holder));
  return success;
}


bool page_table_unstall_file(page_table_type *page_table, void *upage){
///  printf ("page_table_unstall_file.\n");
  struct thread *thr = thread_current();
  bool success = false;
  lock_acquire(&page_table_lock);
  if(page_table_accessible(page_table,upage)){
    struct page_table_node *node = page_search(page_table,upage);
    ASSERT(node != NULL);
    if(node->status == File){
      hash_delete(page_table, &(node->hash_node));
      free(node);
      success = true;
    }
    else if(node->status == Frame){
    //  printf ("in frame!\n");
      uint32_t* pagedir = thr->pagedir;
      if(pagedir_is_dirty(pagedir, node->key))
        write_page_to_file(node->mmap_f,node->key,node->value);
      pagedir_clear_page(pagedir, node->key);
      hash_delete(page_table, &(node->hash_node));
      frame_table_free_frame(node->value);
      free(node);
      success = true;
    }
  }
  lock_release(&page_table_lock);
  return success;
}


bool evict_page_to_file(struct thread *cur, void *upage){
  struct page_table_node *node = page_search(cur->page_table, upage);
  bool success =  false;
  ASSERT(node != NULL);
  if(node->status == Frame){
      ASSERT(node->mmap_f != NULL);
      node->value = node->mmap_f;
      node->status = File;
      pagedir_clear_page(cur->pagedir, upage);
      success = true;
  }
  return success;
}


bool evict_page_to_swap(struct thread *cur, void *upage, swap_index_t index){
  struct page_table_node *node = page_search(cur->page_table, upage);
  bool success =  false;
  ASSERT(node != NULL);
  if(node->status == Frame){
      node->value = (void*) index;
      node->status = Swap;
      pagedir_clear_page(cur->pagedir, upage);
      success = true;
  }
  return success;
}


void page_table_destroy_frames (struct hash_elem *e, void *aux){
  struct page_table_node *entry =  hash_entry(e,struct page_table_node, hash_node);
  if(entry->status  == Frame){
    pagedir_clear_page(thread_current()->pagedir, entry->key);
 //   printf ("destroy_frame %0x %0x\n", entry->key, entry->value);
    frame_table_free_frame(entry->value);
  }
  else if(entry->status == Swap){
    swap_free((swap_index_t) entry->value);
  //   printf ("destroy_swap %0x %d\n", entry->key, entry->value);
  }
  free(entry);
}



bool page_fault_handler(const void* vaddr, bool write, void* esp){
  // printf ("# page_fault_handler %d\n", thread_current()->tid);
  /*vaddr is the virtual address requested*/
  /* write is whether the request needs to  write*/
  /*esp is the stack point when the request occurs*/
  // printf ("# page_fault_handler, %0x, %d, %0x, %d\n", vaddr, write, esp, is_user_vaddr(vaddr));
  
  struct thread *cur_thread = thread_current();
  page_table_type *table = cur_thread->page_table;

  if (table == NULL)
    return false;

  uint32_t *pagedir = cur_thread->pagedir;
  void* upage = pg_round_down(vaddr);/*virtual page number*/

  //printf ("page_fault_handler %x %x %x\n", vaddr, table, upage);
  lock_acquire(&page_table_lock);
  //printf ("page_search begin.\n");
  struct page_table_node *node = page_search(table, upage);//node in page table
  //printf ("page_search end  %x\n", node);
  
  ASSERT(!(node != NULL && node->status == Frame));//in frame won't occur page fault

  if(write == true && node != NULL && node->writable == false){  // permission conflict
    lock_release (&page_table_lock);
   // printf ("page_fault_handler end  %d\n", (page_table_lock.holder));
    return false;
  }

  void* frame = NULL;

//printf ("before success.\n");
  bool success = false;
  if(upage >= STACK_BOTTOM_LINE){ 
  //  printf ("up stack.\n");
    if(vaddr >= esp - INST_LENGTH) {//else it is  a invalid address
      if(node == NULL){
        frame = frame_table_get_frame(PAL_USER, upage);
        if(frame != NULL){//find it in frame table! 
          node = malloc(sizeof(*node));//add a new entry in page table
          node->key = upage;
          node->value = frame;
          node->status = Frame;
          node->writable = true;
          node->mmap_f = NULL;
          hash_insert(table,&(node->hash_node));
          success = true;
        }
      }
      else{//it is in page table
        if(node->status == Swap){//in swap slot
          frame =  frame_table_get_frame(PAL_USER, upage);
          if(frame != NULL){// has a new frame to use
            swap_out((swap_index_t)node->value, frame);
            node->value = frame;
            node->status = Frame;
            success = true;
          }
        }
      }
    }
  }
  else{
   // printf ("else.\n");
    if(node != NULL){
      if(node->status == Swap){
  //      printf ("in frame.\n");
        frame = frame_table_get_frame(PAL_USER, upage);
        if(frame != NULL){
          swap_out((swap_index_t)node->value, frame);
          node->value = frame;
          node->status = Frame;
          success = true;
        }
      }
      else if(node->status == File){
     //   printf ("in file.\n");
        frame = frame_table_get_frame(PAL_USER , upage);
   //    printf("create frame: %0x\n", frame);
        if(frame != NULL){
       //   printf ("read_page_from_file begin  %0x\n", upage);
          read_page_from_file(node->value, upage, frame);
        //  printf ("read_page_from_file end.\n");
          node->value = frame;
          node->status = Frame;
          success = true;
        }
      }
    }
  }

  frame_set_not_referenced(frame);
  lock_release(&page_table_lock);

 // printf ("page_fault_handler end  %d\n", (page_table_lock.holder));
  if(success) {
    pagedir_set_page(pagedir,node->key,node->value,node->writable);
  }
  return success;
}


/*utils function*/
bool page_table_accessible(page_table_type* page_table, void* upage){
  return upage< STACK_BOTTOM_LINE  &&  page_search(page_table,upage) != NULL;
}

/* hash table utils function*/
unsigned page_table_hash(const struct hash_elem* e, void* aux){
  struct page_table_node * entry = hash_entry(e,struct page_table_node, hash_node);
  return hash_bytes(&(entry->key),  sizeof(entry->key));
}


bool page_table_less(const struct hash_elem *a,const struct hash_elem *b, void *aux){
  struct page_table_node * entry_a = hash_entry(a,struct page_table_node, hash_node);
  struct page_table_node * entry_b = hash_entry(b,struct page_table_node, hash_node);
  return entry_a->key  <  entry_b->key;
}
/*FLY's code end */

