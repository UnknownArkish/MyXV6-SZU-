/*
    Alloc测试程序
        1. Alloc申请测试
        2. Free空洞测试
        3. 首次适应算法测试
*/


#include"types.h"
#include"stat.h"
#include"user.h"

#define KB (1024)

#define ALLOC_NUM 6

char buf[100];

void Block(const char* msg){
    printf(0, (char*)msg);
    gets(buf, 100);
}

int main(){
    void* addr[ALLOC_NUM];


    Block("Before Alloc 6\n");
    int i;
    for( i = 0; i < ALLOC_NUM; i++ ){
        addr[i] = Alloc(8 * KB);
    }

    Block("Before free\n");
    printf(0, "Free: %d\n", Free(addr[1]));
    addr[1] = 0;
    printf(0, "Free: %d\n", Free(addr[2]));
    addr[2] = 0;
    printf(0, "Free: %d\n", Free(addr[4]));
    addr[4] = 0;

    Block("Before Alloc again\n");
    addr[1] = Alloc(4 * KB);
    addr[2] = Alloc(4 * KB);
    addr[4] = Alloc(4 * KB);
    Block("After Alloc again\n");

    for( i = 0; i < ALLOC_NUM; i++ ){
        if( addr[i] != 0 )
            Free(addr[i]);
    }
    exit();
}