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
#include "filesys/filesys.h"
#include "filesys/file.h"
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
static void exit_forcely (void);
/* GLS's code end */


/* GLS's code begin */
/* As suggested by 4.3.4, only a user process can call into file system at once.
This means the code in the folder 'filesys' is a cirtical section. */
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
  int syscall_number = 0;
  read_user(f->esp, &syscall_number, sizeof (int));
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
  if (uaddr == NULL                                         || /* case 1 */
    !is_user_vaddr (uaddr)                                  || /* case 2 */
    pagedir_get_page(thread_current()->pagedir, uaddr) == NULL /* case 3*/) { 
      exit_forcely ();
  }
  return true;
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
static void 
exit_forcely (void) {
  if (lock_held_by_current_thread (&syscall_filesys_lock)) {
    lock_release (&syscall_filesys_lock);
  }
  syscall_exit (-1);    
}
/* GLS's code end */
