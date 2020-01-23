#include "init.h"

#if defined(LPUGL_USE_WIN)
    #include "pugl/detail/win_gl.c"

#elif defined(LPUGL_USE_MAC)
    #error use pugl_opengl.m for mac

#elif defined(LPUGL_USE_X11)
    #include "pugl/detail/x11_gl.c"

#else
    #error missing platform definition
#endif 
