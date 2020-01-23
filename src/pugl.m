#include "init.h"

#include "pugl/detail/implementation.c"

#if defined(LPUGL_USE_MAC)
    #include "pugl/detail/mac.m"
#else
    #error use pugl.c
#endif 
