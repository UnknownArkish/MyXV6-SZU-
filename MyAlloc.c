
#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include"spinlock.h"
#include"MyAlloc.h"


extern int mappages(pde_t*, void*, uint, uint, int);
pte_t * walkpgdir(pde_t*, const void*, int );

struct VMATable vmaTable;


void
InitVMA(struct VMA* vma){
    vma->start = -1;
    vma->size = vma->page_size = 0;
    vma->prev = vma->next = 0;
}


// 使用首次适应算法，找到vm结构体中可以放下nbytes的Location
struct VMA* FindVMALocation_FF(struct VM* vm, uint nbytes){
    struct VMA* result = 0;
    struct VMA* pVMA = vm->header;
    while( pVMA && pVMA->next ){
        struct VMA* pNext = pVMA->next;
        if( pNext->start - ( pVMA->start + pVMA->page_size ) >= nbytes ){
            break;
        }
        pVMA = pVMA->next;
    }
    result = pVMA;
    return result;
}

// 从vmaTable中找到一个未使用的vma
struct VMA* FindUnusedVMA(){
    int i;
    struct VMA* result = 0;
    acquire(&vmaTable.lock);
    for( i = 0; i < vmaTable.total_count; i++ ){
        if( vmaTable.storage[i].start < 0 ){
            result = &vmaTable.storage[i];
            result->start = 0;
            break;
        }
    }
    release(&vmaTable.lock);
    return result;
}
// 回收一个VMA
void RecycleVMA(struct VMA* vma){
    InitVMA(vma);
}

// 初始化Mylloc文件
void
InitMyAlloc(){
    initlock(&vmaTable.lock, "vmaTable");
    
    // 为table分配初始的vma储备
    vmaTable.storage = (struct VMA*)kalloc();
    // 对创建好的storage进行初始化
    int vmaSize = sizeof(struct VMA);
    int vmaCountPerPage = PGSIZE / vmaSize;

    vmaTable.total_count = vmaCountPerPage;
    vmaTable.left_count = vmaTable.total_count;
    int i;
    for( i = 0; i < vmaTable.total_count; i++ ){
        InitVMA( &vmaTable.storage[i] );
    }
}

// 拷贝VM结构体，并对VM中的内容进行映射
struct VM* CopyVM( pde_t* pgdirDest, pde_t* pgdirSrc, struct VM* vm){
    struct VM* result = (struct VM*)kalloc();
    result->header = 0;
    result->vma_count = 0;

    pte_t* pte;
    uint paSrc;
    uint paDest;
    uint flags;

    if( vm && vm->header != 0 ){
        struct VMA* pVMA = vm->header;
        while( pVMA != 0 ){
            // 进行内存分配
            void* addr = Alloc(pgdirDest, result, pVMA->size);
            // 复制内容
            if( (pte = walkpgdir( pgdirSrc, (void*)pVMA->start, 0 )) == 0){
                panic("CopyVM: pte should exist");
            }
            if( !(*pte & PTE_P) ){
                panic("CopyVM: page not present");
            }
            paSrc = PTE_ADDR(*pte);
            flags = PTE_FLAGS(*pte);

            pte = walkpgdir( pgdirDest, addr, flags );
            paDest = PTE_ADDR(*pte);

            memmove( (void*)P2V(paDest), (void*)P2V(paSrc), pVMA->size );
            pVMA = pVMA->next;
        }
    }
    return result;
}

void* Alloc(pde_t* pgdir, struct VM* vm, uint nbytes){
    struct VMA* vma = FindUnusedVMA();
    void* result = 0;
    if( vma != 0 ){
        // 提高到一页大小的倍数
        uint nbytes_pageup = PGROUNDUP(nbytes);
        // 需要的物理页帧数
        int pageNumNeeded = nbytes_pageup / PGSIZE;

        // 找到要放置的下一个vma
        struct VMA* vmaLocation = FindVMALocation_FF(vm, nbytes_pageup);
        // 填写vma的信息，并将vma注册到结构体vm当中
        if( vmaLocation ){
            vma->start = vmaLocation->start + vmaLocation->page_size;
            vma->next = vmaLocation->next;
            vmaLocation->next->prev = vma;
            vmaLocation->next = vma;
            vma->prev = vmaLocation;
        }else{
            vma->start = VMA_ALLOC_FLOOR;
            vma->next = vma->prev = 0;
            vm->header = vma;
        }
        vma->size = nbytes;
        vma->page_size = nbytes_pageup;

        int i;
        for( i = 0 ; i < pageNumNeeded; i++ ){
            char* temp = kalloc();
            // cprintf("kelloc: %p\n", temp);
            memset(temp, 0, PGSIZE);
            // 映射
            mappages( pgdir, (void*)(vma->start + i * PGSIZE), PGSIZE, V2P(temp), PTE_W | PTE_U );
        }
        result = (void*)vma->start;
        vm->vma_count++;
    }
    return result;
}

// 释放指向的内存空间
// 往往是Alloc的返回结果
int Free( pde_t* pgdir, struct VM* vm, void* addr){
    if( vm == 0 ){
        return -1;
    }
    // 遍历vm，找到起始地址和addr相同的vma
    struct VMA* vmaLocation = 0;

    struct VMA* pPrevVMA = 0;
    struct VMA* pVMA = vm->header;
    while( pVMA != 0 ){
        if( (void*)pVMA->start == addr ){
            vmaLocation = pVMA;
            break;
        }
        pPrevVMA = pVMA;
        pVMA = pVMA->next;
    }
    // 没有找到地址相同的则返回-1
    if( vmaLocation == 0 ){
        return -1;
    }
    
    // 解除页表映射
    uint page_size = vmaLocation->page_size;
    uint va = vmaLocation->start;
    uint endva = va + page_size - 1;

    uint pa;
    pte_t* pte;
    for( ; va < endva ; va += PGSIZE ){
        pte = walkpgdir( pgdir, (char*)va, 0 );
        if( !pte ){
            va += (NPTENTRIES - 1) * PGSIZE;
        }else if( (*pte & PTE_P) != 0 ){
            pa = PTE_ADDR(*pte);
            kfree( (char*)P2V(pa) );
            *pte = 0;
        }
    }
    
    // @TODO: 修改vm结构体
    // 判断vmaLocation是否是header
    if( pPrevVMA == 0 ){
        vm->header = vmaLocation->next;
        if( vm->header != 0 )
            vm->header->prev = 0;
    }else{
        pPrevVMA->next = vmaLocation->next;
        if( vmaLocation->next != 0 )
            vmaLocation->next->prev = pPrevVMA;
    }
    // 回收vma
    RecycleVMA(vmaLocation);
    vm->vma_count--;
    return 1;
}