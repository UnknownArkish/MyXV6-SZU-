// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?     是否上锁

  // For debugging:                           以下变量作为Debug时使用
  char *name;        // Name of lock.         
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};

