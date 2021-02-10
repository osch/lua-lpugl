#include "init.h"

#include "pugl-repo/src/implementation.c"

#if defined(LPUGL_USE_WIN)
    #include "pugl-repo/src/win.c"
    #include "pugl-repo/src/win_stub.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl-repo/src/x11.c"
    #include "pugl-repo/src/x11_stub.c"

#else
    #error missing platform definition
#endif 
