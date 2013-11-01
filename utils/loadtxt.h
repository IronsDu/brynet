#ifndef _LOADTXT_H
#define _LOADTXT_H

//单行最大字符数
#define MAX_LINE_CHAR 1024
//单行最大分割出来的字符串数
#define MAX_STRINLINE 100

//单行数据处理(回调函数)
typedef void(*fnParseLine)(const char* aStrArray[], int aArrayLen, void* aArg);

//加载文本文件
void g_loadtxt(const char* aFileName, fnParseLine pfnCallBack, char aSplitChar, void* aArg);

#endif
