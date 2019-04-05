// 描述系统中所有进程的文件

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

int sh_var = 0;               // 所有进程共享的一个整型变量
void sh_var_write(int value){
  sh_var = value;
}
int sh_var_read(void){
  return sh_var;
}


#define SEM_MAXIMUM_NUM 128       // 最大信号量个数
#define BLOCK_PROC_MAXIMUM_NUM 32 // 最大阻塞进程个数
int sem_count = 0;                // 当前使用的信号量个数

// 唤醒第一个等待chan的进程
void wakeup1p(void *chan);

struct sem
{
  struct spinlock lock;                        // 单个信号量使用的内核自旋态锁
  int resource_count;                          // 信号量当前资源数量
  int block_proc_len;                          // 阻塞进程个数
  int block_procs_pid[BLOCK_PROC_MAXIMUM_NUM]; // 阻塞进程列表，存储pid
  int is_allocated;                            // 信号量是否已分配
};

struct sem sems[SEM_MAXIMUM_NUM]; // 操作系统中所有的信号量
struct spinlock sems_lock;        // 用以操作sems时解决同步问题的自旋锁

// 信号量的初始化函数
void sem_init(void)
{
  // 初始化有关操作sems的锁，调用spinlock的initlock函数
  initlock(&sems_lock, "sems");
}
// 创建一个信号量，参数为资源初始值，返回信号量的下标值
// 返回-1表示没有空余的信号量
int create_sem(int res_count)
{
  acquire(&sems_lock);
  int result = -1;
  // 先判断当前信号量个数是否已经大于最大个数
  // 如果是则不用进行循环查找空闲信号量
  if (sem_count < SEM_MAXIMUM_NUM)
  {
    int i;
    // 需要操作sems数组，因此需要获取sems_lock锁
    for (i = 0; i < SEM_MAXIMUM_NUM; i++)
    {
      if (sems[i].is_allocated == 0)
      {
        sems[i].is_allocated = 1;
        // 初始化锁
        initlock(&sems[i].lock, "sem");
        sems[i].resource_count = res_count;
        
        result = i;
        sem_count++;
        break;
      }
    }
  }
  release(&sems_lock);
  return result;
}
// 信号量p操作，返回0表示正常，1表示失败
int sem_p(int sem_index)
{
  acquire(&sems[sem_index].lock);
  int result = 0;
  do
  {
    if (sems[sem_index].is_allocated == 0)
    {
      result = 1;
      break;
    }

    sems[sem_index].resource_count--;
    if (sems[sem_index].resource_count < 0)
    {
      // 以信号量作为chan进行睡眠，让出资源
      // 使用信号量中的锁，睡眠阻塞时该锁会释放，直到重新调度时
      // 才会重新获得该锁
      sleep(&sems[sem_index], &sems[sem_index].lock);
    }
  } while (0);
  // 见sleep时解释
  release(&sems[sem_index].lock);
  return result;
}

// 信号量v操作，返回0正常，1表示失败
int sem_v(int sem_index)
{
  acquire(&sems[sem_index].lock);
  int result = 0;
  do
  {
    if (sems[sem_index].is_allocated == 0)
    {
      result = 1;
      break;
    }

    sems[sem_index].resource_count++;
    if (sems[sem_index].resource_count <= 0)
    {
      // 如果有进程正在等待（sleep）资源，则唤醒等待该信号量的第一个进程
      wakeup1p(&sems[sem_index]);
    }
  } while (0);
  release(&sems[sem_index].lock);
  return result;
}

// 释放一个信号量，返回0正常，1表示失败
int free_sem(int sem_index)
{
  acquire(&sems[sem_index].lock);
  acquire(&sems_lock);
  int result = 0;
  do
  {
    // 指定信号量没有分配
    if (sems[sem_index].is_allocated == 0)
    {
      result = 1;
      break;
    }

    // 进行脏数据的还原
    sems[sem_index].block_proc_len = 0;
    sems[sem_index].resource_count = 0;
    sems[sem_index].is_allocated = 0;
    sem_count--;
  } while (0);
  release(&sems_lock);
  release(&sems[sem_index].lock);

  return result;
}

// 所有进程
struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// 唤醒第一个等待chan的进程
void wakeup1p(void *chan)
{
  // @TODO: 遍历ptable中所有进程，找到第一个正在睡眠且chan相同的进程
  // @TODO：将其设置为RUNNABLE
  acquire(&ptable.lock);
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      break;
    }
  release(&ptable.lock);
}

// 操作系统的第一个进程（init进程）
static struct proc *initproc;
// 下一个进程的pid号，初始化为1
int nextpid = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// 初始化proc
void pinit(void)
{
  // 初始化ptable的lock
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
// Must hold ptable.lock.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);

  p = allocproc();
  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;

  acquire(&ptable.lock);

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    release(&ptable.lock);
    return -1;
  }

  // Copy process state from p.
  if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    release(&ptable.lock);
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *p;
  int fd;

  if (proc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (proc->ofile[fd])
    {
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == proc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for zombie children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != proc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || proc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock); //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
// 进入调度器
void sched(void)
{
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (cpu->ncli != 1)
    panic("sched locks");
  if (proc->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 释放锁（原子操作）并以chan作为睡眠信号（唤醒时指定chan进行唤醒）
void sleep(void *chan, struct spinlock *lk)
{
  if (proc == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  // 为了保证可以改变当前进程的state和通知调度器，必须acquire(ptable.lock)
  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep. 进入睡眠状态
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
// 唤醒ptable中所有以chan信号睡眠的进程（使进程进入RUNNABLE状态）
// 调用前必须保证持有ptbale的lock
static void
wakeup1(void *chan)
{
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
// 唤醒所有以chan作为信号量的进程
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
