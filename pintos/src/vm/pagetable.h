#ifndef SUPPLEMENTAL_PAGE_TABLE
#define SUPPLEMENTAL_PAGE_TABLE

#include "../lib/stdint.h"
#include "../lib/kernel/hash.h"
#include "../userprog/syscall.h"
#include "threads/thread.h"
#include "swaptable.h"

/*FLY's code begin */
enum page_status{
  Frame, Swap, File
};

typedef struct hash page_table_type;

struct page_table_node{
  void* key, //virtual address
  *value; 
  /* physical address : status = frame
    swap slot index: status = swap
    file mapid: status =  file
  */
  struct mmap_file* mmap_f;
  bool writable;
  enum page_status status;
  struct hash_elem hash_node;
};


/* page table entry:
|-----------------------------------------------------------------------------------|
|  key: virtual page number |  value:framenumber, swapslot,file | writable | status |
|-----------------------------------------------------------------------------------|

*/

void page_table_lock_init(void); //OK

/* Basic life cycle. */
page_table_type *page_table_create(void);//OK
void page_table_destroy(page_table_type* page_table); //OK

/*search*/
struct page_table_node* page_search(page_table_type* page_table, void* upage); //OK
struct page_table_node* page_search_with_lock(page_table_type* page_table, void* upage); //OK

/*interfaces*/
bool page_table_available(page_table_type *page_table, void *upage);//OK
bool page_table_install_frame(void* upage, void* kpage,bool writable); //OK
bool page_table_install_file(page_table_type *page_table, struct mmap_file *mmap_f, void *upage);//OK
bool page_table_unstall_file(page_table_type *page_table, void *upage);//OK
bool evict_page_to_file(struct thread *cur, void *upage);//OK
bool evict_page_to_swap(struct thread *cur, void *upage, swap_index_t index);//OK

/* page fault */
bool page_fault_handler(const void* vaddr, bool write, void* esp);

/*FLY's code end */

#endif