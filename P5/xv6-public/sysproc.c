#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "mutex.h"

// spinlock
struct spinlock mutexlock;
void
initmutexlock(void)
{
  initlock(&mutexlock, "mutexlock");
}

int
sys_fork(void)
{
  return fork();
}

int sys_clone(void)
{
  int fn, stack, arg;
  argint(0, &fn);
  argint(1, &stack);
  argint(2, &arg);
  return clone((void (*)(void*))fn, (void*)stack, (void*)arg);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  if (n == 0) {
    yield();
    return 0;
  }
  acquire(&tickslock);
  ticks0 = ticks;
  myproc()->sleepticks = n;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  myproc()->sleepticks = -1;
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_macquire(void)
{
  mutex *m;

  // Get mutex pointer from syscall args
  if(argptr(0, (char**)&m, sizeof(*m)) < 0) {
    return -1;
  }

  // Acquire spinlock to ensure atomic operations
  acquire(&mutexlock);
  priorityElevation(m);

  while (m->locked) {
    sleep(m, &mutexlock); // calling process is put to sleep, spinlock released until it awakens
  }

  // mutex available, set to locked and record PID of current holder
  m->locked = 1;
  m->pid = myproc()->pid;

  // release global spinlock after acquiring mutex
  release(&mutexlock);

  return 0;
}

// System call to acquire mutex lock
int
sys_mrelease(void)
{
  mutex *m;

  // Get mutex pointer from syscall args
  if(argptr(0, (char**)&m, sizeof(*m)) < 0) {
    return -1;
  }

  // Acquire spinlock to ensure atomic operations
  acquire(&mutexlock);

  // Mark mutex as unlocked and clear PID
  m->locked = 0;
  m->pid = 0;

  // Restore calling process original priority (in case of elevation)
  myproc()->nice = myproc()->original_nice;

  // Wakeup all other waiting processes
  wakeup(m);

  // Release global spinlock
  release(&mutexlock);

  return 0;
}

// System call to adjust the calling process's priority (nice value) within the allowed range
int sys_nice(void)
{
    int inc;

    // Retrieve the nice value increment from system call args
    if (argint(0, &inc) < 0)
        return -1;

    // Clamp the nice value increment to the allowed range before applying it.
    if (inc > 19){
        inc = 19; // upper limit
    } else if (inc < -20){
        inc = -20; // lower limit
    }

    // Apply the clamped nice value increment and update original_nice to show this
    myproc()->nice = inc;
    myproc()->original_nice = inc;
    return 0;
}

