#ifndef LPUGL_MAIN_H
#define LPUGL_MAIN_H

#include "util.h"
#include "async_util.h"

extern Mutex*        lpugl_global_lock;
extern AtomicCounter lpugl_id_counter;

LPUGL_DLL_PUBLIC int luaopen_lpugl(lua_State* L);

#endif /* LPUGL_MAIN_H */
