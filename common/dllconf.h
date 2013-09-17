#ifndef _DLLCONF_H
#define _DLLCONF_H

#if defined _MSC_VER || defined __MINGW32__
#define DLL_CONF __declspec(dllexport)
#else
#define DLL_CONF
#endif

#endif
