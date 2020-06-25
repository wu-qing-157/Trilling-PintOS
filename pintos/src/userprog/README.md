# PintOS Project 2

The porject 2 is based on initial project 1. In pintos, each user process has only one thread.  

## Basic Structure
In `process.h`, I define `struct process_descriptor` which contains all informations of a user process. 
```
struct process_descriptor {
    pid_t pid;
    char *cmd;
    struct thread *current_thread;
    struct thread *parent_thread;
    bool waited;
    bool exited;
    int exit_status;
    struct list_elem elem;
    struct file* own_file;  
    struct list opened_files;
    int opened_count;
    bool load_success;
    struct semaphore load_sema;
    struct semaphore wait_sema;
};
```
- `pid`: The identifier of the process.
-  `cmd`: The command line of the process. For example, `cmd` = 'echo x'.
-  `current_thread`: Each user process has only one thread. 
-  `waited` : Whether the process is being waited by its parent process or not.
-  `exited`: Whether the process has exited or not.
-  `exit_status`: The exit code of the process.
- `own_file`: A pointer to ELF executable file of the thread. For examples, if pintos is executing `echo x y z`, the `own_file` points to the file `echo`.
- `opend_files`: A list used to store all files opened by the process. (doesn't contains ELF executable file)
- `opened_count`: How many files are opened by the process now. The initial `opened_count` is 2 because each process has `STDIN` and `STDOUT`.
-  `load_success`: Whether ELF executable file is loaded successfully by loader in `process.c`.
-  `load_sema`: Make parent process wait its child process to load successfully.
-  `wait_sema`: Make parent process wait its child process to exit.

I need to add some members in `struct thread`.
```
struct thread {
   ...
   struct process_descriptor *p_desc;
   struct list child_process;
};   
```
- `p_desc`: A pointer points to the process which corresponds to the thread. 
- `child_process`: A list used to store all child processes of the thread.

Some system calls are related to file system, so I define `struct file_descriptor` in `syscall.h` to describe the information of a opened file.

```
struct file_descriptor {
    fid_t id;
    struct file *file;
    struct list_elem elem;
};
```

## Task 1. Process Termination Messages

Print `exit_status` of the current process when exiting.

## Task 2. Argument Passing

In the task, we firstly spill the command line into file name and arguments. For example, if the command line is `echo x y`, then we spill this to `echo`,`x` and `y`. In this case, file name is `echo` and arguments are `x` and `y`. Next, we need to pass arguments when starting process. See  **4.5 80x86 Calling Convention** of the pintos document to understanding how to pass arguments! 

## Task 3. Accessing User Memory

The task may be the most difficult part of project 2, because we need to understand some concepts of pintos like  **user virtual address, user virtual address, page, pagedir** , etc.

When system call, the kernel accesses memory through pointers provided by a user program. If a user virtual address `uaddr` satisfies the following cases, then `uaddr` is invalid and it may be dangerous for kernel uses it. 

- `uaddr` is a null pointer. 

- `uaddr` is a pointer to kernel virtual address. 

  For this case, `vaddr.h` provides a function `is_user_vaddr()`.

- `uaddr` is a pointer to unmapped virtual address.

  For this case, `pagedir.h` provides a function `pagedir_get_page()` to check whether `uaddr` is unmapped or not.

## Task 4. System Calls

For this task, the most difficult system call may be `wait(pid)`, which makes the current thread to wait its child process `pid` to exit. I implement this by using a semaphore `wait_sema`: The current process tries to down `wait_sema`, and process `pid` up `wait_sema` when exiting.

There are some system calls which needs to call into the file system. As suggested by **4.3.4** of the pintos document, only a user process can call into file system at once. This means that the code in the folder `filesys` is a critical section. So we define `syscall_filesys_lock` in `syscall.c`. For example, if we want to open a file, then pintos requires the lock before opening the file and releases the lock after closing the file.

## Task 5. Denying Writes to Executables.

ELF executable files can't be modified by any user processes. For example, If `echo` is being executed by some user process, then it can't be modified. `file.h` provides `file_deny_write()` and `file_allow_write()`.  So we only need to deny writes when starting process and allow writes when exiting process.