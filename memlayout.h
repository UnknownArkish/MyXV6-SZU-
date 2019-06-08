// Memory layout

#define EXTMEM  0x100000            // Start of extended memory ，可扩展内存开始地址
#define PHYSTOP 0xE000000           // Top physical memory      ，物理内存顶部
#define DEVSPACE 0xFE000000         // Other devices are at high addresses，外接设备开始地址

// Key addresses for address space layout (see kmap in vm.c for layout)
// 
#define KERNBASE 0x80000000         // First kernel virtual address，内核虚拟地址
#define KERNLINK (KERNBASE+EXTMEM)  // Address where kernel is linked,内核链接的地址

#define V2P(a) (((uint) (a)) - KERNBASE)    // 虚拟地址转物理地址
#define P2V(a) (((void *) (a)) + KERNBASE)  // 物理地址转虚拟地址

#define V2P_WO(x) ((x) - KERNBASE)    // same as V2P, but without casts
#define P2V_WO(x) ((x) + KERNBASE)    // same as P2V, but without casts
