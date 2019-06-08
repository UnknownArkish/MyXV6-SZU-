

// VMA start的起始地址
#define VMA_ALLOC_FLOOR 0x4000

struct VMATable{
    struct VMA* storage;        // 系统预留的VMA仓库
    struct spinlock lock;       // 操作storage时的自旋锁

    int total_count;
    int left_count;
};