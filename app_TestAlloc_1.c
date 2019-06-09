
/*
    Alloc测试程序
    1. 首次适应算法是否有效
    2. Free是否真的将内存空间还给内核
*/

#include"types.h"
#include"stat.h"
#include"user.h"

#define KB 1024
#define PAGE_SIZE (4 * KB)

#define ALLOC_COUNT 5
void* addr[ALLOC_COUNT];

char buf[100];

void Block(const char* msg){
    printf(0, (char*)msg);
    gets(buf, 100);
}

void Clean(){
    int i;
    for( i = 0 ;i < ALLOC_COUNT; i++ ){
        if( addr[i] != 0 ){
            Free(addr[i]);
        }
    }
}

int main(){
    Block("Before Alloc 5 times\n");
    
    addr[0] = Alloc( 2 * PAGE_SIZE );
    addr[1] = Alloc( 3 * PAGE_SIZE );
    addr[2] = Alloc( 1 * PAGE_SIZE );
    addr[3] = Alloc( 7 * PAGE_SIZE );
    addr[4] = Alloc( 9 * PAGE_SIZE );

    Block("Before Free 2 times\n");

    Free(addr[1]);  addr[1] = 0;
    Free(addr[3]);  addr[3] = 0;
    Block("Before Alloc again \n");
    // alloc again
    addr[1] = Alloc( 5 * PAGE_SIZE );
    addr[3] = Alloc( 2 * PAGE_SIZE );

    Block("Afrer Alloc again\n");

    Clean();
    exit();
}