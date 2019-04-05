#include"types.h"
#include"defs.h"
#include"spinlock.h"
#include"sem.h"

int sh_var = 0;               // 所有进程共享的一个整型变量
void sh_var_write(int value){
  sh_var = value;
}
int sh_var_read(void){
  return sh_var;
}

int sem_count = 0;                // 当前使用的信号量个数
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