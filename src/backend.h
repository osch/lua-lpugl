#ifndef LPUGL_BACKEND_H
#define LPUGL_BACKEND_H

#include "base.h"
#include "pugl/pugl.h"

#define LPUGL_BACKEND_MAGIC 0x123451

struct LpuglWorld;

typedef struct LpuglBackend {
    
    int                  magic;
    char                 versionId[32];  // lpugl.backend-XXX-XX.XX.XX
    struct LpuglWorld*   world;
    int                  used;
    const PuglBackend*   puglBackend;
    int                  (*newDrawContext)(lua_State* L, void* context);
    int                  (*finishDrawContext)(lua_State* L, int contextIdx);
    void                 (*closeBackend)(lua_State* L, int backendIdx);

} LpuglBackend;

#endif // LPUGL_BACKEND_H
