#include "init.h"

#if defined(LPUGL_USE_WIN)
    #include "pugl-repo/src/win_cairo.c"
    #include "pugl-repo/src/win_stub.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl_cairo.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl-repo/src/x11_cairo.c"
    #include "pugl-repo/src/x11_stub.c"

#else
    #error missing platform definition
#endif 
