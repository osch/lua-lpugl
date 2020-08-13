#ifndef LPUGL_INIT_H
#define LPUGL_INIT_H

#if !defined(LPUGL_USE_WIN) && !defined(LPUGL_USE_X11) && !defined(LPUGL_USE_MAC)
    #if defined(WIN32) || defined(_WIN32)
        #define LPUGL_USE_WIN
    #elif defined(__APPLE__) && defined(__MACH__)
        #define LPUGL_USE_MAC
    #else
        #define LPUGL_USE_X11
    #endif
#endif

#if defined _WIN32 
  #define PUGL_API 
#else
  #if __GNUC__ >= 4
    #define PUGL_API         __attribute__ ((visibility ("hidden")))
    #define PUGL_API_PRIVATE __attribute__ ((visibility ("hidden")))
  #else
    #define PUGL_API 
    #define PUGL_API_PRIVATE
  #endif
#endif

#ifndef LPUGL_UNUSED
  #if defined(__cplusplus)
  #   define LPUGL_UNUSED(name)
  #elif defined(__GNUC__)
  #   define LPUGL_UNUSED(name) name##_unused __attribute__((__unused__))
  #else
  #   define LPUGL_UNUSED(name) name
  #endif
#endif

#endif // LPUGL_INIT_H
