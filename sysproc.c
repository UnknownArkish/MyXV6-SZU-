#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

// 写入sh_var
int sys_sh_var_write(void){
  int value;
  argint(0,&value);
  sh_var_write(value);
  return 0;
}
// 读取sh_var
int sys_sh_var_read(void){
  return sh_var_read();
}

// 创建一个信号量
int sys_create_sem(void){
  // 获取参数，调用create_sem
  int res_count;
  argint(0,&res_count);
  return create_sem(res_count);
}
// 信号量p操作
int sys_sem_p(void){
  // 获取参数，调用sem_p
  int sem_index;
  argint(0,&sem_index);
  return sem_p(sem_index);
}
// 信号量v操作
int sys_sem_v(void){
  // 获取参数，调用sem_v
  int sem_index;
  argint(0,&sem_index);
  return sem_v(sem_index);
}
// 释放一个信号量
int sys_free_sem(void){
  // 获取参数，调用free_sem
  int sem_index;
  argint(0,&sem_index);
  return free_sem(sem_index);
}

int sys_getcpuid(void){
  return cpunum();
}

int
sys_fork(void)
{
  return fork();
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
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  // 获取调用sys_sleep时第一个参数，放到n当中
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
// 返回从系统运行时发生了多少次时钟中断
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
