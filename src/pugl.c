#include "init.h"

#include "pugl/detail/implementation.c"

#if defined(LPUGL_USE_WIN)
    #include "pugl/detail/win.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl/detail/x11.c"

#else
    #error missing platform definition
#endif 
