#ifndef LPUGL_BASE_H
#define LPUGL_BASE_H

#include "init.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* async_defines.h must be included first */
#include "async_defines.h"

#ifdef LPUGL_ASYNC_USE_WIN32
    #include <sys/types.h>
    #include <sys/timeb.h>
#else
    #include <sys/time.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


/**
 * dllexport
 *
 * see also http://gcc.gnu.org/wiki/Visibility
 */

#ifndef LPUGL_BUILDING_DLL
  #define LPUGL_BUILDING_DLL 1
#endif

#ifndef LPUGL_DLL_PUBLIC
  #if defined _WIN32 || defined __CYGWIN__
    #if LPUGL_BUILDING_DLL
      #ifdef __GNUC__
        #define LPUGL_DLL_PUBLIC __attribute__ ((dllexport))
      #else
        #define LPUGL_DLL_PUBLIC __declspec(dllexport) /* Note: actually gcc seems to also supports this syntax. */
      #endif
    #else
      #define LPUGL_DLL_PUBLIC
    #endif
  #else
    #if __GNUC__ >= 4
      #pragma GCC visibility push (hidden) 
      #if LPUGL_BUILDING_DLL
        #define LPUGL_DLL_PUBLIC __attribute__ ((visibility ("default")))
      #else
        #define LPUGL_DLL_PUBLIC
      #endif
    #else
      #define LPUGL_DLL_PUBLIC
    #endif
  #endif
#endif

#define COMPAT53_PREFIX lpugl_compat
#include "compat-5.3.h"

#endif // LPUGL_BASE_H
