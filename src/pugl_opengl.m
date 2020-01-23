#include "init.h"

#if defined(LPUGL_USE_MAC)
    #include "pugl/detail/mac_gl.m"

#else
    #error use pugl_opengl.c
#endif 
