#ifndef _CRASHDUMP_H
#define _CRASHDUMP_H

#include "platform.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <dbghelp.h>   
#include <shellapi.h>   
#include <shlobj.h>   
#include <atlstr.h>
#include <strsafe.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
using namespace  std;
#include <signal.h>

#pragma   comment(lib, "Dbghelp.lib")

class CrashDump
{
public:

	static void dump_callstack(CONTEXT *context )
	{
        if(SymInitialize( GetCurrentProcess(), NULL, TRUE ))
        {
            printf( "Init dbghelp ok.\n" );
        }
        else
        {
            return;
        }

        SetErrorMode( SEM_NOGPFAULTERRORBOX );   

		STACKFRAME sf;
		memset( &sf, 0, sizeof( STACKFRAME ) );

		sf.AddrPC.Offset = context->Eip;
		sf.AddrPC.Mode = AddrModeFlat;
		sf.AddrStack.Offset = context->Esp;
		sf.AddrStack.Mode = AddrModeFlat;
		sf.AddrFrame.Offset = context->Ebp;
		sf.AddrFrame.Mode = AddrModeFlat;

		DWORD machineType = IMAGE_FILE_MACHINE_I386;

        HANDLE hProcess = GetCurrentProcess();
		HANDLE hThread = GetCurrentThread();

		FILE* hDumpFile = NULL;

		char	filename[4096];
		memset(filename, 0, sizeof(filename));

		time_t	theTime;
		time(&theTime);
		tm	t;
		localtime_s(&t, &theTime);
		sprintf_s(filename, "dump-%d-%d-%d-%d-%d-%d.txt",t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

		fopen_s(&hDumpFile,filename, "w");
        if(hDumpFile != NULL)
        {
            for( ; ; )
            {
                if( !StackWalk(machineType, hProcess, hThread, &sf, context, 0, SymFunctionTableAccess, SymGetModuleBase, 0 ) )
                {
                    break;
                }

                if( sf.AddrFrame.Offset == 0 )
                {
                    break;
                }
                BYTE symbolBuffer[ sizeof( SYMBOL_INFO ) + 4096 ];
                PSYMBOL_INFO pSymbol = ( PSYMBOL_INFO ) symbolBuffer;

                pSymbol->SizeOfStruct = sizeof( SYMBOL_INFO );
                pSymbol->MaxNameLen = MAX_SYM_NAME;

                DWORD64 symDisplacement = 0;
                if( SymFromAddr( hProcess, sf.AddrPC.Offset, 0, pSymbol ) )
                {
                    fprintf( hDumpFile, "Function : %s\n", pSymbol->Name );
                }
                else
                {
                    fprintf(hDumpFile,  "SymFromAdd failed! %d, %d, offset:%d\n", GetLastError(), hProcess, sf.AddrPC.Offset);
                }

                IMAGEHLP_LINE lineInfo = { sizeof(IMAGEHLP_LINE) };
                DWORD dwLineDisplacement;

                if( SymGetLineFromAddr( hProcess, sf.AddrPC.Offset, &dwLineDisplacement, &lineInfo ) )
                {
                    fprintf(hDumpFile, "[Source File : %s]\n", lineInfo.FileName ); 
                    fprintf(hDumpFile, "[Source Line : %u]\n", lineInfo.LineNumber ); 
                }
                else
                {
                    fprintf(hDumpFile,  "SymGetLineFromAddr failed!\n" );
                }
            }
        }

        SymCleanup(GetCurrentProcess());
	}

	static LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers)   
	{
        LONG ret = EXCEPTION_CONTINUE_SEARCH;

        char err[1024];
        char    filename[1024];
        memset(filename, '\0', sizeof(filename));

        time_t	theTime;
        time(&theTime);
        tm	t;
        localtime_s(&t, &theTime);
        sprintf_s(filename, "dump-%d-%d-%d-%d-%d-%d.dmp",t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

        HANDLE hDumpFile = CreateFile( filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hDumpFile == INVALID_HANDLE_VALUE)
        {
            sprintf(err, "Gen DumpFile Failed, error : %d", GetLastError());

            ::MessageBox(NULL, err, _T("Gen File Failed !"), MB_TOPMOST | MB_ICONSTOP);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        MINIDUMP_EXCEPTION_INFORMATION ExpParam;   
        ExpParam.ThreadId = GetCurrentThreadId();   
        ExpParam.ExceptionPointers = pExceptionPointers;   
        ExpParam.ClientPointers = TRUE;   

        MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory |
                                                MiniDumpWithDataSegs |
                                                MiniDumpWithHandleData |
                                                /*MiniDumpWithFullMemoryInfo |*/
                                                MiniDumpWithThreadInfo /*|
                                                MiniDumpWithUnloadedModules*/);

        if(MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),hDumpFile, mdt, &ExpParam, NULL, NULL))
        {
            ::MessageBox(NULL, filename, _T("Crash Report!"), MB_TOPMOST | MB_ICONSTOP);
            ret = EXCEPTION_EXECUTE_HANDLER;
        }
        else
        {
            sprintf(err, "WriteDump Failed, error : %d", GetLastError());
            ::MessageBox(NULL, err, _T("Crash Report Failed !"), MB_TOPMOST | MB_ICONSTOP);
        }

        CloseHandle(hDumpFile);
 
        dump_callstack(pExceptionPointers->ContextRecord);

        return ret;
	}   

	static	void	SetMyCrashDump()
	{
		SetUnhandledExceptionFilter(MyUnhandledExceptionFilter); 
	}
};
#else

class CrashDump
{
public:
    static	void	SetMyCrashDump()
    {
        /*  under linux do nothing  */
    }
};
#endif


#endif
