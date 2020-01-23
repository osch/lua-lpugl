
#include "init.h"
#include "pugl/pugl_gl.h"

#include "lpugl_opengl.h"
#include "world.h"
#include "version.h"
#include "backend.h"
#include "error.h"

/* ============================================================================================ */

#define LPUGL_OPENGL_BACKEND_UV_WORLD 0

/* ============================================================================================ */

static const char* const LPUGL_OPENGL_BACKEND_CLASS_NAME = "lpugl.opengl.backend";

/* ============================================================================================ */

typedef struct LpuglOpenglBackend {

    LpuglBackend base;

} LpuglOpenglBackend;

/* ============================================================================================ */

static void setupBackendMeta(lua_State* L);

static int pushBackendMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LPUGL_OPENGL_BACKEND_CLASS_NAME)) {
        setupBackendMeta(L);
    }
    return 1;
}


/* ============================================================================================ */

static void closeBackend(lua_State* L, int backendIdx)
{
    LpuglOpenglBackend* udata = lua_touserdata(L, backendIdx);
    if (lua_getuservalue(L, backendIdx) == LUA_TTABLE) {    /* -> uservalue */
        if (lua_rawgeti(L, -1, LPUGL_OPENGL_BACKEND_UV_WORLD)
                                       == LUA_TUSERDATA) {  /* -> uservalue, world */
            lua_pushnil(L);                                 /* -> uservalue, world, nil */
            lua_setuservalue(L, backendIdx);                /* -> uservalue, world */
        }                                                   /* -> uservalue, ? */
        lua_pop(L, 2);                                      /* -> */
    } else {                                                /* -> nil */
        lua_pop(L, 1);                                      /* -> */
    }
    udata->base.world = NULL;
}

/* ============================================================================================ */

static int Backend_close(lua_State* L)
{
    LpuglOpenglBackend* udata = luaL_checkudata(L, 1, LPUGL_OPENGL_BACKEND_CLASS_NAME);
    if (udata->base.world) {
        if (udata->base.used > 0) {
            return luaL_error(L, "backend is used by view");
        }
        if (lua_getuservalue(L, 1) == LUA_TTABLE) {             /* -> uservalue */
            if (lua_rawgeti(L, -1, LPUGL_OPENGL_BACKEND_UV_WORLD)
                                           == LUA_TUSERDATA) {  /* -> uservalue, world */
                WorldUserData* worldUdata = lua_touserdata(L, -1);
                if (worldUdata->world) {
                    worldUdata->world->deregistrateBackend(L, lua_gettop(L), 1);
                }
            }                                                   /* -> usevalue, ? */
            lua_pop(L, 2);                                      /* -> */
        } else {                                                /* -> ? */
            lua_pop(L, 1);                                      /* -> */
        }
        closeBackend(L, 1);
    }
    return 0;
}

/* ============================================================================================ */

static int Backend_isClosed(lua_State* L)
{
    LpuglOpenglBackend* udata = luaL_checkudata(L, 1, LPUGL_OPENGL_BACKEND_CLASS_NAME);
    lua_pushboolean(L, udata->base.world == NULL);
    return 1;
}

/* ============================================================================================ */

static int Lpugl_opengl_newBackend(lua_State* L)
{
    WorldUserData* worldUdata = luaL_checkudata(L, 1, "lpugl.world");

    if (!lua_getmetatable(L, 1) || lua_getfield(L, -1, "__name") != LUA_TSTRING 
     || strcmp("lpugl.world", lua_tostring(L, -1)) != 0
     || !worldUdata || worldUdata->magic != LPUGL_WORLD_MAGIC)
    {
        return luaL_argerror(L, 1, "invalid lpugl.world");
    }                                                               /* -> meta, __name */
    lua_pop(L, 2);                                                  /* -> */
    if (strcmp(worldUdata->versionId, "lpugl.world-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING) != 0) {
        return luaL_argerror(L, 1, "lpugl.backend version mismatch");
    }
    if (worldUdata->restricted) {
        return lpugl_error(L, LPUGL_ERROR_ILLEGAL_STATE ": lpugl.world restricted");
    }
    if (!worldUdata->world) {
        return lpugl_error(L, LPUGL_ERROR_ILLEGAL_STATE ": lpugl.world closed");
    }

    LpuglOpenglBackend* udata = lua_newuserdata(L, sizeof(LpuglOpenglBackend));
    memset(udata, 0, sizeof(LpuglOpenglBackend));
    
    udata->base.magic = LPUGL_BACKEND_MAGIC;
    strcpy(udata->base.versionId, "lpugl.backend-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING);
    udata->base.puglBackend       = puglGlBackend();
    udata->base.closeBackend      = closeBackend;

    pushBackendMeta(L);      /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    
    worldUdata->world->registrateBackend(L, 1, lua_gettop(L)); /* -> udata */
    udata->base.world = worldUdata->world;
    lua_newtable(L);                                           /* -> udata, uservalue, world */
    lua_pushvalue(L, 1);                                       /* -> udata, uservalue, world */
    lua_rawseti(L, -2, LPUGL_OPENGL_BACKEND_UV_WORLD);         /* -> udata, uservalue */
    lua_setuservalue(L, -2);                                   /* -> udata */
    
    return 1;
}

/* ============================================================================================ */

static int Lpugl_opengl_newWorld(lua_State* L)
{
    luaL_checkstring(L, 1); /* for puglSetClassName */
    int nargs = lua_gettop(L);
    bool loaded = false;
    if (lua_getglobal(L, "require") == LUA_TFUNCTION) {          /* -> require */
        lua_pushstring(L, "lpugl");                              /* -> require, "lpugl" */
        lua_call(L, 1, 1);                                       /* -> lpugl */
        if (lua_getfield(L, -1, "newWorld") == LUA_TFUNCTION) {  /* -> lpugl, newWorld() */
            for (int i = 1; i <= nargs; ++i) {
                lua_pushvalue(L, i);                             
            }                                                    /* -> lpugl, newWorld(), args... */
            lua_call(L, nargs, 1);                               /* -> lpugl, world */
            if (lua_getfield(L, -1, "setDefaultBackend") 
                             == LUA_TFUNCTION) {                 /* -> lpugl, world, setDefaultBackend() */
                lua_pushvalue(L, -2);                            /* -> lpugl, world, setDefaultBackend(), world */
                lua_pushcfunction(L, Lpugl_opengl_newBackend);   /* -> lpugl, world, setDefaultBackend(), world, newBackend() */
                lua_pushvalue(L, -2);                            /* -> lpugl, world, setDefaultBackend(), world, newBackend(), world */
                lua_call(L, 1, 1);                               /* -> lpugl, world, setDefaultBackend(), world, backend */
                lua_call(L, 2, 0);                               /* -> lpugl, world */
                loaded = true;
            }
        }
    }
    if (!loaded) {
        return lpugl_error(L, LPUGL_ERROR_FAILED_OPERATION ": error loading module \"lpugl\"");
    }
    return 1;
}

/* ============================================================================================ */

static const luaL_Reg BackendMethods[] = 
{
    { "close",        Backend_close },
    { "isClosed",     Backend_isClosed },
    { NULL,           NULL } /* sentinel */
};

static const luaL_Reg BackendMetaMethods[] = 
{
    { "__gc",         Backend_close  },
    { NULL,           NULL           } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newBackend",   Lpugl_opengl_newBackend },
    { "newWorld",     Lpugl_opengl_newWorld   },
    { NULL,           NULL } /* sentinel */
};

static void setupBackendMeta(lua_State* L)
{
    lua_pushstring(L, LPUGL_OPENGL_BACKEND_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");
    
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "lpugl.backend");

    luaL_setfuncs(L, BackendMetaMethods, 0);
    
    lua_newtable(L);  /* BackendClass */
        luaL_setfuncs(L, BackendMethods, 0);
    lua_setfield (L, -2, "__index");
}

LPUGL_DLL_PUBLIC int luaopen_lpugl_opengl(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */
    
    if (luaL_newmetatable(L, LPUGL_OPENGL_BACKEND_CLASS_NAME)) {
        setupBackendMeta(L);
    }
    lua_pop(L, 1);

    int n = lua_gettop(L);
    
    int module = ++n; lua_newtable(L);                         /* -> module */
    lua_pushliteral(L, LPUGL_VERSION_STRING);                  /* -> module, version */
    lua_setfield(L, module, "_VERSION");                       /* -> module */
    lua_pushstring(L, "lpugl.opengl"
                   " (Version:" LPUGL_VERSION_STRING 
                   ",Platform:" LPUGL_PLATFORM_STRING
                   ",Date:" LPUGL_BUILD_DATE_STRING ")" );     /* -> module, info */
    lua_setfield(L, module, "_INFO");                          /* -> module */
    lua_pushliteral(L, LPUGL_PLATFORM_STRING);                 /* -> module, string */
    lua_setfield(L, module, "PLATFORM");                       /* -> module */
    luaL_setfuncs(L, ModuleFunctions, 0);                      /* -> module */

    lua_newtable(L);                                           /* -> module, meta */
    if (lua_getglobal(L, "require") == LUA_TFUNCTION) {        /* -> module, meta, require */
        lua_pushstring(L, "lpugl");                            /* -> module, meta, require, "lpugl" */
        lua_call(L, 1, 1);                                     /* -> module, meta, lpugl */
        lua_setfield(L, -2, "__index");                        /* -> module, meta */
        lua_pushliteral(L, "lpugl.opengl");                    /* -> module, meta, string */
        lua_setfield(L, -2, "__metatable");                    /* -> module, meta */
        lua_setmetatable(L, module);                           /* -> module */
        return 1;
    } else {
        return lpugl_error(L, LPUGL_ERROR_FAILED_OPERATION ": missing global function \"require\"");
    }

    return 1;
}

