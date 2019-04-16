// 此文件用以简单观察xv6的进程调度

#include"types.h"
#include"user.h"
#include"stat.h"

int main(int argc, char** argv){
    int a;
    printf(1, "A simple app to check schedule of xv6\n");
    a = fork();
    a = fork();
    // dead loop...
    while(1){a++;}

    exit();
}
