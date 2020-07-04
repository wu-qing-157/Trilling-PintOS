## Group Work

The work of each group member is briefly described as follows:

| Who    | Work                              |
| ------ | --------------------------------- |
| 顾逸   | Project 1, Some part of Project 4 |
| 郭林松 | Project 2, Some part of Project 3 |
| 傅凌玥 | Most part of Project 3            |
| 姚远   | Most part of Project 4            |

## Features

+ Due to the implementation of buffer cache, we remove all the bounce buffer in the original project, and only copy the needed part in the block, saving lots of time when writing small-scaled data or slightly modifying the directory structure.
+ We add two entry `.` and `..` to all directories, causing the path parser to be as simple as possible. In this way, we may easily guarantee the path parser will work correctly when dealing with strange paths such as `/.././../../a/./../..`. Also, it is convenient to add another syscall requiring these two special entries like what `ls -a` does.
+  We add a special read/write to `dest`/`src` when using cache, preventing a user mode page fault happening when the cache is working (which may cause another cache operation, which may corrupt the cache).

## Project 1: Threads

#### Task 1. Alarm Clock

This task is quite simple. We add a list to save sleeping thread, and keep track of the wakeup time as a member of `struct thread`. Every time tick, we check the list to find all threads to wakeup.

#### Task 2. Priority Scheduler

Firstly, we check whether to yield the current thread when creating a thread, unblocking a thread, or modifying the priority of a thread. When `lock_release` or something similar is called, we will choose the thread with maximum priority in the waiter list.

Then it's time to deal with the priority donation. We add the following members to `struct thread`.

```c
struct thread {
    struct list donating;
    int raw_priority;
    struct list_elem donating_elem;
    struct thread *waiting;
};
```

Among the newly-add members, donating is a list tracking who is donating to this thread, while waiting keeps the thread this thread is donating to.

Every time a `lock_acquire` is called, if the calling thread has to wait another, it donate its priority to that thread. Every time a `lock_release` is called, all thread donating to it undo the donation, and if a new thread is waken up, all other threads start donating the newly-waken-up thread.

When a donation happens, we change the priority of the receiver. If the receiver's priority increases by this operation, do the donation recursively. Similarly, if a donation undo causes the receiver's priority to decrease, update the priority recursively.

#### Task 3. Advanced Scheduler

The implementation of advanced scheduler is exactly the same as described in the PintOS document. We update the `load_avg` and the `recent_cpu` in `thread_tick`, and check whether to yield the CPU.

## Project 2: User Programs

In pintos, each user process has only one thread.  

### Basic Structure
In `process.h`, I define `struct process_descriptor` which contains all information of a user process. 
```c
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
```c
struct thread {
   ...
   struct process_descriptor *p_desc;
   struct list child_process;
};   
```
- `p_desc`: A pointer points to the process which corresponds to the thread. 
- `child_process`: A list used to store all child processes of the thread.

Some system calls are related to file system, so I define `struct file_descriptor` in `syscall.h` to describe the information of a opened file.

```c
struct file_descriptor {
    fid_t id;
    struct file *file;
    struct list_elem elem;
};
```



#### Task 1. Process Termination Messages

Print `exit_status` of the current process when exiting.

#### Task 2. Argument Passing

In the task, we firstly spill the command line into file name and arguments. For example, if the command line is `echo x y`, then we spill this to `echo`,`x` and `y`. In this case, file name is `echo` and arguments are `x` and `y`. Next, we need to pass arguments when starting process. See  **4.5 80x86 Calling Convention** in pintos document to understand how to pass arguments! 

#### Task 3. Accessing User Memory

The task may be the most difficult part of project 2, because we need to understand some concepts of pintos like  **user virtual address, user virtual address, page, pagedir** , etc.

When system call, the kernel accesses memory through pointers provided by a user program. If a user virtual address `uaddr` satisfies the following cases, then `uaddr` is invalid:

- `uaddr` is a null pointer. 

- `uaddr` is a pointer to kernel virtual address. 

  For this case, `vaddr.h` provides a function `is_user_vaddr()`.

- `uaddr` is a pointer to unmapped virtual address.

  For this case, `pagedir.h` provides a function `pagedir_get_page()` to check whether `uaddr` is unmapped or not.

#### Task 4. System Calls

For this task, the most difficult system call may be `wait(pid_t pid)`, which makes the current thread to wait its child process `pid` to exit. I implement this by using a semaphore `wait_sema`: The current process tries to down `wait_sema`, and process `pid` up `wait_sema` when exiting.

There are some system calls which needs to call into the file system. As suggested by **4.3.4** of the pintos document, only a user process can call into file system at once. This means that the code in the folder `filesys` is a critical section. So we define `syscall_filesys_lock` in `syscall.c`. For example, if we want to open a file, then pintos requires the lock before opening the file and releases the lock after closing the file.

#### Task 5. Denying Writes to Executables

ELF executable files can't be modified by any user processes. For example, If `echo` is being executed by some user process, then it can't be modified. `file.h` provides `file_deny_write()` and `file_allow_write()`.  So we only need to deny writes when starting process and allow writes when exiting process.


## Project 3: Virtual Memory

#### Task 1. Supplemental Page Table   

Each process maintains a supplementary page table which maps user virtual page to actual location. Each entry holds `key`, `value`, ` status` and `writable` information.  The information `key`  represents the virtual page and `value` represents the actual location. There are three kinds of location for each virtual page:

- *Frame*: The virtual page is in physical memory, then the member `value` holds its physical frame.
- *File*: The virtual page is in the file system, then the member `value` holds its corresponding file.
- *Swap*: The virtual page is in swap table, then the member `value` holds its swap index.

When a page fault occurs in the system, the virtual page will be searched in the supplementary page table. If the page can be found,  the supplementary page table will request a physical frame and copy the page from its actual location (swap table or file system) to the requested frame.  If there is an error (such as the page is not found or write permission conflicts), it will force the current process to exit with -1.

#### Task 2. Frame Table

This table holds the global physical memory allocation. The entry for each frame holds its virtual address in `upage` and the thread to which it belongs in `thr`. 

All processes request user frames through the frame table. If a blank frame is available, assign it directly. If there are no idle frames, the frame table uses the clock algorithm to select and replace a frame in the table. The replaced frame will be evicted to swap table or file system. If the replaced frame is from mmaped files, then it will be evicted to file system. Otherwise it will be evicted to swap table. The only exception is when the replaced frame is from the static data segment. Although it's  from ELF executable file, it will be evicted to swap table instead of file system.

#### Task 3. Swap Table

Swap table holds some functions for page and disk swap interactions. PintOS can write pages on the disk swap area to a frame by the function `swap_out`, or it can write a page into the swap area by the function `swap_in`.

The free information on the disk is maintained with a bitmap, and the free swap block can be found by using `bitmap_scan`.

## Project 4: File Systems

#### Task 1. Buffer Cache

All read/write operations to the `fs_device` is captured by the cache. The cache uses the clock replacement policy, and has 64 available entries.

#### Task 2. Indexed and Extensible Files

An `inode` can have several direct blocks,  several indirect blocks, and several doubly indirect blocks, the amount of which can be controlled by three macros. The blocks are used in the above order, to provide quick access to smaller files.

Several functions are added to support extending the file or releasing the unnecessary blocks.

Thanks to the implementation of buffer cache, we now support to write a block partly to the file system, so any bounce buffer is no longer needed.

#### Task 3. Subdirectory

In the basic file system, all files live in a single directory, and we'll now replace it by a hierarchical name space. To implement this, we call function `parse_directory` to parse a directory. For the special file names `.` and `..`, two special files named "." and ".." are added whenever we add a new directory, pointing to the current directory and parent directory, respectively. Directory information is stored in  `inode`  disk. Some functions arer added in sys_call for the support of system call.





