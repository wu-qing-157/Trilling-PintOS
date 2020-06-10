#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* GLS's code begin */
#include "threads/synch.h"
#define PID_INIT ((pid_t) -1)
typedef int pid_t;
/* GLS's code end */

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* GLS's code begin */
struct process_descriptor {
    pid_t pid;
    struct thread *current_thread;
    struct thread *parent_thread;
    char *cmd;     // The command of this process being executed.
    bool waited;   // the process is waited by the parent process or not. 
    bool exited;   // the process have exited or not.
    int exit_status;
    struct list_elem elem;
    bool load_success;
    struct semaphore load_sema;
    struct semaphore wait_sema;
};
/* GLS's code end */

#endif /* userprog/process.h */

