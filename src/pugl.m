#include "init.h"

#include "pugl-repo/src/implementation.c"

#if defined(LPUGL_USE_MAC)
    #include "pugl-repo/src/mac.m"
#else
    #error use pugl.c
#endif 
