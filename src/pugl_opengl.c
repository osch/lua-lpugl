#include "init.h"

#if defined(LPUGL_USE_WIN)
    #include "pugl-repo/src/win_gl.c"
    #include "pugl-repo/src/win_stub.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl_opengl.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl-repo/src/x11_gl.c"
    #include "pugl-repo/src/x11_stub.c"

#else
    #error missing platform definition
#endif 
