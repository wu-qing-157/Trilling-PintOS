# Trilling-PintOS

## Worklist

Work|Who|Status
---|---|---
__Threads__||
Alarm Clock|GXY|Pass Test
Priority Scheduling|GXY|Pass Test
Advanced Scheduler||
__User Programs__||
Process Termination Messages||
Argument Passing||
Accessing User Memory||
System Calls||
Denying Writes to Executables||
__Virtual Memory__||
Paging||
Stack Growth||
Memory Mapped Files||
Accessing User Memory||
__File Systems__||
Indexed and Extensible Files||
Subdirectories||
Buffer Cache||
Synchronization||
__Additional Features__||


## Usage

First install GCC, Perl, Make, GDB, QEMU.

Under `pintos/src/utils`, run `make`, and add this directory to your `PATH`.

## Colaboration

There is no restriction on code style, comments, e.t.c.
However, use the following pattern to mark your work:

Use the following pattern to deprecate old code:

```c
/* old code begin */
// old_line;
// old_line;
// old_line;
/* old code end */
```

Use the following pattern to mark your own work:

```c
/* WHO's code begin */
new_line;
new_line;
new_line;
/* WHO's code end */ 
```