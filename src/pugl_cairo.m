#include "init.h"

#if defined(LPUGL_USE_MAC)
    #include "pugl-repo/src/mac_cairo.m"

#else
    #error use pugl_cairo.c
#endif 
