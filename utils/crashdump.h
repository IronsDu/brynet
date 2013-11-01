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

	static BOOL CALLBACK MyMiniDumpCallback(
		PVOID                            pParam, 
		const PMINIDUMP_CALLBACK_INPUT   pInput, 
		PMINIDUMP_CALLBACK_OUTPUT        pOutput 
		) 
	{
		BOOL bRet = FALSE; 


		// Check parameters 

		if( pInput == 0 ) 
			return FALSE; 

		if( pOutput == 0 ) 
			return FALSE; 


		// Process the callbacks 

		switch( pInput->CallbackType ) 
		{
		case IncludeModuleCallback: 
			{
				// Include the module into the dump 
				bRet = TRUE; 
			}
			break; 

		case IncludeThreadCallback: 
			{
				// Include the thread into the dump 
				bRet = TRUE; 
			}
			break; 

		case ModuleCallback: 
			{
				// Does the module have ModuleReferencedByMemory flag set ? 

				if( !(pOutput->ModuleWriteFlags & ModuleReferencedByMemory) ) 
				{
					// No, it does not - exclude it 

					wprintf( L"Excluding module: %s \n", pInput->Module.FullPath ); 

					pOutput->ModuleWriteFlags &= (~ModuleWriteModule); 
				}

				bRet = TRUE; 
			}
			break; 

		case ThreadCallback: 
			{
				// Include all thread information into the minidump 
				bRet = TRUE;  
			}
			break; 

		case ThreadExCallback: 
			{
				// Include this information 
				bRet = TRUE;  
			}
			break; 

		case MemoryCallback: 
			{
				// We do not include any information here -> return FALSE 
				bRet = FALSE; 
			}
			break; 

		case CancelCallback: 
			break; 
		}

		return bRet; 
	}


	static void dump_callstack(CONTEXT *context )
	{
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

	static LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers)   
	{

        //return EXCEPTION_CONTINUE_SEARCH;

		

		//收集信息   
		/*CStringW strBuild;   
		strBuild.Format(L"Build: %s %s", __DATE__, __TIME__);   
		CStringW strError;   
		HMODULE hModule;   
		WCHAR szModuleName[MAX_PATH] = L"";   
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)pExceptionPointers->ExceptionRecord->ExceptionAddress, &hModule);   
		GetModuleFileName(hModule, (LPCH)szModuleName, ARRAYSIZE(szModuleName));   
		strError.AppendFormat(L"%s %d , %d ,%d.", szModuleName,pExceptionPointers->ExceptionRecord->ExceptionCode, pExceptionPointers->ExceptionRecord->ExceptionFlags, pExceptionPointers->ExceptionRecord->ExceptionAddress);   */
		
        //生成 mini crash dump   

        char	filename[1024];
        memset(filename, '\0', sizeof(filename));

        time_t	theTime;
        time(&theTime);
        tm	t;
        localtime_s(&t, &theTime);
        sprintf_s(filename, "dump-%d-%d-%d-%d-%d-%d.dmp",t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

		HANDLE hDumpFile;   
		MINIDUMP_EXCEPTION_INFORMATION ExpParam;   

		hDumpFile = CreateFile( filename, GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0 );

		ExpParam.ThreadId = GetCurrentThreadId();   
		ExpParam.ExceptionPointers = pExceptionPointers;   
		ExpParam.ClientPointers = TRUE;   

		MINIDUMP_TYPE MiniDumpWithDataSegs = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);   

        BOOL bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),    
            hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);   

        if( SymInitialize( GetCurrentProcess(), NULL, TRUE ) )
        {
            printf( "Init dbghelp ok.\n" );
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        SetErrorMode( SEM_NOGPFAULTERRORBOX );   
        dump_callstack(pExceptionPointers->ContextRecord);

        if( SymCleanup( GetCurrentProcess() ) )
        {
            printf( "Cleanup dbghelp ok.\n" );
        }

        return EXCEPTION_EXECUTE_HANDLER; //或者  关闭程序   
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