// 这是我们为xv6添加的第一个用户程序，enjoy！
// 记得在Makefile将此c文件链接生成的_*文件添加进去

#include"types.h"
#include"stat.h"
#include"user.h"

int main(int argc, char** argv){
    printf(1, "This is my own app!\n");
    exit();
}