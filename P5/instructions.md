# CS 537 Project 5 -- Scheduler

## Administrivia

- **Due Date**: April 2nd, at 11:59pm.
- Projects may be turned in up to 3 days late but you will receive a penalty of 10 percentage points for every day it is turned in late.
- **Slip Days**:
  - In case you need extra time on projects,  you each will have 2 slip days for individual projects and 3 slip days for group projects (5 total slip days for the semester). After the due date we will make a copy of the handin directory for on time grading. 
  - To use a slip day you will submit your files with an additional file `slipdays.txt` in your regular project handin directory. This file should include one thing only and that is a single number, which is the number of slip days you want to use (i.e. 1 or 2). Each consecutive day we will make a copy of any directories which contain one of these `slipdays.txt` files. 
  - After using up your slip days you can get up to 90% if turned in 1 day late, 80% for 2 days late, and 70% for 3 days late. After 3 days we won't accept submissions.
  - Any exception will need to be requested from the instructors.
  - Example of `slipdays.txt`:

```
1
```

- Some tests are provided at `~cs537-1/tests/P5`. There is a `README.md` file in that directory which contains the instructions to run the tests. The tests are partially complete and **you are encouraged to create more tests**.

- Questions: We will be using Piazza for all questions.

- Collaboration: The assignment may be done by yourself or with one partner. Copying code (from others) is considered cheating. [Read this](http://pages.cs.wisc.edu/~remzi/Classes/537/Spring2018/dontcheat.html) for more info on what is OK and what is not. Please help us all have a good semester by not doing this.

- This project is to be done on the [Linux lab machines ](https://csl.cs.wisc.edu/docs/csl/2012-08-16-instructional-facilities/), so you can learn more about programming in C on a typical UNIX-based platform (Linux). Your solution will be tested on these machines.

- **Handing it in**: 
  
  - Copy all xv6 files (not just the ones you changed) to `~cs537-1/handin/<cslogin>/P5/`.- after running `make clean`. The directory structure should look like the following:
    
    ```
    handin/<cslogin>/P5/
        |---- README.md 
        |---- resources.txt
        |---- xv6-public
              | ---- all the contents of xv6 with your modifications
    ```
  - **Group project handin:** each group only needs to hand in once. To submit, one person should put the code in their handin directory (the other should be empty).

- **NOTE: For this project, a [modified xv6 with multithreading feature](https://git.doit.wisc.edu/cdis/cs/courses/cs537/spring24/public/p5/-/tree/main/xv6-public?ref_type=heads) is provided. Please do not work on the original xv6.**

## Introduction

In this lab, we will delve into the intricacies of priority inversion within the context of xv6, a simple, Unix-like teaching operating system that has been extended to support threads. You will be tasked with implementing key features to explore and mitigate the effects of priority inversion. This lab includes creating a sleeplock for use in user space, introducing a `nice` system call to adjust the caller process's nice value (thus indirectly its priority), and modifying the xv6 scheduler to prioritize processes based on their effective priority, which considers both their nice value and the dynamics of priority inversion. Through these exercises, you will gain hands-on experience with advanced operating system concepts, enhancing the xv6 scheduler to more adeptly handle priority inversion and improve overall system performance and fairness.

## Objectives

- To understand priority inversion and priority inheritance. 
- To understand `sleeplock` implementation in xv6
- To understand context switches in xv6
- To implement system calls which modify process state

## Background: Priority Inversion and Priority Inheritance

**Priority Inversion** occurs when a higher-priority task is forced to wait for a lower-priority task to release a shared resource, leading to a scenario where the execution order is inverted from the intended priority-based schedule. 

Imagine a scenario in a multitasking operating system where a low-priority task, Task *L*, holds a lock on a shared resource. A high-priority task, Task *H*, which does not require the locked resource initially, eventually tries to access it and is blocked because Task *L* holds the lock. Meanwhile, medium-priority tasks, which do not need the shared resource, continue to run, effectively delaying Task *H* further. This scenario illustrates the counterintuitive phenomenon where a lower-priority task indirectly preempts a higher-priority one, leading to inefficient task management and potential system slowdowns.

**Priority Inheritance** is a strategic solution designed to mitigate the effects of priority inversion. With this protocol, when a higher-priority task is waiting for a resource locked by a lower-priority task, the system temporarily elevates the priority of the lower-priority task to that of the highest-priority waiting task. This adjustment helps ensure that the lower-priority task can complete its execution and release the resource more quickly, thereby reducing the waiting time for the higher-priority task.

Once the lower-priority task releases the shared resource, its priority is reverted back to its original level, and the higher-priority task can proceed with its execution, thus preserving the overall integrity and efficiency of the system's scheduling policy.

## Project Details

In this project, you are tasked with enhancing the xv6 operating system by introducing mechanisms to mitigate priority inversion. Specifically, you will:

1. Implement a sleep lock that can be used in user-space,
2. Develop a `nice` system call to adjust the nice value of the current program, influencing its scheduling priority, and
3. Modify the xv6 scheduler to account for both the nice value of processes and priority inheritance.

### New System Call: `clone()`

A [modified version of xv6](https://git.doit.wisc.edu/cdis/cs/courses/cs537/spring24/public/p5/-/tree/main/xv6-public?ref_type=heads) is provided, featuring a new system call, `clone`, which facilitates the creation of threads. 

#### Creating Multi-threaded Programs with `clone()`

`clone()` allows a function to be executed in a new thread within the same virtual address space as the parent process. Unlike `fork()`, `clone()` shares the calling process's address space, making explicit stack management necessary.

The function prototype of the new system call `clone()` is:

```c
int clone(void (*fn)(void*), void* stack, void* arg)
```

It executes the function designated by `fn` in a new thread (sharing the same virtual address space as its parent), with an argument provided by `arg`. 

Since the child and calling process may share memory,  it is not possible for the child thread to execute in the same stack as the calling process.  The calling process must therefore set up memory space for the child stack and pass a pointer to this space to `clone()`.  Stacks grow downward, so stack usually points to the topmost address of the memory space set up for the child stack.  Note that `clone()` does not provide a means whereby the caller can inform the kernel of  the  size  of  the stack area. 

Here is an example showing how one can create a multi-threading program 

```c
void fn(void* arg) {
  ...;

  // Please use `exit` to terminate
  exit();
}

int main() {
  char* stack = (char*)malloc(4096);
  int tid =
      clone(fn, stack + 4096, 0);
  if (tid < 0) {
    printf(2, "clone error");
  }
  wait();
  exit();
}
```

An example multithread program is also provided in file `multithread.c`. 

This function is designed to resemble `clone(2)` on Linux. 

#### Understanding How `clone()` Works

Process Table Slot Allocation: Upon invocation, `clone()` searches for an unused entry in the process table (notably mentioned as ptable within `proc.c`, line 10) to register the new thread.

Virtual Address Space Sharing: It assigns the new thread the same page directory pointer as its parent, ensuring they share the same virtual address space. 

Stack and Execution Setup: `clone()` then initializes the thread's stack by pushing the provided argument (`arg`) for the designated function (`fn`) onto the new thread's stack. It adjusts the stack pointer and instruction pointer to ensure that upon starting, the thread begins execution within `fn`, using its own private stack space. 

Thread Termination and Page Table Management: When a thread terminates, the system checks if any other threads are still sharing the same page table. This is done through a reference count associated with each page table. If the terminating thread is the last one using the page table (i.e., the reference count drops to 0), the page table is then reclaimed by the system. 

### User-space Sleep Lock

xv6 provides a `sleeplock` defined in `sleeplock.c` and `sleeplock.h` that can be used in the kernel. However, this lock isn't directly accessible from user-space programs. To bridge this gap, you'll implement a user-space variant of sleeplock. 

User-space sleep lock interface:

* Define the Lock Structure:
  Start by defining a lock structure in your code. This structure will represent the lock state and ownership. You will `typedef` this structure as `mutex`. 
  ```c
  typedef struct {
    // Lock state, ownership, etc.
  } mutex;
  ```

* Initialize the Lock:
  Define a function `minit` used to initialize a `mutex`. This function sets the lock to an unlocked state and prepares any necessary internal structures. 
  ```c
  void minit(mutex* m);
  ```
* Acquiring the Lock (`macquire`):
  A thread calls `macquire` to acquire lock. If the lock is available, the thread acquires it and continues execution. If another thread holds the lock, the calling thread is **put to sleep** until the lock becomes available.
  
  ```c
  void macquire(mutex* m);
  ```
* Releasing the Lock (`mrelease`):
  The thread holding the lock calls `mrelease` when it has finished its critical section. This action releases the lock and wakes up any threads that were sleeping while waiting for the lock to become available.

  ```c
  void mrelease(mutex* m);
  ```

A recommended way is to implement `minit` as a user-level function while realizing `macquire` and `mrelease` as system calls. 

To make these functionalities accessible, ensure that user programs can use these functions and structures by including `user.h`. 

Here is an example of using `mutex`: 

```c
#include "user.h"

mutex m;

void fn(void* arg) {
  macquire(mutex* m);
  // critical section
  void mrelease(mutex* m);
  exit();
}

int main() {
  minit(&m);
  ...
  if (clone(fn, stack + STACK_SZ, 0) < 0)
      ...
}
```

After implementing the lock, you may test your solution using `test_1`, `test_2`, and `test_3`. 

#### Additional Notes

- While implementing this feature, it's beneficial to refer to the existing sleeplock implementation in sleeplock.c and sleeplock.h for inspiration. 

- You do not need to handle scenarios where a child process or thread is created while a lock is held.

### `nice` System Call

The nice system call is designed to adjust the priority of the thread making the call. Here's how it functions:

- Function Prototype:

  ```c
  int nice(int inc);
  ```

- Functionality

  Purpose: `nice()` modifies the calling thread's nice value by adding `inc` to it.

  Priority Influence: The nice value affects the thread's priority, where a higher nice value corresponds to a lower priority.

  Range: The nice value can range from +19 (indicating the lowest priority) to -20 (indicating the highest priority).

  Default Value: Initially, every thread has a nice value of 0.

  Range Clamping: If an attempt is made to set the nice value beyond its allowed range, the value is adjusted to the nearest limit within the range (+19 or -20).

- Return Values:
  Success: Returns 0 if the operation is successful.

  Error: Returns -1 in case of an error. This behavior differs from the nice implementation on Linux.

#### Make the Scheduler `nice`-aware

The default scheduler of xv6 uses round-robin policy, which does not take the priority of each process into account. Since now a thread/process has a property of niceness, the scheduler should be aware of that. This means it should now prioritize processes and threads based on their niceness values. 

Specifically, in every scheduling decision (tick), the scheduler must select the thread or process with the lowest niceness value to run next. This selection prioritizes tasks with higher importance.

If multiple threads or processes share the same lowest niceness value, the scheduler should revert to a round-robin approach for these tasks, ensuring fair CPU time distribution among them. 

Ensure that the modified scheduler treats processes and threads identically, applying the same priority rules to both.

After accommodating niceness when scheduling, you may test your solution using `test_4` ~ `test_9`. 

### Implementing Priority Inheritance in Scheduler

As described in background, the scheduler should prioritize the lock holder if a high-priority thread is waiting on the lock. 

Here we take `test_10` as an example of how priority inheritance works and how you can write more tests to test your solution. 

```c
#include "types.h"
#include "user.h"

mutex m;

void fn1(void* arg) {
  nice(10);
  macquire(&m);
  // Some work...
  mrelease(&m);
  exit();
}

void fn2(void* arg) {
  sleep(3);
  nice(-10);
  macquire(&m);
  // Some work...
  mrelease(&m);
  exit();
}

int main() {
  char* stack1 = (char*)malloc(4096);
  char* stack2 = (char*)malloc(4096);

  clone(fn1, stack1 + 4096, 0);
  clone(fn2, stack2 + 4096, 0);

  sleep(6);

  // Some work...

  wait(); wait(); exit();
}
```

In this example, `main` first creates 2 threads. Thread 1 executes `fn1` and thread 2 executes `fn2`. The main thread and thread 2 are first blocked for a while, so thread 1, the only runnable thread, is scheduled. Thread 1 lowers its priority and grabs the lock successfully. When thread 2 gets the chance to run, it increases its priority and attempts to acquire the lock. Since the lock is already held by thread 1, thread 2 is put to sleep to wait for the lock to be available. However, since thread 2, the high-priority thread, is waiting for thread 1, the priority of thread 1 is now elevated and thread 1 should be prefered to the main thread, which has middle priority, when scheduling. After thread 1 is done, thread 2 will successfuly hold the lock, and be scheduled as it has higher priority than the main thread. 

Specifically, in this project, The temporary priority (niceness) of a lock holder is adjusted to match the highest priority (lowest niceness value) among all threads waiting for that lock. Mathematically, this is represented as:

$$
\text{elevated\_nice}_\text{lock holder}=\min_{t \text{ waits on lock}}\text{nice}_t
$$

If a thread holds more than one lock, its temporary priority is elevated to the highest priority among all threads waiting for any of the locks it holds.

Note that, we don't consider nested elevation. For instance, suppose $t_1$ with niceness $0$ holds lock $l_1$, $t_2$ with niceness $-1$ holds lock $l_2$ and waits on $l_1$, $t_3$ with niceness $-2$ waits on $l_2$. The elevated niceness if $t_2$ is $-2$ because $t_3$ waits on the lock $t_2$ is holding. However, the elevated niceness of $t_1$ is $-1$ instead of $-2$ because only $t_2$ is directly waiting for $t_1$. 

After the lock holder releases the lock, it's priority should be restored. 

To simplify the project, we assume that a thread can hold no more than 16 locks at the same time. 

After implementing priority inheritance, you may test your solution using `test_10`. 

<!-- ## Notes on Grading

1. Logging is performed when a user-space virtual memory switch happens (function call to `switchuvm`). This is done by calling function `log_sched()`. 

2. Grading relies on the naming rules specified in `clone` and field `nclone` in structure `proc`.  -->

## Hints

1. A recommended workflow is to follow the way the instructions are structured: 
   - Understand how `sleeplock` works in xv6 and implement your own version that can be used in user-space. 
   - Add `nice` system call and change the scheduler to consider niceness. 
   - Use elevated priority instead of simply niceness when scheduling to solve priority inversion. 

   Before working on your sleep lock, make sure you understand how `sleep` and `wakeup` works and what is the role of `chan` in `proc` structure. 

   Before working on the scheduler, you should already know how `scheduler()` does round robin scheduling and why `swtch()` can switch processes. 

2. For priority inheritance, the scheduler needs to know the waiting relations among processes. So information of the lock acquisition and waiting on the sleep lock should be available to the scheduler. This also implies the scheduler needs to know all the locks a thread currently has held to evaluate elevated niceness. 

3. If you are going to follow implementation of `struct sleeplock` in xv6, note that it uses the address of `locked` as `chan` when calling `sleep`. Since the code runs in kernel-space, the address for this lock is exclusive. Think about what kind of address (physical or virtual) you want to use to identify your lock. 

## Notes on Grading

You may notice a file named `grade.c`, containing a function called `log_sched()`, is placed under `xv6-public`. This function is called in `switchuvm`, which is invoked every time scheduling happens. The tester replaces `log_sched()` with some function that actually makes a log to record the order of how threads are scheduled, and you solution is tested based on the execution trace obtained from this log. 

