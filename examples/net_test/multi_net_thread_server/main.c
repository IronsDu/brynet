#include <stdio.h>

#include "gateway.h"

/*  使用server-model01目录中的网路库代码 : 内置开启多个线程等待IOCP    */
/*  消息包直接在网络事件中处理  */

int main()
{
    gateway_init(4002);
    gateway_start();
    getchar();
}