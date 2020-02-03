#include <cairo.h>

#include "init.h"

#if defined(LPUGL_USE_X11)
    #include <X11/X.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <cairo-xlib.h>

#elif defined(LPUGL_USE_WIN)
    #include <Windows.h>
    #include <cairo-win32.h>

#elif defined(LPUGL_USE_MAC)
    #include <cairo-quartz.h>
#endif

#include "pugl/pugl_cairo.h"

#include "lpugl_cairo.h"
#include "world.h"
#include "view.h"
#include "version.h"
#include "backend.h"
#include "error.h"

/* ============================================================================================ */

#define LPUGL_CAIRO_BACKEND_UV_WORLD      0
#define LPUGL_CAIRO_BACKEND_UV_LAYOUT_CTX 1

/* ============================================================================================ */

static const char* const OOCAIRO_MT_NAME_CONTEXT = "6404c570-6711-11dd-b66f-00e081225ce5";

static const char* const LPUGL_CAIRO_BACKEND_CLASS_NAME = "lpugl.cairo.backend";

/* ============================================================================================ */

typedef struct LpuglCairoBackend {

    LpuglBackend     base;
    cairo_surface_t* layoutSurface;
    
#if defined(LPUGL_USE_X11)
    Pixmap           x11LayoutPixmap;

#elif defined(LPUGL_USE_WIN)
    HDC              winLayoutHdc;
#endif

} LpuglCairoBackend;

/* ============================================================================================ */

static void setupBackendMeta(lua_State* L);

static int pushBackendMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LPUGL_CAIRO_BACKEND_CLASS_NAME)) {
        setupBackendMeta(L);
    }
    return 1;
}


/* ============================================================================================ */

static int Backend_getLayoutContext(lua_State* L)
{
    LpuglCairoBackend* udata = luaL_checkudata(L, 1, LPUGL_CAIRO_BACKEND_CLASS_NAME);
    
    if (!udata->base.world) {
        return lpugl_error(L, LPUGL_ERROR_ILLEGAL_STATE ": lpugl.cairo.backend closed");
    }
    
    lua_getuservalue(L, 1);                                            /* -> uservalue */
    if (lua_rawgeti(L, -1, LPUGL_CAIRO_BACKEND_UV_LAYOUT_CTX) == LUA_TUSERDATA)
    {                                                                  /* -> uservalue, context */
        return 1;
    }                                                                  /* -> uservalue, ? */
    lua_pop(L, 1);                                                     /* -> uservalue */
    
    cairo_t** rslt = lua_newuserdata(L, sizeof(cairo_t*));             /* -> uservalue, context */
    *rslt = NULL;

    if (luaL_getmetatable(L, OOCAIRO_MT_NAME_CONTEXT) != LUA_TTABLE) { /* -> uservalue, context, meta */
        lua_pop(L, 1);                                                 /* -> uservalue, context */
        bool loaded = false;
        if (lua_getglobal(L, "require") == LUA_TFUNCTION) {            /* -> uservalue, context, require */
            lua_pushstring(L, "oocairo");                              /* -> uservalue, context, require, "oocairo" */
            lua_call(L, 1, 0);                                         /* -> uservalue, context */
    
            if (luaL_getmetatable(L, OOCAIRO_MT_NAME_CONTEXT) == LUA_TTABLE) { /* -> uservalue, context, meta */
                loaded = true;
            }
        }
        if (!loaded) { /* -> context, meta */
            return lpugl_error(L, LPUGL_ERROR_FAILED_OPERATION ": error loading module \"oocairo\"");
        }
    }                                                                  /* -> uservalue, context, meta */
    lua_setmetatable(L, -2);                                           /* -> uservalue, context */
    lua_pushvalue(L, -1);                                              /* -> uservalue, context, context */
    lua_rawseti(L, -3, LPUGL_CAIRO_BACKEND_UV_LAYOUT_CTX);             /* -> uservalue, context */
    
#if defined(LPUGL_USE_X11)
    Display*     display = (Display*) puglCairoBackendGetNativeWorld(udata->base.world->puglWorld);
    int          screen  = XDefaultScreen(display);
    XVisualInfo  pat; pat.screen = screen;
    int          n;
    XVisualInfo* vi = XGetVisualInfo(display, VisualScreenMask, &pat, &n);

    udata->x11LayoutPixmap = XCreatePixmap(display, XRootWindow(display, screen), 10, 10, vi->depth);
    udata->layoutSurface   = cairo_xlib_surface_create(display, udata->x11LayoutPixmap, vi->visual, 10, 10);
    XFree(vi);

#elif defined(LPUGL_USE_WIN)
    HDC hdc              = CreateCompatibleDC(NULL);
    udata->winLayoutHdc  = hdc;
    udata->layoutSurface = cairo_win32_surface_create(hdc);

#elif defined(LPUGL_USE_MAC)
    CGContextRef cgContext = (CGContextRef)puglCairoBackendGetNativeWorld(udata->base.world->puglWorld);
    cairo_surface_t* tmp = cairo_quartz_surface_create_for_cg_context(cgContext, 10, 10);
    if (tmp) {
        udata->layoutSurface = cairo_surface_create_similar(tmp, CAIRO_CONTENT_COLOR_ALPHA, 10, 10);
        cairo_surface_destroy(tmp);
    }
#endif

    *rslt = cairo_create(udata->layoutSurface);
    return 1;
}

/* ============================================================================================ */

static int newDrawContext(lua_State* L, void* nativeContext)           /* -> viewUserValue */
{
    cairo_t** rslt = lua_newuserdata(L, sizeof(cairo_t*));             /* -> viewUV, context */
    *rslt = NULL;
    
    if (luaL_getmetatable(L, OOCAIRO_MT_NAME_CONTEXT) != LUA_TTABLE) { /* -> viewUV, context, meta */
        lua_pop(L, 1);                                                 /* -> viewUV, context */
        bool loaded = false;
        if (lua_getglobal(L, "require") == LUA_TFUNCTION) {            /* -> viewUV, context, require */
            lua_pushstring(L, "oocairo");                              /* -> viewUV, context, require, "oocairo" */
            lua_call(L, 1, 0);                                         /* -> viewUV, context */
    
            if (luaL_getmetatable(L, OOCAIRO_MT_NAME_CONTEXT) == LUA_TTABLE) { /* -> viewUV, context, meta */
                loaded = true;
            }
        }
        if (!loaded) { /* -> viewUV, context, meta */
            return lpugl_error(L, LPUGL_ERROR_FAILED_OPERATION ": error loading module \"oocairo\"");
        }
    }
    lua_setmetatable(L, -2);                    /* -> viewUV, context */
    lua_pushvalue(L, -1);                       /* -> viewUV, context, context */
    lua_rawseti(L, -3, LPUGL_VIEW_UV_DRAWCTX);  /* -> viewUV, context */
    
    *rslt = cairo_reference(nativeContext);
    
    return 1;
}

/* ============================================================================================ */

static int finishDrawContext(lua_State* L, int contextIdx)
{
    cairo_t** context = lua_touserdata(L, contextIdx);   /* -> context */
    if (*context) {
        cairo_destroy(*context);
        *context = NULL;
    }
    return 0;
}

/* ============================================================================================ */

static void closeBackend(lua_State* L, int backendIdx)
{
    LpuglCairoBackend* udata = lua_touserdata(L, backendIdx);
    if (lua_getuservalue(L, backendIdx) == LUA_TTABLE) {       /* -> uservalue */
        lua_pushnil(L);                                        /* -> uservalue, nil */
        lua_rawseti(L, -2, LPUGL_CAIRO_BACKEND_UV_WORLD);      /* -> uservalue */

        if (lua_rawgeti(L, -1, LPUGL_CAIRO_BACKEND_UV_LAYOUT_CTX) == LUA_TUSERDATA)
        {                                                      /* -> uervalue, layoutCtx */
            cairo_t** context = lua_touserdata(L, -1);
            if (*context) {
                cairo_destroy(*context);
                *context = NULL;
            }
        }                                                      /* -> uervalue, ? */
        lua_pop(L, 1);                                         /* -> uservalue */
        lua_pushnil(L);                                        /* -> uservalue, nil */
        lua_rawseti(L, -2, LPUGL_CAIRO_BACKEND_UV_LAYOUT_CTX); /* -> uservalue */
    }                                                          /* -> ? */
    lua_pop(L, 1);                                             /* -> */

    if (udata->layoutSurface) {
        cairo_surface_destroy(udata->layoutSurface);
        udata->layoutSurface = NULL;
    }
#if defined(LPUGL_USE_X11)
    if (udata->x11LayoutPixmap) {
        XFreePixmap((Display*) puglCairoBackendGetNativeWorld(udata->base.world->puglWorld), 
                    udata->x11LayoutPixmap);
        udata->x11LayoutPixmap = 0;
    }
#elif defined(LPUGL_USE_WIN)
    if (udata->winLayoutHdc) {
        DeleteObject(udata->winLayoutHdc);
        udata->winLayoutHdc = 0;
    }
#endif
    udata->base.world = NULL;
}

/* ============================================================================================ */

static int Backend_close(lua_State* L)
{
    LpuglCairoBackend* udata = luaL_checkudata(L, 1, LPUGL_CAIRO_BACKEND_CLASS_NAME);
    if (udata->base.world) {
        if (udata->base.used > 0) {
            return luaL_error(L, "backend is used by view");
        }
        if (lua_getuservalue(L, 1) == LUA_TTABLE) {             /* -> uservalue */
            if (lua_rawgeti(L, -1, LPUGL_CAIRO_BACKEND_UV_WORLD)
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
    LpuglCairoBackend* udata = luaL_checkudata(L, 1, LPUGL_CAIRO_BACKEND_CLASS_NAME);
    lua_pushboolean(L, udata->base.world == NULL);
    return 1;
}

/* ============================================================================================ */

static int Lpugl_cairo_newBackend(lua_State* L)
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

    LpuglCairoBackend* udata = lua_newuserdata(L, sizeof(LpuglCairoBackend));
    memset(udata, 0, sizeof(LpuglCairoBackend));
    
    udata->base.magic = LPUGL_BACKEND_MAGIC;
    strcpy(udata->base.versionId, "lpugl.backend-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING);
    udata->base.puglBackend       = puglCairoBackend();
    udata->base.newDrawContext    = newDrawContext;
    udata->base.finishDrawContext = finishDrawContext;
    udata->base.closeBackend      = closeBackend;

    pushBackendMeta(L);      /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    
    worldUdata->world->registrateBackend(L, 1, lua_gettop(L)); /* -> udata */
    udata->base.world = worldUdata->world;
    lua_newtable(L);                                           /* -> udata, uservalue */
    lua_pushvalue(L, 1);                                       /* -> udata, uservalue, world */
    lua_rawseti(L, -2, LPUGL_CAIRO_BACKEND_UV_WORLD);          /* -> udata, uservalue */
    lua_setuservalue(L, -2);                                   /* -> udata */
    
    return 1;
}

/* ============================================================================================ */

static int Lpugl_cairo_newWorld(lua_State* L)
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
                lua_pushcfunction(L, Lpugl_cairo_newBackend);    /* -> lpugl, world, setDefaultBackend(), world, newBackend() */
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
    { "close",            Backend_close            },
    { "isClosed",         Backend_isClosed         },
    { "getLayoutContext", Backend_getLayoutContext },
    { NULL,               NULL } /* sentinel */
};

static const luaL_Reg BackendMetaMethods[] = 
{
    { "__gc",         Backend_close  },
    { NULL,           NULL           } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newBackend",   Lpugl_cairo_newBackend },
    { "newWorld",     Lpugl_cairo_newWorld   },
    { NULL,           NULL } /* sentinel */
};

static void setupBackendMeta(lua_State* L)
{
    lua_pushstring(L, LPUGL_CAIRO_BACKEND_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");
    
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "lpugl.backend");

    luaL_setfuncs(L, BackendMetaMethods, 0);
    
    lua_newtable(L);  /* BackendClass */
        luaL_setfuncs(L, BackendMethods, 0);
    lua_setfield (L, -2, "__index");
}

LPUGL_DLL_PUBLIC int luaopen_lpugl_cairo(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */
    
    if (luaL_newmetatable(L, LPUGL_CAIRO_BACKEND_CLASS_NAME)) {
        setupBackendMeta(L);
    }
    lua_pop(L, 1);

    int n = lua_gettop(L);
    
    int module = ++n; lua_newtable(L);                      /* -> module */

    lua_pushliteral(L, LPUGL_VERSION_STRING);               /* -> module, version */
    lua_setfield(L, module, "_VERSION");                    /* -> module */
    lua_pushstring(L, "lpugl.cairo"
                   " (Version:" LPUGL_VERSION_STRING 
                   ",Platform:" LPUGL_PLATFORM_STRING
                   ",Date:" LPUGL_BUILD_DATE_STRING ")" );  /* -> module, info */
    lua_setfield(L, module, "_INFO");                       /* -> module */
    lua_pushliteral(L, LPUGL_PLATFORM_STRING);              /* -> module, string */
    lua_setfield(L, module, "PLATFORM");                    /* -> module */
    luaL_setfuncs(L, ModuleFunctions, 0);                   /* -> module */

    lua_newtable(L);                                        /* -> module, meta */
    if (lua_getglobal(L, "require") == LUA_TFUNCTION) {     /* -> module, meta, require */
        lua_pushstring(L, "lpugl");                         /* -> module, meta, require, "lpugl" */
        lua_call(L, 1, 1);                                  /* -> module, meta, lpugl */
        lua_setfield(L, -2, "__index");                     /* -> module, meta */
        lua_pushliteral(L, "lpugl.cairo");                  /* -> module, meta, string */
        lua_setfield(L, -2, "__metatable");                 /* -> module, meta */
        lua_setmetatable(L, module);                        /* -> module */
        return 1;
    } else {
        return lpugl_error(L, LPUGL_ERROR_FAILED_OPERATION ": missing global function \"require\"");
    }
}

