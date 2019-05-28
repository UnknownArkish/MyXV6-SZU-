extern int sh_var;

#define SEM_MAXIMUM_NUM 128       // 最大信号量个数
#define BLOCK_PROC_MAXIMUM_NUM 32 // 最大阻塞进程个数
struct sem
{
  struct spinlock lock;                        // 单个信号量使用的内核自旋态锁
  int resource_count;                          // 信号量当前资源数量
  int block_proc_len;                          // 阻塞进程个数
  int block_procs_pid[BLOCK_PROC_MAXIMUM_NUM]; // 阻塞进程列表，存储pid
  int is_allocated;                            // 信号量是否已分配
};

extern int sem_count;
extern struct sem sems[];
extern struct spinlock sems_lock;