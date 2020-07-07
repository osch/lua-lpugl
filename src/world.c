#include "base.h"

#include "lpugl.h"
#include "world.h"
#include "view.h"
#include "error.h"
#include "version.h"
#include "backend.h"

/* ============================================================================================ */

static const char* const LPUGL_WORLD_CLASS_NAME = "lpugl.world";

static LpuglWorld* global_world_list = NULL;

/* ============================================================================================ */

static void setupWorldMeta(lua_State* L);

static int pushWorldMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LPUGL_WORLD_CLASS_NAME)) {
        setupWorldMeta(L);
    }
    return 1;
}

/* ============================================================================================ */

static const char* toLuaString(lua_State* L, WorldUserData* udata)
{
    if (udata) {
        return lua_pushfstring(L, "%s: %p (id=%d)", LPUGL_WORLD_CLASS_NAME, udata, (int)udata->id);
    } else {
        return lua_pushfstring(L, "%s: invalid", LPUGL_WORLD_CLASS_NAME);
    }
}

/* ============================================================================================ */

int lpugl_world_errormsghandler(lua_State* L)
{
    lua_newtable(L);
    int rslt = lua_gettop(L);
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (   luaL_callmeta(L, 1, "__tostring")  /* does it have a metamethod */
            && lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
        {
            lua_rawseti(L, rslt, 1); /* string is the message */
            lua_pushvalue(L, 1); 
            lua_rawseti(L, rslt, 2); /* original error object */
            lua_settop(L, rslt);    /* return rslt */
            return 1;
        } else {
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                     luaL_typename(L, 1));
        }
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    lua_rawseti(L, rslt, 1);       /* traceback is the message */
    lua_pushvalue(L, 1); 
    lua_rawseti(L, rslt, 2);       /* original error object */
    lua_settop(L, rslt);           /* return rslt */
    return 1;
}

/* ============================================================================================ */

static PuglStatus lpugl_world_process(PuglWorld* puglWorld, void* voidData)
{
    LpuglWorld* world = voidData;
    if (!world || world->puglWorld != puglWorld) {
        fprintf(stderr, "lpugl: internal error in world.c:%d\n", __LINE__);
        abort();
    }
    if (!world->eventL) {
        fprintf(stderr, "lpugl: internal error in world.c:%d\n", __LINE__);
        abort();
    }
    atomic_set(&world->awakeSent, 0);
    
    lua_State* L = world->eventL;
    int oldTop = lua_gettop(L);
    
    lua_checkstack(L, LUA_MINSTACK);

    lua_pushcfunction(L, lpugl_world_errormsghandler);
    
    int msgh = lua_gettop(L);

    bool wasInCallback = world->inCallback;
    world->inCallback = true;

    if (   lua_rawgeti(L, LUA_REGISTRYINDEX, world->weakWorldRef) != LUA_TTABLE /* -> weakWorld */
        || lua_rawgeti(L, -1, 0) != LUA_TUSERDATA                               /* -> weakWorld, worldUdata */
        || lua_getuservalue(L, -1) != LUA_TTABLE                                /* -> weakWorld, worldUdata, worldUservalue */
        || lua_rawgeti(L, -1, LPUGL_WORLD_UV_PROCFUNC) != LUA_TFUNCTION)        /* -> weakWorld, worldUdata, worldUservalue, procFunc */
    {
        fprintf(stderr, "lpugl: internal error in view.c:%d\n", __LINE__);
        abort();
    }
    int rc = lua_pcall(L, 0, 0, msgh);                                      /* -> weakWorld, worldUdata, worldUservalue, ? */

    world->inCallback = wasInCallback;
    if (!wasInCallback && world->mustClosePugl) {
        lpugl_world_close_pugl(world);
    }

    if (rc != 0) {                                                          /* -> weakWorld, worldUdata, worldUservalue, error */
        bool handled = false;
        if (lua_rawgeti(L, -2, LPUGL_WORLD_UV_ERRFUNC) == LUA_TFUNCTION) {  /* -> weakWorld, worldUdata, worldUservalue, error, errFunc */
            lua_rawgeti(L, -2, 1);                                          /* -> weakWorld, worldUdata, worldUservalue, error, errFunc, error[1] */
            lua_rawgeti(L, -3, 2);                                          /* -> weakWorld, worldUdata, worldUservalue, error, errFunc, error[1], error[2] */
            int rc2 = lua_pcall(L, 2, 0, msgh);                             /* -> weakWorld, worldUdata, worldUservalue, error, ? */
            if (rc2 == 0) {
                handled = true;                                             /* -> weakWorld, worldUdata, worldUservalue, error */
                lua_pop(L, 1);                                              /* -> weakWorld, worldUdata, worldUservalue */
            } else {                                                        /* -> weakWorld, worldUdata, worldUservalue, error, error2 */
                lua_rawgeti(L, -1, 1);                                      /* -> weakWorld, worldUdata, worldUservalue, error, error2, errmsg2 */
                fprintf(stderr, 
                        "%s: %s\n", LPUGL_ERROR_ERROR_IN_ERROR_HANDLING,
                        lua_tostring(L, -1));
                lua_pop(L, 2);                                              /* -> weakWorld, worldUdata, worldUservalue, error */
            }
        } else {                                                            /* -> weakWorld, worldUdata, worldUservalue, error, nil */
            lua_pop(L, 1);                                                  /* -> weakWorld, worldUdata, worldUservalue, error */
        }
        if (!handled) {                                                     /* -> weakWorld, worldUdata, worldUservalue, error */
            lua_rawgeti(L, -1, 1);                                          /* -> weakWorld, worldUdata, worldUservalue, error, errmsg */
            fprintf(stderr, 
                    "%s: %s\n", LPUGL_ERROR_ERROR_IN_EVENT_HANDLING,
                    lua_tostring(L, -1));
            abort();
        }                                                                   /* -> weakWorld, worldUdata, worldUservalue, error */
        lua_pop(L, 1);                                                      /* -> weakWorld, worldUdata, worldUservalue */
    }                                                                       /* -> weakWorld, worldUdata, worldUservalue */

    lua_settop(L, oldTop);
    return PUGL_SUCCESS;
}

/* ============================================================================================ */

static void registrateBackend(lua_State* L, int worldIdx, int backendIdx)
{
    lua_getuservalue(L, worldIdx);                                   /* -> uservalue */
    if (lua_rawgeti(L, -1, LPUGL_WORLD_UV_BACKENDS) != LUA_TTABLE) { /* -> uservalue, ? */
        lua_pop(L, 1);                                               /* -> uservalue */
        lua_newtable(L);                                             /* -> uservalue, backends */
        lua_newtable(L);                                             /* -> uservalue, backends, meta */
        lua_pushstring(L, "__mode");                                 /* -> uservalue, backends, meta, key */
        lua_pushstring(L, "v");                                      /* -> uservalue, backends, meta, key, value */
        lua_rawset(L, -3);                                           /* -> uservalue, backends, meta */
        lua_setmetatable(L, -2);                                     /* -> uservalue, backends */
        lua_pushvalue(L, -1);                                        /* -> uservalue, backends, backends */
        lua_rawseti(L, -3, LPUGL_WORLD_UV_BACKENDS);                 /* -> uservalue, backends */
    }                                                                /* -> uservalue, backends */
    lua_pushvalue(L, backendIdx);                                    /* -> uservalue, backends, backend */
    lua_rawsetp(L, -2, lua_touserdata(L, backendIdx));               /* -> uservalue, backends */
    lua_pop(L, 2);                                                   /* -> */
}

/* ============================================================================================ */

static void deregistrateBackend(lua_State* L, int worldIdx, int backendIdx)
{
    lua_getuservalue(L, worldIdx);                                   /* -> uservalue */
    if (lua_rawgeti(L, -1, LPUGL_WORLD_UV_BACKENDS) == LUA_TTABLE) { /* -> uservalue, backends */
        lua_pushnil(L);                                              /* -> uservalue, backends, nil */
        lua_rawsetp(L, -2, lua_touserdata(L, backendIdx));           /* -> uservalue, backends */
    }                                                                /* -> uservalue, ? */
    lua_pop(L, 2);
}

/* ============================================================================================ */

static int Lpugl_newWorld(lua_State* L)
{
    luaL_checkstring(L, 1); /* for puglSetClassName */
    
    WorldUserData* udata = lua_newuserdata(L, sizeof(WorldUserData));
    memset(udata, 0, sizeof(WorldUserData));
    udata->magic = LPUGL_WORLD_MAGIC;
    strcpy(udata->versionId, "lpugl.world-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING);
    pushWorldMeta(L);        /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    LpuglWorld* world = calloc(1, sizeof(LpuglWorld));
    if (!world) {
        return lpugl_ERROR_OUT_OF_MEMORY(L);
    }
    async_lock_init(&world->lock);
    udata->world = world;
    atomic_set(&world->used, 1);
    world->id = atomic_inc(&lpugl_id_counter);
    udata->id = world->id;
    world->weakWorldRef = LUA_REFNIL;
    world->registrateBackend = registrateBackend;
    world->deregistrateBackend = deregistrateBackend;

    async_mutex_lock(lpugl_global_lock);
        world->nextWorld = global_world_list;
        global_world_list = world;
    async_mutex_unlock(lpugl_global_lock);

    world->puglWorld = puglNewWorld(PUGL_MODULE, 0);
    if (!world->puglWorld) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    puglSetClassName(world->puglWorld, lua_tostring(L, 1));
    
    // weakWorld[0] = world
    lua_newtable(L);                                        /* -> world, weakWorld*/
    lua_newtable(L);                                        /* -> world, weakWorld, meta */
    lua_pushstring(L, "__mode");                            /* -> world, weakWorld, meta, key */
    lua_pushstring(L, "v");                                 /* -> world, weakWorld, meta, key, value */
    lua_rawset(L, -3);                                      /* -> world, weakWorld, meta */
    lua_setmetatable(L, -2);                                /* -> world, weakWorld */
    lua_pushvalue(L, -2);                                   /* -> world, weakWorld, world */
    lua_rawseti(L, -2, 0);                                  /* -> world, weakWorld */
    world->weakWorldRef = luaL_ref(L, LUA_REGISTRYINDEX);   /* -> world */

    lua_newtable(L);                                /* -> world, uservalue */
    lua_newtable(L);                                /* -> world, uservalue, views */
    lua_rawseti(L, -2, LPUGL_WORLD_UV_VIEWS);        /* -> world, uservalue */
    world->eventL = lua_newthread(L);               /* -> world, uservalue, eventL */
    lua_rawseti(L, -2, LPUGL_WORLD_UV_EVENTL);       /* -> world, uservalue */
    lua_setuservalue(L, -2);                        /* -> world */
    
    puglSetProcessFunc(world->puglWorld, lpugl_world_process, world);
    return 1;
}

/* ============================================================================================ */

static int Lpugl_world(lua_State* L)
{
    lua_Integer id = luaL_checkinteger(L, 1);
    
    WorldUserData* udata = lua_newuserdata(L, sizeof(WorldUserData));
    memset(udata, 0, sizeof(WorldUserData));
    pushWorldMeta(L);        /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    udata->id         = id;
    udata->restricted = true;
    LpuglWorld* w = global_world_list;
    async_mutex_lock(lpugl_global_lock);
        while (w && w->id != id) {
            w = w->nextWorld;
        }
    async_mutex_unlock(lpugl_global_lock);
    if (w) {
        atomic_inc(&w->used);
        udata->world = w;
        return 1;
    } else {
        return lpugl_ERROR_UNKNOWN_OBJECT_world_id(L, id);
    }
}

/* ============================================================================================ */

static void closeAll(lua_State* L, int udata, LpuglWorld* world)
{
    if (lua_getuservalue(L, udata) == LUA_TTABLE)                       /* -> uservalue */
    {
        if (lua_rawgeti(L, LUA_REGISTRYINDEX, 
                           world->weakWorldRef) == LUA_TTABLE)          
        {                                                               /* -> uservalue, weakWorld */
            if (world->viewCount > 0) {
                lua_rawgeti(L, -2, LPUGL_WORLD_UV_VIEWS);               /* -> uservalue, weakWorld, views */
                
                int views = lua_gettop(L);
                
                lua_pushnil(L);                                         /* -> uservalue, weakWorld, views, nil */
                while (lua_next(L, views)) {                            /* -> uservalue, weakWorld, views, key, value */
                    struct ViewUserData* view = lua_touserdata(L, -1);
                    if (view && lpugl_view_close(L, view, lua_gettop(L))) {
                        world->viewCount -= 1;
                    }
                    lua_pushnil(L);                                     /* -> uservalue, weakWorld, views, key, value, nil */
                    lua_rawsetp(L, -5, view);                           /* -> uservalue, weakWorld, views, key, value */
                    lua_pop(L, 1);                                      /* -> uservalue, weakWorld, views, key */
                }                                                       /* -> uservalue, weakWorld, views */
                lua_newtable(L);                                        /* -> uservalue, weakWorld, views, empty views */
                lua_rawseti(L, -4, LPUGL_WORLD_UV_VIEWS);               /* -> uservalue, weakWorld, views */
                lua_pop(L, 2);                                          /* -> uservalue */
            } else {                                                    /* -> uservalue, weakWorld */
                lua_pop(L, 1);                                          /* -> uservalue */
            }
        } else {                                                        /* -> uservalue, ? */
            lua_pop(L, 1);                                              /* -> uservalue */
        }
        if (lua_rawgeti(L, -1, LPUGL_WORLD_UV_BACKENDS)                 /* -> uservalue, ? */
                                                   == LUA_TTABLE) {     /* -> uservalue, backends */
            int backends = lua_gettop(L);                               /* -> uservalue, backends */
            lua_pushnil(L);                                             /* -> uservalue, backends, nil */
            while (lua_next(L, backends)) {                             /* -> uservalue, backends, key, value */
                LpuglBackend* backend = lua_touserdata(L, -1);          /* -> uservalue, backends, key, value */
                if (backend->closeBackend) {
                    backend->closeBackend(L, lua_gettop(L));
                }
                lua_pop(L, 1);                                          /* -> uservalue, backends, key */
            }                                                           /* -> uservalue, backends */
            lua_pushnil(L);                                             /* -> uservalue, backends, nil */
            lua_rawseti(L, -3, LPUGL_WORLD_UV_BACKENDS);                /* -> uservalue, backends */
        }                                                               /* -> uservalue, ? */
        lua_pop(L, 2);                                                  /* -> */
    } else {                                                            /* -> ? */
        lua_pop(L, 1);                                                  /* -> */
    }
}


/* ============================================================================================ */

static void destructWorld(LpuglWorld* world)
{
    async_mutex_lock(lpugl_global_lock);
        if (global_world_list == world) {
            global_world_list = world->nextWorld;
        } else {
            LpuglWorld* w = global_world_list;
            while (w && w->nextWorld != world) {
                w = w->nextWorld;
            }
            if (w) {
                w->nextWorld = world->nextWorld;
            }
        }
    async_mutex_unlock(lpugl_global_lock);
    async_lock_destruct(&world->lock);
    free(world);
}

/* ============================================================================================ */

void lpugl_world_close_pugl(LpuglWorld* world)
{
    async_lock_acquire(&world->lock);
        if (world->puglWorld) {
            puglFreeWorld(world->puglWorld);
            world->puglWorld = NULL;
        }
    async_lock_release(&world->lock);
    
    if (atomic_dec(&world->used) <= 0) {
        destructWorld(world);
    }
}


static void releaseWorldUdata(lua_State* L, int udataIdx, WorldUserData* udata)
{
    LpuglWorld* world = udata->world;
    if (world) {
        if (!udata->restricted) {
            closeAll(L, udataIdx, world);
            if (world->puglWorld) {
                puglSetProcessFunc(world->puglWorld, NULL, NULL);
                puglUpdate(world->puglWorld, 0);
            }
            luaL_unref(L, LUA_REGISTRYINDEX, world->weakWorldRef);
            world->weakWorldRef = LUA_REFNIL;
            if (!world->inCallback) {
                lpugl_world_close_pugl(world);
            } 
            else {
                world->mustClosePugl = true;
            }
        }
        else {
            if (atomic_dec(&world->used) <= 0) {
                destructWorld(world);
            }
        }
        udata->world = NULL;
    }
}

/* ============================================================================================ */

static int World_release(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    releaseWorldUdata(L, 1, udata);
    return 0;
}

/* ============================================================================================ */

static int World_close(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);

    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (udata->world) {
        releaseWorldUdata(L, 1, udata);
    }
    return 0;
}

/* ============================================================================================ */

static int World_setDefaultBackend(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    if (lua_isnil(L, 2)) {
        lua_getuservalue(L, 1);                         /* -> uservalue */
        lua_pushnil(L)     ;                            /* -> uservalue, nil */
        lua_rawseti(L, -2, LPUGL_WORLD_UV_DEFBACKEND);  /* -> uservalue */
    }
    else {
        luaL_checktype(L, 2, LUA_TUSERDATA);
        LpuglBackend* backend = lua_touserdata(L, 2);
        if (!lua_getmetatable(L, 2) || lua_getfield(L, -1, "lpugl.backend") != LUA_TBOOLEAN || !lua_toboolean(L, -1)
         || !backend || backend->magic != LPUGL_BACKEND_MAGIC)
        {
            return luaL_argerror(L, 2, "invalid backend");
        }                                               /* -> meta, backendFlag */
        lua_pop(L, 2);                                  /* -> */
        if (strcmp(backend->versionId, "lpugl.backend-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING) != 0) {
            return luaL_argerror(L, 2, "backend version mismatch");
        }
        if (!backend->world) {
            return luaL_argerror(L, 2, "backend closed");
        }
        if (backend->world != udata->world) {
            return luaL_argerror(L, 2, "backend belongs to other lpugl.world");
        }
        lua_getuservalue(L, 1);                         /* -> uservalue */
        lua_pushvalue(L, 2);                            /* -> uservalue, backend */
        lua_rawseti(L, -2, LPUGL_WORLD_UV_DEFBACKEND);  /* -> uservalue */
    }
    return 0;
}

/* ============================================================================================ */

static int World_getDefaultBackend(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);

    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    lua_getuservalue(L, 1);                         /* -> uservalue */
    lua_rawgeti(L, -1, LPUGL_WORLD_UV_DEFBACKEND);  /* -> uservalue, backend */
    return 1;
}

/* ============================================================================================ */

static int World_toString(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    toLuaString(L, udata);
    return 1;
}

/* ============================================================================================ */

static int World_id(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    lua_pushinteger(L, udata->id);
    return 1;
}

/* ============================================================================================ */

static int World_newView(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    int initArg = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        initArg = 2;
    } else {
        lua_newtable(L);
        initArg = lua_gettop(L);
    }

    lua_getuservalue(L, 1);                     /* -> uservalue */
    lua_rawgeti(L, -1, LPUGL_WORLD_UV_VIEWS);   /* -> uservalue, viewLookup */
    
    int viewLookup = lua_gettop(L);

    if (lua_getfield(L, initArg, "backend") == LUA_TNIL) { /* uservalue, viewLookup, ? */
        lua_pop(L, 1);                                     /* uservalue, viewLookup */
        lua_rawgeti(L, -2, LPUGL_WORLD_UV_DEFBACKEND);     /* uservalue, viewLookup, backend */
        LpuglBackend* backend = lua_touserdata(L, -1);
        if (!backend) {
            return luaL_argerror(L, initArg, "missing backend parameter and no default backend available");
        }
        if (!backend->world) {
            return luaL_argerror(L, initArg, "missing backend parameter and default backend is closed");
        }
        lua_setfield(L, initArg, "backend");               /* uservalue, viewLookup */
    } else {
        lua_pop(L, 1);                                     /* uservalue, viewLookup */
    }
    lpugl_view_new(L, udata->world, initArg, viewLookup);
    
    return 1;
}

/* ============================================================================================ */

static int World_update(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    double timeout = lua_isnone(L, 2) ? -1 : luaL_checknumber(L, 2);
 
    bool wasInCallback = world->inCallback;
    world->inCallback = true;
    
    PuglStatus status = puglUpdate(world->puglWorld, timeout);

    world->inCallback = wasInCallback;
    if (!wasInCallback && world->mustClosePugl) {
        lpugl_world_close_pugl(world);
    }
    if (status == PUGL_SUCCESS || status == PUGL_FAILURE) {
        lua_pushboolean(L, status == PUGL_SUCCESS);
        return 1;
    } else {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
}

/* ============================================================================================ */

static int World_hasViews(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    lua_pushboolean(L, udata->world && udata->world->viewCount > 0);
    return 1;
}

/* ============================================================================================ */

static int World_isClosed(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    lua_pushboolean(L, udata->world == NULL);
    return 1;
}

/* ============================================================================================ */

static int World_viewList(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);

    LpuglWorld* world = udata->world;
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    

    lua_newtable(L);                          /* -> rslt */

    lua_getuservalue(L, 1);                   /* -> rslt, uservalue */
    lua_rawgeti(L, -1, LPUGL_WORLD_UV_VIEWS);  /* -> rslt, uservalue, views */
    
    int views = lua_gettop(L);
    int rslt  = views - 2;
    
    int i = 1;    
    lua_pushnil(L);                /* -> rslt, uservalue, views, nil */
    while (lua_next(L, views)) {   /* -> rslt, uservalue, views, key, value */
        lua_rawseti(L, rslt, i++); /* -> rslt, uservalue, views, key */
    }                              /* -> rslt, uservalue, views */
    lua_pop(L, 2);                 /* -> rslt */
    return 1;
}


/* ============================================================================================ */

static int World_setProcessFunc(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    luaL_checktype(L, 2, LUA_TFUNCTION);
    
    lua_getuservalue(L, 1);                      /* -> uservalue */
    lua_pushvalue(L, 2);                         /* -> uservalue, procFunc */
    lua_rawseti(L, -2, LPUGL_WORLD_UV_PROCFUNC);  /* -> uservalue */
    return 0;
}

/* ============================================================================================ */

static int World_setErrorFunc(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    luaL_checktype(L, 2, LUA_TFUNCTION);
    
    lua_getuservalue(L, 1);                     /* -> uservalue */
    lua_pushvalue(L, 2);                        /* -> uservalue, errFunc */
    lua_rawseti(L, -2, LPUGL_WORLD_UV_ERRFUNC);  /* -> uservalue */
    return 0;
}

/* ============================================================================================ */

static int World_setNextProcessTime(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    luaL_checknumber(L, 2);
    puglSetNextProcessTime(world->puglWorld, lua_tonumber(L, 2));
    return 0;
}

/* ============================================================================================ */

static int World_awake(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
 
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    

    async_lock_acquire(&world->lock);
        if (world->puglWorld && atomic_set_if_equal(&world->awakeSent, 0, 1)) {
            puglAwake(world->puglWorld);
        }
    async_lock_release(&world->lock);    
    return 0;
}

/* ============================================================================================ */

static int World_getTime(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
 
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }    
    lua_pushnumber(L, puglGetTime(world->puglWorld));
    return 1;
}

/* ============================================================================================ */

static int World_setClipboard(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
 
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    size_t len;
    const char* data = luaL_checklstring(L, 2, &len);
    puglSetClipboard(world->puglWorld, NULL, data, len);
    return 0;
}

/* ============================================================================================ */

static int World_hasClipboard(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    LpuglWorld* world = udata->world;
 
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    lua_pushboolean(L, puglHasClipboard(world->puglWorld));
    return 1;
}

/* ============================================================================================ */

static int World_getScreenScale(lua_State* L)
{
    WorldUserData* udata = luaL_checkudata(L, 1, LPUGL_WORLD_CLASS_NAME);
    if (udata->restricted) {
        return lpugl_ERROR_RESTRICTED_ACCESS(L);
    }
    if (!udata->world) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_pushnumber(L, puglGetDefaultScreenScale(udata->world->puglWorld));
    return 1;
}

/* ============================================================================================ */

static const luaL_Reg WorldMethods[] = 
{
    { "setDefaultBackend",  World_setDefaultBackend  },
    { "getDefaultBackend",  World_getDefaultBackend  },
    { "id",                 World_id                 },
    { "newView",            World_newView            },
    { "update",             World_update             },
    { "close",              World_close              },
    { "isClosed",           World_isClosed           },
    { "hasViews",           World_hasViews           },
    { "viewList",           World_viewList           },
    { "setProcessFunc",     World_setProcessFunc     },
    { "setErrorFunc",       World_setErrorFunc       },
    { "setNextProcessTime", World_setNextProcessTime },
    { "awake",              World_awake              },
    { "getTime",            World_getTime            },
    { "setClipboard",       World_setClipboard       },
    { "hasClipboard",       World_hasClipboard       },
    { "getScreenScale",     World_getScreenScale     },
    
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg WorldMetaMethods[] = 
{
    { "__tostring", World_toString },
    { "__gc",       World_release  },

    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newWorld",   Lpugl_newWorld },
    { "world",      Lpugl_world },
    { NULL,         NULL } /* sentinel */
};

static void setupWorldMeta(lua_State* L)
{
    lua_pushstring(L, LPUGL_WORLD_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, WorldMetaMethods, 0);
    
    lua_newtable(L);  /* BufferClass */
        luaL_setfuncs(L, WorldMethods, 0);
    lua_setfield (L, -2, "__index");
}


int lpugl_world_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, LPUGL_WORLD_CLASS_NAME)) {
        setupWorldMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}

