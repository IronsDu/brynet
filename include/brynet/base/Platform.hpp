#pragma once

#if defined _MSC_VER || defined __MINGW32__
#define BRYNET_PLATFORM_WINDOWS
#pragma comment(lib, "ws2_32")
#elif defined __APPLE_CC__ || defined __APPLE__
#define BRYNET_PLATFORM_DARWIN
#else
#define BRYNET_PLATFORM_LINUX
#endif
