#include "init.h"

#if defined(LPUGL_USE_MAC)
    #include "pugl-repo/src/mac_gl.m"

#else
    #error use pugl_opengl.c
#endif 
