#ifndef LPUGL_WORLD_H
#define LPUGL_WORLD_H

#include "base.h"
#include "util.h"
#include "async_util.h"

#include "pugl/pugl.h"

/* ============================================================================================ */

//  uservalue indices
#define LPUGL_WORLD_UV_VIEWS      1
#define LPUGL_WORLD_UV_ERRFUNC    2
#define LPUGL_WORLD_UV_EVENTL     3
#define LPUGL_WORLD_UV_PROCFUNC   4
#define LPUGL_WORLD_UV_DEFBACKEND 5
#define LPUGL_WORLD_UV_BACKENDS   6

/* ============================================================================================ */

struct LpuglBackend;

typedef struct LpuglWorld {
    Lock                  lock;
    lua_Integer           id;
    AtomicCounter         used;
    PuglWorld*            puglWorld;
    struct LpuglWorld*    nextWorld;
    int                   weakWorldRef;
    int                   viewCount;
    lua_State*            eventL;
    AtomicCounter         awakeSent;
    void                  (*registrateBackend)(lua_State* L, int worldIdx, int backendIdx);
    void                  (*deregistrateBackend)(lua_State* L, int worldIdx, int backendIdx);
} LpuglWorld;

/* ============================================================================================ */

#define LPUGL_WORLD_MAGIC 0x123450

typedef struct WorldUserData {
    int           magic;
    char          versionId[32];  // lpugl.world-XXX-XX.XX.XX
    LpuglWorld*   world;
    lua_Integer   id;
    bool          restricted;
} WorldUserData;

/* ============================================================================================ */

int lpugl_world_errormsghandler(lua_State* L);

int lpugl_world_init_module(lua_State* L, int module);

/* ============================================================================================ */

#endif // LPUGL_WORLD_H

