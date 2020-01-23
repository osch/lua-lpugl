#include "init.h"

#if defined(LPUGL_USE_MAC)
    #include "pugl/detail/mac_cairo.m"

#else
    #error use pugl_cairo.c
#endif 
