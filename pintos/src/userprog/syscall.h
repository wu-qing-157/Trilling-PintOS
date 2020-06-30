#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* GLS's code begin */

#include "kernel/list.h"
#include "kernel/hash.h"

typedef int fid_t;

/* used to handle syscall related to file. */
struct file_descriptor {
    fid_t id;
    struct file *file;
    struct list_elem elem;
};

void syscall_close_file (struct file_descriptor* f_desc);
void exit_forcely (void);

#ifdef VM

typedef int mmapid_t;

struct mmap_file {
    mmapid_t id;
    void *addr;
    struct file* file;
    uint32_t file_bytes;
    uint32_t zero_bytes; 
    uint32_t ofs;
    bool writable;
    bool static_data;
    struct list_elem elem;
};

void syscall_munmap_file (struct mmap_file *mmap_f);
void read_page_from_file (struct mmap_file *mmap_f, void *upage, void *kpage);
void write_page_to_file (struct mmap_file *mmap_f, void *upage, void *kpage);
bool page_available_mmap (struct hash *page_table, int page_num, void *addr);
bool page_install_mmap (struct hash *page_table, int page_num, struct mmap_file *mmap_f);
#endif

/* GLS's code end */

#endif /* userprog/syscall.h */

