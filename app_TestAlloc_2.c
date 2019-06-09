
/*
    Alloc测试程序
    1. 测试访问野指针
*/

#include"types.h"
#include"stat.h"
#include"user.h"

#define KB 1024
int main(){
    int* addr = Alloc( 4 * KB );
    addr[0] = 100;
    printf(0, "Before Free::addr[0]: %d\n", addr[0]);
    Free(addr);
    printf(0, "After Free::addr[0]: %d\n", addr[0]);
    exit();
}