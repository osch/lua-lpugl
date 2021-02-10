#include "init.h"

#if defined(LPUGL_USE_MAC)
    #include "pugl-repo/src/mac_cairo_gl.m"

#else
    #error use pugl_cairo.c
#endif 
