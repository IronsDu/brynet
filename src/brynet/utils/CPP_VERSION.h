#ifndef _BRYNET_UTILS_CPP_VERSION_H
#define _BRYNET_UTILS_CPP_VERSION_H

#if (__cplusplus >= 201103L || \
     (defined(_MSC_VER) && _MSC_VER >= 1800))
#define HAVE_LANG_CXX11 1
#endif

#if (__cplusplus >= 201402L || \
     (defined(_MSC_VER) && _MSC_VER >= 1900))
#define HAVE_LANG_CXX14 1
#endif

#if (__cplusplus >= 201703L || \
     (defined(_MSC_VER) && _MSC_VER >= 1910))
#define HAVE_LANG_CXX17 1
#endif

#endif