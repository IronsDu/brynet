#include <string.h>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <assert.h>

using namespace std;

#include "loadtxt.h"

void g_loadtxt(const char* aFileName, fnParseLine pfnCallBack, char aSplitChar, void* aArg)
{
    if (NULL == aFileName || NULL == pfnCallBack)
    {
        return;
    }

    ifstream fin(aFileName);
    char lineBuff[MAX_LINE_CHAR];

    while(fin.good() && !fin.eof())
    {
        fin.getline(lineBuff, sizeof(lineBuff));

        const char* pStrArray[MAX_STRINLINE] = {0};
        int strArrayLen = 0;

        const size_t len = strlen(lineBuff);
        const size_t incLen = len - 1;

        /*  如果最后一个元素为回车符则直接替换带结束符  */
        if(incLen >= 0 && lineBuff[incLen] == '\r')
        {
            lineBuff[incLen] = '\0';
        }

        const char* oneStr = lineBuff;
        pStrArray[strArrayLen++] = oneStr;

        for (size_t i = 0; i < len; i++)
        {
            if (lineBuff[i] == aSplitChar)
            {
                lineBuff[i] = '\0';
                if (i <= incLen)
                {
                    oneStr = lineBuff + i + 1;
                    pStrArray[strArrayLen++] = oneStr;
                }
            }
        }

        if( len > 0 && 
            (len >= 2 && !(lineBuff[0] == '/' && (lineBuff[1] == '/')))
            )
        {
            pfnCallBack(pStrArray, strArrayLen, aArg);
        }
    }

    fin.close();
}
