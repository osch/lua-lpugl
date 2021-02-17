#include "init.h"

#include "pugl-repo/src/implementation.c"

#if defined(LPUGL_USE_WIN)
    #include "pugl-repo/src/win.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl-repo/src/x11.c"

#else
    #error missing platform definition
#endif 
