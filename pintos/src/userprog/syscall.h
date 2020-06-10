#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* GLS's code begin */

#include "kernel/list.h"

typedef int fid_t;

/* used to handle syscall related to file. */
struct file_descriptor {
    fid_t id;
    struct file *file;
    struct list_elem elem;
};

void syscall_close_file (struct file_descriptor* f_desc);

/* GLS's code end */

#endif /* userprog/syscall.h */

