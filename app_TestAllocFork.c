
#include"types.h"
#include"stat.h"
#include"user.h"

#define KB 1024

char buf[100];

void Block(const char* msg){
    printf(0, "Block::");
    printf(0, (char*)msg);
    gets(buf, 100);
}

int main(){

    int* addr = Alloc(4 *KB);
    addr[0] = 100;
    int pid = fork();
    if( pid < 0 ){
        printf(0, "error with fork\n");
    }else if( pid == 0 ){
        printf(0, "child's process::addr[0]: %d\n", addr[0]);

        Free(addr);
        exit();
    }else{
        Free(addr);
        wait();
        exit();
    }
    return 0;
}