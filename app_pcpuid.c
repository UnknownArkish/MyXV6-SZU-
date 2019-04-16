// 此文件用以测试我们添加的getcpuid系统调用

#include"user.h"
#include"stat.h"
#include"types.h"

int main(int argc, char** argv){
    printf(1, "My CPU id is %d\n", getcpuid());
    exit();
}