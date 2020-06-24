# Trilling-PintOS

## Worklist

Work|Who|Status
---|---|---
__Threads__||
Alarm Clock|GXY|Pass Test
Priority Scheduling|GXY|Pass Test
Advanced Scheduler|GXY|Pass Test
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

## Links

### Project assignments

+ [Threads](https://www.cs.jhu.edu/~huang/cs318/fall17/project/project1.html)
+ [Use Programs](https://www.cs.jhu.edu/~huang/cs318/fall17/project/project2.html)
+ [Virtual Memory](https://www.cs.jhu.edu/~huang/cs318/fall17/project/project3.html)
+ [File Systems](https://www.cs.jhu.edu/~huang/cs318/fall17/project/project4.html)

### Introduction by Stanford

[Table of Contents](http://web.stanford.edu/class/cs140/projects/pintos/pintos.html)

## Usage

### Prerequisites

GCC, Perl, Make, GDB, QEMU.

Just use `apt` or `brew`.

### Build pintos utility tools

Under `pintos/src/utils`, run `make`, and add this directory to your `PATH`.

### Build sub-project

In `threads`, `userprog`, `vm`, `filesys`, or other subdirectories, run

```shell
make
```

### Run all tests

To run all tests and see your grade, run

```shell
make check
```

If you want to run all tests again with source codes unchanged, you may use

```shell
make --always-make check
```

### Run a single test

To run a single test, in directory `build`, run

```shell
bash pintos.sh run alarm-multiple
```

### Debug a single test

To debug a single test with GDB, in directory `build`, run

```shell
bash pintos.sh debug alarm-multiple
```

and in another terminal prompt in directory `build`, run

```shell
pintos-gdb kernel.o
```

and in the GDB prompt, type `debugpintos`.

## Colaboration

There are no restrictions on code style, comments, e.t.c.

It is recommended to use the following pattern to mark your work:

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