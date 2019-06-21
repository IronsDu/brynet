#pragma once

#if defined _MSC_VER || defined __MINGW32__
#define PLATFORM_WINDOWS
#elif defined __APPLE_CC__ || defined __APPLE__
#define PLATFORM_DARWIN
#else
#define PLATFORM_LINUX
#endif
