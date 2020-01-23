#include "init.h"

#if defined(LPUGL_USE_WIN)
    #include "pugl/detail/win_cairo.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl_cairo.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl/detail/x11_cairo.c"

#else
    #error missing platform definition
#endif 
