#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* GLS's code begin */
#include "process.h"
#include "pagedir.h"
#include "string.h"
#include "user/syscall.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/pagetable.h"
#define SYSCALL_STDIN_FILENO 0
#define SYSCALL_STDOUT_FILENO 1
/* GLS's code end */

static void syscall_handler (struct intr_frame *);

/* GLS's code begin */
static void syscall_halt (void);
static void syscall_exit (int status);
static pid_t syscall_exec (const char *cmd_line);
static int syscall_wait (pid_t pid);
static bool syscall_create (const char *file, off_t initial_size);
static bool syscall_remove (const char *file);
static int syscall_open (const char *file);
static int syscall_filesize (int fd);
static int syscall_read (int fd, void *buffer, off_t size);
static int syscall_write (int fd, const void *buffer, off_t size);
static void syscall_seek (int fd, off_t position);
static unsigned syscall_tell (int fd);
static void syscall_close (int fd);
static struct file_descriptor* find_file (struct thread *t, fid_t fd);
/* The following functions is used to handle invalid user virtual addres. */
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void read_user (void *uaddr, void *dest, unsigned size);
static bool is_valid_uaddr (void *uaddr);
static bool is_valid_user_string (const char *str);
static bool is_valid_user_buffer (const void* buffer, off_t size);


#ifdef VM
static mmapid_t syscall_mmap(int fd, void *addr);
static void syscall_munmap(mmapid_t id);
static struct mmap_file* find_mmap_file (struct thread *t, mmapid_t id);
#endif
/* GLS's code end */


/* GLS's code begin */
/* As suggested by 4.3.4, only a user process can call into file system at once.
This means the code in the folder 'filesys' is a critical section. */
static struct lock syscall_filesys_lock;
/* GLS's code end */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* GLS's code begin */
  lock_init (&syscall_filesys_lock);
  /* GLS's code end */
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* old code begin */
  // printf ("system call!\n");
  // thread_exit ();
  /* old code end */

  /* GLS's code begin */
#ifdef VM  
  thread_current()->current_esp = f->esp;
#endif   

  int syscall_number = 0;
  read_user(f->esp, &syscall_number, sizeof (int));
  // printf ("syscall_number  %d\n", syscall_number);
  switch (syscall_number)
  {
  case SYS_HALT: {
    syscall_halt();
    break;
  }
  
  case SYS_EXIT: {
    int exit_status;
    read_user (f->esp + sizeof (int), &exit_status, sizeof (exit_status));
    syscall_exit (exit_status);
    break;
  }
  
  case SYS_EXEC: {
    const char *cmd_line;
    read_user (f->esp + sizeof (int), &cmd_line, sizeof (cmd_line));
    if (is_valid_user_string (cmd_line)) {
      f->eax = syscall_exec(cmd_line);
    }
    break;
  }

  case SYS_WAIT: {
    pid_t pid;
    read_user (f->esp + sizeof (int), &pid, sizeof (pid));
    f->eax = syscall_wait (pid);
    break;
  }

  case SYS_CREATE: {
    const char *filename;
    off_t initial_size;
    read_user (f->esp + sizeof (int), &filename, sizeof (filename));
    read_user (f->esp + sizeof (int) + sizeof (filename), &initial_size, sizeof (initial_size));
    if (is_valid_user_string (filename)) {
      f->eax = syscall_create(filename, initial_size);
    }
    break;
  }

  case SYS_REMOVE: {
    const char *filename;
    read_user (f->esp + sizeof (int), &filename, sizeof (filename));
    if (is_valid_user_string (filename)) {
      f->eax = syscall_remove (filename);
    }
    break;
  }

  case SYS_OPEN: {
    const char *filename;
    read_user (f->esp + sizeof (int), &filename, sizeof (filename));
    if (is_valid_user_string (filename)) {
      f->eax = syscall_open (filename);
    }
    break;
  }

  case SYS_FILESIZE: {
    int fd;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    /* save return value to the EAX register */
    f->eax = syscall_filesize(fd);
    break;
  }

  case SYS_READ: {
    int fd;
    void *buffer;
    off_t size;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    read_user (f->esp + sizeof (int) + sizeof (fd), &buffer, sizeof (buffer));
    read_user (f->esp + sizeof (int) + sizeof (fd) + sizeof (buffer), &size, sizeof (size));
    if (is_valid_user_buffer (buffer, size)) {
      f->eax = syscall_read (fd, buffer, size);
    }
    break;
  }

  case SYS_WRITE: {
    int fd;
    const void *buffer;
    off_t size;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    read_user (f->esp + sizeof (int) + sizeof (fd), &buffer, sizeof (buffer));
    read_user (f->esp + sizeof (int) + sizeof (fd) + sizeof (buffer), &size, sizeof (size));
    if (is_valid_user_buffer (buffer, size)) {
      f->eax = syscall_write (fd, buffer, size);
    }
    break;
  }

  case SYS_SEEK: {
    int fd;
    off_t position;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    read_user (f->esp + sizeof (int) + sizeof (fd), &position, sizeof (position));
    syscall_seek (fd, position);
    break;
  }

  case SYS_TELL: {
    int fd;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    f->eax = syscall_tell (fd);
    break;
  }

  case SYS_CLOSE: {
    int fd;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    syscall_close (fd);
    break;
  }

  #ifdef VM
  case SYS_MMAP: {
    int fd;
    void *addr;
    read_user (f->esp + sizeof (int), &fd, sizeof (fd));
    read_user (f->esp + sizeof (int) + sizeof (int), &addr, sizeof (addr));
   // if (is_valid_uaddr (addr)) {
      f->eax = syscall_mmap (fd, addr);
    //}
    break;
  }

  case SYS_MUNMAP: {
    mmapid_t id;
    read_user (f->esp + sizeof (int), &id, sizeof (id));
    syscall_munmap (id);
    break;
  }
  #endif

  default:
    break;
  }
  /* GLS's code end */
}


/* GLS's code begin */
static void 
syscall_halt(void) {
  shutdown_power_off();
}
/* GLS's code end */


/* GLS's code begin */
static void 
syscall_exit (int status) {
  struct thread *current_thread = thread_current();
  if (current_thread->p_desc != NULL) {
    current_thread->p_desc->exit_status = status;
  }
  thread_exit();
  /* thread_exit() will call process_exit(), so no need to call process_exit() */
}
/* GLS's code end */


/* GLS's code begin */
static pid_t 
syscall_exec (const char *cmd_line) {
  /* process_execute() will call load(), which uses file system. */
  lock_acquire (&syscall_filesys_lock);
  pid_t pid = process_execute (cmd_line);
  lock_release (&syscall_filesys_lock);
  return pid;
}
/* GLS's code end */


/* GLS's code begin */
static int 
syscall_wait (pid_t pid) {
  return process_wait (pid);
}
/* GLS's code end */


/* GLS's code begin */
static bool 
syscall_create (const char *file, off_t initial_size) {
  lock_acquire (&syscall_filesys_lock);
  bool create_success = filesys_create(file, initial_size);
  lock_release (&syscall_filesys_lock);
  return create_success;
}
/* GLS's code end */


/* GLS's code begin */
static bool 
syscall_remove (const char *file) {
  lock_acquire (&syscall_filesys_lock);
  bool create_success = filesys_remove(file);
  lock_release (&syscall_filesys_lock);
  return create_success;
}
/* GLS's code end */


/* GLS's code begin */
static int
syscall_open (const char *file) {
  int return_value = -1;
  
  lock_acquire (&syscall_filesys_lock);
  struct file *opened_file = filesys_open(file);
  
  if (opened_file != NULL) {
    struct thread *current_thread = thread_current();
    struct process_descriptor *p_desc = current_thread->p_desc;
    struct file_descriptor *f_desc = palloc_get_page(0);
    if (f_desc == NULL) {
      // printf("[Error] open(): can't palloc a new page.\n");
    }
    else {
      f_desc->file = opened_file;
      f_desc->id = p_desc->opened_count++;
      list_push_back(&(p_desc->opened_files), &(f_desc->elem));
      return_value = f_desc->id;
    }
  }
  lock_release (&syscall_filesys_lock);
  return return_value;
} 
/* GLS's code end */


/* GLS's code begin */
static int 
syscall_filesize (int fd) {
  int return_value = -1;

  struct thread *current_thread = thread_current();
  struct file_descriptor* f_desc = find_file (current_thread, fd);
  if (f_desc != NULL) {
    lock_acquire (&syscall_filesys_lock);
    return_value = file_length(f_desc->file);
    lock_release (&syscall_filesys_lock);
  }

  return return_value;
}
/* GLS's code end */


/* GLS's code begin */
static int 
syscall_read (int fd, void *buffer, off_t size) {
  int return_value = -1;

  if (fd == SYSCALL_STDOUT_FILENO) {
    // printf("[Error] read(): fd is STDOUT_FILEON\n.");
  }
  else if (fd == SYSCALL_STDIN_FILENO) {
    int i;
    for (i = 0; i < size; ++i) {
    //  printf ("put_user\n");
      put_user ((uint8_t*) buffer + i, input_getc());
      return_value = i + 1;
    }
  }
  else {
    struct thread *current_thread = thread_current();
    struct file_descriptor* f_desc = find_file(current_thread, fd);
    if (f_desc != NULL) {
      lock_acquire (&syscall_filesys_lock);
      return_value = file_read (f_desc->file, buffer, size);
      lock_release (&syscall_filesys_lock);
    }
  }

  return return_value;
}
/* GLS's code end */


/* GLS's code begin */
static int 
syscall_write (int fd, const void *buffer, off_t size) {
  int return_value = -1;
  if (fd == SYSCALL_STDIN_FILENO) {
    // printf("[Error] write(): fd is STDIN_FILEON\n.");
  }
  else if (fd == SYSCALL_STDOUT_FILENO) {
    putbuf(buffer, (size_t) size);
    return_value = size;
  }
  else {
    struct thread *current_thread = thread_current();
    struct file_descriptor* f_desc = find_file(current_thread, fd);
    if (f_desc != NULL) {
      lock_acquire (&syscall_filesys_lock);
      return_value = file_write (f_desc->file, buffer, size);
      lock_release (&syscall_filesys_lock);
    }
  }
  return return_value;
}
/* GLS's code end */


/* GLS's code begin */
static void 
syscall_seek (int fd, off_t position) {
  if (fd < 2) {
    // printf("[Error] seek(): fd is STDIN_FILEON or STDOUT_FILEON\n.");
  } 
  else {
    struct thread *current_thread = thread_current();
    struct file_descriptor* f_desc = find_file(current_thread, fd);
    if (f_desc != NULL) {
      lock_acquire (&syscall_filesys_lock);
      file_seek (f_desc->file, position);
      lock_release (&syscall_filesys_lock);
    }
  }
}
/* GLS's code end */


/* GLS's code begin */
static unsigned 
syscall_tell (int fd) {
  int return_value = -1;
  if (fd < 2) {
    // printf("[Error] tell(): fd is STDIN_FILEON or STDOUT_FILEON\n.");
  } 
  else {
    struct thread *current_thread = thread_current();
    struct file_descriptor* f_desc = find_file(current_thread, fd);
    if (f_desc != NULL) {
      lock_acquire (&syscall_filesys_lock);
      return_value = file_tell (f_desc->file);
      lock_release (&syscall_filesys_lock);
    }
  }
  return return_value;
}
/* GLS's code end */


/* GLS's code begin */
/* The following function is used by process to close all files when exiting. */
void 
syscall_close_file (struct file_descriptor *f_desc) {
  if (f_desc != NULL) {
    lock_acquire (&syscall_filesys_lock);
    file_close (f_desc->file);
    palloc_free_page (f_desc);
    lock_release (&syscall_filesys_lock);
  }
}
/* GLS's code end */


/* GLS's code begin */
static void 
syscall_close (int fd) {
  if (fd < 2) {
    // printf("[Error] close(): fd is STDIN_FILEON or STDOUT_FILEON\n.");
  } 
  else {
    struct thread *current_thread = thread_current();
    struct file_descriptor* f_desc = find_file(current_thread, fd);
    if (f_desc != NULL) {
      lock_acquire (&syscall_filesys_lock);
      file_close (f_desc->file);
      list_remove(&(f_desc->elem));
      palloc_free_page (f_desc);
      lock_release (&syscall_filesys_lock);
    }
  }
}
/* GLS's code end */


/* GLS's code begin */
static struct file_descriptor*
find_file (struct thread *t, fid_t fd) {
  if (fd < 2) /* STDIN_FILEON or STDOUT_FILEON */
    return NULL;
  struct list *opened_files = &(t->p_desc->opened_files);
  struct list_elem *file_elem = NULL;
  for (file_elem = list_begin (opened_files); file_elem != list_end (opened_files); 
    file_elem = list_next (file_elem)) {
      struct file_descriptor* tmp = list_entry (file_elem, struct file_descriptor, elem);
      if (tmp->id == fd) {
        return tmp;
      } 
    }
  return NULL;
}
/* GLS's code end */


/* GLS's code begin */
static void
read_user (void *uaddr, void *dest, unsigned size) {
  unsigned i;
  for (i = 0; i < size; ++i) {
    *((uint8_t*) dest + i) = get_user(uaddr + i) & 0xff;
  }
} 
/* GLS's code end */


/* GLS's code begin */
/* The following code is provided by the 
4.1.5(Accessing User Memory)  in the document of pintos.*/

/* Reads a byte at user virutal address UADDR. */
static int
get_user (const uint8_t *uaddr) {
  if (is_valid_uaddr((void*) uaddr)) {
    int res;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (res) : "m" (*uaddr));
    return res;
  }
  return 0;
}

/* Writes BYTE to user virtual address UDST. */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  if (is_valid_uaddr((void*) udst)) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
  }
  return false;
}
/* GLS's code end */


/* GLS's code begin */
/* The following function is used to check whether 
an address is a valid user virtuall address or not */
/* The following cases are invalid. */
/* case 1 : UADDR is a null pointer. */
/* case 2 : UADDR is a pointer to kernel virtual address. */
/* case 3 : UADDR is a pointer to unmapped virtual address. */
static bool
is_valid_uaddr (void *uaddr) {
  if (uaddr != NULL /* case 1 */ && is_user_vaddr (uaddr)  /* case 2 */) {
    /* case 3*/
#ifdef VM
   //printf ("syscall page fault  %0x %0x\n", uaddr, thread_current()->current_esp);
    if ( page_search_with_lock(thread_current()->page_table, pg_round_down(uaddr)) != NULL
      || page_fault_handler(uaddr, false, thread_current()->current_esp) ) {
        return true;
      }
#else
    if (pagedir_get_page(thread_current()->pagedir, uaddr) != NULL) { 
      return true;
    }
#endif
  }
  exit_forcely ();
  return false;
}
/* GLS's code end */


/* GLS's code begin */
static bool 
is_valid_user_string (const char *str) {
  if (!is_valid_uaddr ((void*) str))
    return false;
  unsigned i = 0;
  while (*(str + i) != '\0') {
    ++i;
    if (((int) (str + i) & PGMASK) == 0 && !is_valid_uaddr ((void*) str + i)) {
      /* only need to check the beginning of a page. */
      return false;
    }
  }
  return true;
}
/* GLS's code end */


/* GLS's code begin */
static bool 
is_valid_user_buffer (const void* buffer, off_t size) {
  if (!is_valid_uaddr ((void*) buffer))
    return false;
  int i;
  for (i = 1; i < size; ++i) {
    if ((((int) buffer + i) & PGMASK) == 0 && !is_valid_uaddr ((void*) buffer + i)) {
      return false;
    }
  }
  return true;
}
/* GLS's code end */


/* GLS's code begin */
/* release the filesys lock and exit with -1. */
void 
exit_forcely (void) {
  if (lock_held_by_current_thread (&syscall_filesys_lock)) {
    lock_release (&syscall_filesys_lock);
  }
  syscall_exit (-1);    
}
/* GLS's code end */


/* GLS's code begin */
#ifdef VM
/* Check if can install PAGE_NUM pages stared at ADDR in PAGE_TABLE */
bool 
page_available_mmap (page_table_type *page_table, int page_num, void *addr) {
  int i;
  for (i = 0; i < page_num; ++i, addr += PGSIZE) {
    if (!page_table_available (page_table, addr)) {
      return false;
    }
  }
  return true;
}
/* GLS's code end */


/* GLS's code begin */
/* Install PAGE_NUM pages stared at ADDR in PAGE_TABLE */
bool 
page_install_mmap (page_table_type *page_table, int page_num, struct mmap_file *mmap_f) {
  int i;
  void *addr = mmap_f->addr;
  for (i = 0; i < page_num; ++i, addr += PGSIZE) { 
    //printf("page_table_install_file:  page_table:%0x  addr:%0x\n", page_table, addr);
    if (!page_table_install_file (page_table, mmap_f, addr)) {
      return false;
    }
  }
  return true;
}
/* GLS's code end */


/* GLS's code begin */
static mmapid_t 
syscall_mmap (int fd, void* addr) {
  if (addr == NULL || ((uint32_t) addr & PGMASK) || fd == 0 || fd == 1) {
    return -1;
  }

  lock_acquire (&syscall_filesys_lock);  

  struct thread *current_thread = thread_current();
  struct file_descriptor *file_d = find_file (current_thread, fd);
  struct file* reopened_file = NULL;
  uint32_t file_bytes = 0;
  if (file_d != NULL && file_d->file != NULL) {
    reopened_file = file_reopen (file_d->file);  
    if (reopened_file != NULL) {
      file_bytes = file_length (reopened_file);
    }
  }
  if (file_d == NULL || file_d->file == NULL || reopened_file == NULL || file_bytes == 0) {
   lock_release (&syscall_filesys_lock);
    return -1;  
  }

  int page_num = (file_bytes + PGSIZE - 1) / PGSIZE;
  page_table_type *page_table = current_thread->page_table;
  if (page_available_mmap (page_table, page_num, addr)) {
    struct mmap_file *mmap_f = malloc (sizeof (struct mmap_file));
    mmap_f->id = current_thread->mmap_count++;
    mmap_f->addr = addr;
    mmap_f->file = reopened_file;
    mmap_f->file_bytes = file_bytes;
    mmap_f->zero_bytes = 0;
    mmap_f->ofs = 0;
    mmap_f->writable = true;
    mmap_f->static_data = false;
    if (page_install_mmap (page_table, page_num, mmap_f)) {
      list_push_back (&(current_thread->mmap_list), &(mmap_f->elem));
      lock_release (&syscall_filesys_lock);
      return mmap_f->id;
    }
    else {
      free (mmap_f);
    }
  }

  lock_release (&syscall_filesys_lock);
  return -1;
}
/* GLS's code end */


/* GLS's code begin */
static void
syscall_munmap(mmapid_t id) {
  struct thread *current_thread = thread_current();
  struct mmap_file *mmap_f = find_mmap_file (current_thread, id);
  if (mmap_f != NULL) {
   lock_acquire (&syscall_filesys_lock);
    int i, page_num = (mmap_f->file_bytes + mmap_f->zero_bytes + PGSIZE - 1) / PGSIZE;
    void *addr = mmap_f->addr;  
    for (i = 0; i < page_num; ++i, addr += PGSIZE) {
      page_table_unstall_file(current_thread->page_table, addr);
    }
    file_close (mmap_f->file);
    list_remove(&(mmap_f->elem));
    free (mmap_f);
   lock_release (&syscall_filesys_lock);
  }
}
/* GLS's code end */


/* GLS's code begin */
void
syscall_munmap_file(struct mmap_file *mmap_f) {
  struct thread* current_thread = thread_current();
  lock_acquire (&syscall_filesys_lock);
  int i, page_num = (mmap_f->file_bytes + mmap_f->zero_bytes + PGSIZE - 1) / PGSIZE;
  void *addr = mmap_f->addr;  
  for (i = 0; i < page_num; ++i, addr += PGSIZE) {
    page_table_unstall_file(current_thread->page_table, addr);
  }
  if (mmap_f->file != current_thread->p_desc->own_file)
   file_close (mmap_f->file);
  list_remove(&(mmap_f->elem));
  free (mmap_f);
  lock_release (&syscall_filesys_lock);
}
/* GLS's code end */


/* GLS's code begin */
void 
read_page_from_file (struct mmap_file *mmap_f, void *upage, void *kpage) {
  void *file_end = mmap_f->addr + mmap_f->file_bytes;
  void *zero_end = file_end + mmap_f->zero_bytes;
 //bool held = lock_held_by_current_thread (&syscall_filesys_lock);
 //if (!held)
 //  lock_acquire (&syscall_filesys_lock);
  if (upage < file_end) {
    if (file_end - upage < PGSIZE) {
      /* read page from the last page of file. */
      int last_page = mmap_f->file_bytes % PGSIZE;
      file_read_at (mmap_f->file, kpage, last_page, mmap_f->ofs + upage - mmap_f->addr);
      memset (kpage + last_page, 0, PGSIZE - last_page);
    }
    else {
      file_read_at (mmap_f->file, kpage, PGSIZE, mmap_f->ofs + upage - mmap_f->addr);
    }
  } 
  else if (mmap_f->zero_bytes > 0) {
    if (upage < zero_end) {
      memset (kpage, 0, PGSIZE);
    }
  }
 // if (!held)
 //  lock_release (&syscall_filesys_lock);
}
/* GLS's code end */


/* GLS's code begin */
void write_page_to_file (struct mmap_file *mmap_f, void *upage, void *kpage) {
  if (mmap_f->writable) {
    void *file_end = mmap_f->addr + mmap_f->file_bytes;
   //bool held = lock_held_by_current_thread (&syscall_filesys_lock);
   //if (!held)
   //  lock_acquire (&syscall_filesys_lock);
    if (upage < file_end) {
      if (file_end - upage < PGSIZE) {
        /* read page from the last page of file. */
        int last_page = mmap_f->file_bytes % PGSIZE;
        file_write_at (mmap_f->file, kpage, last_page, mmap_f->ofs + upage - mmap_f->addr);
      }
      else {
        file_write_at (mmap_f->file, kpage, PGSIZE, mmap_f->ofs + upage - mmap_f->addr);
      }
    } 
     //if (!held)
     // lock_release (&syscall_filesys_lock);
  }
}
/* GLS's code end */


/* GLS's code begin */
static struct mmap_file* 
find_mmap_file (struct thread *t, mmapid_t id) {
  struct list *mmap_list = &(t->mmap_list);
  struct list_elem *mmap_elem = NULL;
  for (mmap_elem = list_begin (mmap_list); mmap_elem != list_end (mmap_list); 
    mmap_elem  = list_next (mmap_elem)) {
      struct mmap_file* tmp = list_entry (mmap_elem, struct mmap_file, elem);
      if (tmp->id == id) {
        return tmp;
      } 
    }
  return NULL;
}
#endif
/* GLS's code end */
