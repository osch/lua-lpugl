#include "lpugl.h"
#include "world.h"
#include "view.h"
#include "error.h"
#include "version.h"

#define LPUGL_MODULE_NAME  "lpugl"

static Mutex         global_mutex;
static AtomicCounter initStage          = 0;
static bool          initialized        = false;
static int           stateCounter       = 0;

Mutex*        lpugl_global_lock = NULL;
AtomicCounter lpugl_id_counter  = 0;

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "Internal error: %s (%s:%d)", text, LPUGL_MODULE_NAME, line);
}*/


static int Lpugl_initApplication(lua_State* L)
{
    PuglApplicationFlags flags = 0;
    if (lua_gettop(L) >= 1) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        bool withMultiThreading = lua_toboolean(L, 1);
        if (withMultiThreading) {
            flags |= PUGL_APPLICATION_THREADS;
        }
    }
    puglInitApplication(flags);
    return 0;
}

static int Lpugl_btest(lua_State* L)
{
    luaL_checkinteger(L, 1);
    lua_Integer rslt = lua_tointeger(L, 1);
    int n = lua_gettop(L);
    for (int i = 2; i <= n; ++i) {
        luaL_checkinteger(L, i);
        rslt &= lua_tointeger(L, i);
    }
    lua_pushboolean(L, rslt);
    return 1;
}

static int Lpugl_type(lua_State* L)
{
    luaL_checkany(L, 1);
    int tp = lua_type(L, 1);
    if (tp == LUA_TUSERDATA) {
        if (lua_getmetatable(L, 1)) {
            if (lua_getfield(L, -1, "__name") == LUA_TSTRING) {
                lua_pushvalue(L, -1);
                if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TTABLE) {
                    if (lua_rawequal(L, -3, -1)) {
                        lua_pop(L, 1);
                        return 1;
                    }
                }
            }
        }
    }
    lua_pushstring(L, lua_typename(L, tp));
    return 1;
}

static const luaL_Reg ModuleFunctions[] = 
{
    { "initApplication", Lpugl_initApplication },
    { "type",            Lpugl_type         },
    { NULL,              NULL } /* sentinel */
};

static int handleClosingLuaState(lua_State* L)
{
    async_mutex_lock(lpugl_global_lock);
    stateCounter -= 1;
    if (stateCounter == 0) {

    }
    async_mutex_unlock(lpugl_global_lock);
    return 0;
}


LPUGL_DLL_PUBLIC int luaopen_lpugl(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */

    /* ---------------------------------------- */

    if (atomic_get(&initStage) != 2) {
        if (atomic_set_if_equal(&initStage, 0, 1)) {
            async_mutex_init(&global_mutex);
            lpugl_global_lock = &global_mutex;
            atomic_set(&initStage, 2);
        } 
        else {
            while (atomic_get(&initStage) != 2) {
                Mutex waitMutex;
                async_mutex_init(&waitMutex);
                async_mutex_lock(&waitMutex);
                async_mutex_wait_millis(&waitMutex, 1);
                async_mutex_destruct(&waitMutex);
            }
        }
    }
    /* ---------------------------------------- */

    async_mutex_lock(lpugl_global_lock);
    {
        if (!initialized) {
            /* create initial id that could not accidently be mistaken with "normal" integers */
            const char* ptr = LPUGL_MODULE_NAME;
            AtomicCounter c = 0;
            if (sizeof(AtomicCounter) - 1 >= 1) {
                int i;
                for (i = 0; i < 2 * sizeof(char*); ++i) {
                    c ^= ((int)(((char*)&ptr)[(i + 1) % sizeof(char*)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
                lua_Number t = lpugl_current_time_seconds();
                for (i = 0; i < 2 * sizeof(lua_Number); ++i) {
                    c ^= ((int)(((char*)&t)[(i + 1) % sizeof(lua_Number)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
            }
            lpugl_id_counter = c;
            initialized = true;
        }


        /* check if initialization has been done for this lua state */
        lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
            lua_rawget(L, LUA_REGISTRYINDEX); 
            bool alreadyInitializedForThisLua = !lua_isnil(L, -1);
        lua_pop(L, 1);
        
        if (!alreadyInitializedForThisLua) 
        {
            stateCounter += 1;
            
            lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
                lua_newuserdata(L, 1); /* sentinel for closing lua state */
                    lua_newtable(L); /* metatable for sentinel */
                        lua_pushstring(L, "__gc");
                            lua_pushcfunction(L, handleClosingLuaState);
                        lua_rawset(L, -3); /* metatable.__gc = handleClosingLuaState */
                    lua_setmetatable(L, -2); /* sets metatable for sentinal table */
            lua_rawset(L, LUA_REGISTRYINDEX); /* sets sentinel as value for unique void* in registry */
        }
    }
    async_mutex_unlock(lpugl_global_lock);

    /* ---------------------------------------- */
    
    int n = lua_gettop(L);
    
    int module      = ++n; lua_newtable(L); 
    int errorModule = ++n; lua_newtable(L);

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    lua_pushliteral(L, LPUGL_VERSION_STRING);
    lua_setfield(L, module, "_VERSION");
    
    {
    #if defined(LPUGL_ASYNC_USE_WIN32)
        #define LPUGL_INFO1 "WIN32"
    #elif defined(LPUGL_ASYNC_USE_GNU)
        #define LPUGL_INFO1 "GNU"
    #elif defined(LPUGL_ASYNC_USE_STDATOMIC)
        #define LPUGL_INFO1 "STD"
    #endif
    #if defined(LPUGL_ASYNC_USE_WINTHREAD)
        #define LPUGL_INFO2 "WIN32"
    #elif defined(LPUGL_ASYNC_USE_PTHREAD)
        #define LPUGL_INFO2 "PTHREAD"
    #elif defined(LPUGL_ASYNC_USE_STDTHREAD)
        #define LPUGL_INFO2 "STD"
    #endif
        lua_pushstring(L, LPUGL_MODULE_NAME
                       " (Version:" LPUGL_VERSION_STRING ",Platform:" LPUGL_PLATFORM_STRING
                       ",Atomic:" LPUGL_INFO1 ",Thread:" LPUGL_INFO2 
                       ",Date:" LPUGL_BUILD_DATE_STRING ")" );
        lua_setfield(L, module, "_INFO");
    }
    
    lua_pushliteral(L, LPUGL_PLATFORM_STRING);      /* -> string */
    lua_setfield(L, module, "platform");            /* -> */
    
    lua_pushinteger(L, PUGL_MOD_SHIFT);             /* -> integer */
    lua_setfield(L, module, "MOD_SHIFT");           /* -> */

    lua_pushinteger(L, PUGL_MOD_CTRL);              /* -> integer */
    lua_setfield(L, module, "MOD_CTRL");            /* -> */

    lua_pushinteger(L, PUGL_MOD_ALT);               /* -> integer */
    lua_setfield(L, module, "MOD_ALT");             /* -> */

    lua_pushinteger(L, PUGL_MOD_SUPER);             /* -> integer */
    lua_setfield(L, module, "MOD_SUPER");           /* -> */

    lua_pushvalue(L, errorModule);
    lua_setfield(L, module, "error");
    
    lua_pushcfunction(L, Lpugl_btest);              /* -> func */
    lua_setfield(L, module, "btest");               /* -> */

    lua_checkstack(L, LUA_MINSTACK);
    
    lpugl_world_init_module  (L, module);
    lpugl_view_init_module   (L, module);
    lpugl_error_init_module  (L, errorModule);

    lua_newtable(L);                                /* -> meta */
    lua_pushliteral(L, "lpugl");                    /* -> meta, string */
    lua_setfield(L, -2, "__metatable");             /* -> meta */
    lua_setmetatable(L, module);                    /* -> */
    
    lua_settop(L, module);
    return 1;
}
