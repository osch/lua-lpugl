#include <math.h>

#include "base.h"

#include "pugl/pugl.h"
#include "pugl/cairo.h"

#include "util.h"
#include "lpugl.h"
#include "view.h"
#include "world.h"
#include "error.h"
#include "version.h"
#include "backend.h"

/* ============================================================================================ */

static const char* const LPUGL_VIEW_CLASS_NAME = "lpugl.view";

typedef struct ViewUserData {
    LpuglWorld*   world;
    LpuglBackend* backend;
    PuglView*     puglView;
    int           eventFuncNargs;
    bool          isChild;
    bool          isPopup;
    bool          drawing;
} ViewUserData;

/* ============================================================================================ */

static void closeView(lua_State* L, ViewUserData* udata, int udataIdx);

static void setupViewMeta(lua_State* L);

static int pushViewMeta(lua_State* L)
{
    if (luaL_newmetatable(L, LPUGL_VIEW_CLASS_NAME)) {
        setupViewMeta(L);
    }
    return 1;
}

/* ============================================================================================ */

static const char* puglKeyToName(PuglKey key) 
{
    switch (key) {
        case PUGL_KEY_BACKSPACE:    return "Backspace";
        case PUGL_KEY_TAB:          return "Tab";
        case PUGL_KEY_RETURN:       return "Return";
        case PUGL_KEY_ESCAPE:       return "Escape";
        case PUGL_KEY_SPACE:        return "Space";
        case PUGL_KEY_DELETE:       return "Delete";
        
        // Unicode Private Use Area
        case PUGL_KEY_F1:           return "F1";
        case PUGL_KEY_F2:           return "F2";
        case PUGL_KEY_F3:           return "F3";
        case PUGL_KEY_F4:           return "F4";
        case PUGL_KEY_F5:           return "F5";
        case PUGL_KEY_F6:           return "F6";
        case PUGL_KEY_F7:           return "F7";
        case PUGL_KEY_F8:           return "F8";
        case PUGL_KEY_F9:           return "F9";
        case PUGL_KEY_F10:          return "F10";
        case PUGL_KEY_F11:          return "F11";
        case PUGL_KEY_F12:          return "F12";
        case PUGL_KEY_LEFT:         return "Left";
        case PUGL_KEY_UP:           return "Up";
        case PUGL_KEY_RIGHT:        return "Right";
        case PUGL_KEY_DOWN:         return "Down";
        case PUGL_KEY_PAGE_UP:      return "Page_Up";
        case PUGL_KEY_PAGE_DOWN:    return "Page_Down";
        case PUGL_KEY_HOME:         return "Home";
        case PUGL_KEY_END:          return "End";
        case PUGL_KEY_INSERT:       return "Insert";
        case PUGL_KEY_SHIFT_L:      return "Shift_L";
        case PUGL_KEY_SHIFT_R:      return "Shift_R";
        case PUGL_KEY_CTRL_L:       return "Ctrl_L";
        case PUGL_KEY_CTRL_R:       return "Ctrl_R";
        case PUGL_KEY_ALT_L:        return "Alt_L";
        case PUGL_KEY_ALT_R:        return "Alt_R";
        case PUGL_KEY_SUPER_L:      return "Super_L";
        case PUGL_KEY_SUPER_R:      return "Super_R";
        case PUGL_KEY_MENU:         return "Menu";
        case PUGL_KEY_CAPS_LOCK:    return "Caps_Lock";
        case PUGL_KEY_SCROLL_LOCK:  return "Scroll_Lock";
        case PUGL_KEY_NUM_LOCK:     return "Num_Lock";
        case PUGL_KEY_PRINT_SCREEN: return "Print_Screen";
        case PUGL_KEY_PAUSE:        return "Pause";

        case PUGL_KEY_KP_ENTER:     return "KP_Enter";
        case PUGL_KEY_KP_HOME:      return "KP_Home";
        case PUGL_KEY_KP_LEFT:      return "KP_Left";
        case PUGL_KEY_KP_UP:        return "KP_Up";
        case PUGL_KEY_KP_RIGHT:     return "KP_Right";
        case PUGL_KEY_KP_DOWN:      return "KP_Down";
        case PUGL_KEY_KP_PAGE_UP:   return "KP_Page_Up";
        case PUGL_KEY_KP_PAGE_DOWN: return "KP_Page_Down";
        case PUGL_KEY_KP_END:       return "KP_End";
        case PUGL_KEY_KP_BEGIN:     return "KP_Begin";
        case PUGL_KEY_KP_INSERT:    return "KP_Insert";
        case PUGL_KEY_KP_DELETE:    return "KP_Delete";
        case PUGL_KEY_KP_MULTIPLY:  return "KP_Multiply";
        case PUGL_KEY_KP_ADD:       return "KP_Add";
        case PUGL_KEY_KP_SUBTRACT:  return "KP_Subtract";
        case PUGL_KEY_KP_DIVIDE:    return "KP_divide";
        
        case PUGL_KEY_KP_0:         return "KP_0";
        case PUGL_KEY_KP_1:         return "KP_1";
        case PUGL_KEY_KP_2:         return "KP_2";
        case PUGL_KEY_KP_3:         return "KP_3";
        case PUGL_KEY_KP_4:         return "KP_4";
        case PUGL_KEY_KP_5:         return "KP_5";
        case PUGL_KEY_KP_6:         return "KP_6";
        case PUGL_KEY_KP_7:         return "KP_7";
        case PUGL_KEY_KP_8:         return "KP_8";
        case PUGL_KEY_KP_9:         return "KP_9";
        case PUGL_KEY_KP_SEPARATOR: return "KP_Separator";
        
        default:                    return NULL;
    }
}

/* ============================================================================================ */

static int convertUnicodeCharToUtf8(uint32_t c, char* rsltBuffer)
{
    if (c <= 0x7F) {
        rsltBuffer[0] = ((unsigned char)c);
        return 1;
    } else if (c <= 0x7FF) {
        rsltBuffer[0] = ((unsigned char)(0xC0 | ((c >> 6) & 0x1F)));
        rsltBuffer[1] = ((unsigned char)(0x80 | (c & 0x3F)));
        return 2;
    } else if (c <= 0xFFFF) {
        rsltBuffer[0] = ((unsigned char)(0xE0 | ((c >> 12) & 0x0F)));
        rsltBuffer[1] = ((unsigned char)(0x80 | ((c >> 6) & 0x3F)));
        rsltBuffer[2] = ((unsigned char)(0x80 | (c & 0x3F)));
        return 3;
    } else {
        rsltBuffer[0] = ((unsigned char)(0xF0 | ((c >> 18) & 0x07)));
        rsltBuffer[1] = ((unsigned char)(0x80 | ((c >> 12) & 0x3f)));
        rsltBuffer[2] = ((unsigned char)(0x80 | ((c >> 6) & 0x3f)));
        rsltBuffer[3] = ((unsigned char)(0x80 | (c & 0x3f)));
        return 4;
    }
}

/* ============================================================================================ */

static PuglStatus handleEvent(PuglView* view, const PuglEvent* event)
{
    if (event->type == PUGL_DESTROY) {
        return PUGL_SUCCESS; // lua view object is not functional at this point
    }
    ViewUserData* udata = puglGetHandle(view);
    if (!udata || !udata->world) {
        fprintf(stderr, "lpugl: internal error in view.c:%d\n", __LINE__);
        abort();
    }
    LpuglWorld* world = udata->world;
    if (!world->eventL) {
        fprintf(stderr, "lpugl: internal error in view.c:%d\n", __LINE__);
        abort();
    }

    int nargs = udata->eventFuncNargs;
    if (nargs < 0) { // missing event handling function
        return PUGL_SUCCESS;
    }

    bool wasInCallback = world->inCallback;
    world->inCallback = true;

    lua_State* L = world->eventL;
    int oldTop = lua_gettop(L);
    
    lua_checkstack(L, LUA_MINSTACK);

    if (   lua_rawgeti(L, LUA_REGISTRYINDEX, world->weakWorldRef) != LUA_TTABLE /* -> weakWorld */
        || lua_rawgetp(L, -1, udata) != LUA_TUSERDATA                           /* -> weakWorld, viewUdata */
        || lua_getuservalue(L, -1) != LUA_TTABLE)                               /* -> weakWorld, viewUdata, viewUservalue */
    {
        fprintf(stderr, "lpugl: internal error in view.c:%d\n", __LINE__);
        abort();
    }
    int uservalue = lua_gettop(L);
    int udataIdx  = uservalue - 1;
    int weakWorld = uservalue - 2;
    
    const char* eventName = NULL;
    switch (event->type) {
        case PUGL_CREATE:             eventName = "CREATE"; break;
        case PUGL_CONFIGURE:          eventName = "CONFIGURE"; break;
        case PUGL_MAP:                eventName = "MAP"; break;
        case PUGL_UNMAP:              eventName = "UNMAP"; break;
        case PUGL_BUTTON_PRESS:       eventName = "BUTTON_PRESS"; break;
        case PUGL_BUTTON_RELEASE:     eventName = "BUTTON_RELEASE"; break;
        case PUGL_KEY_PRESS:          eventName = "KEY_PRESS"; break;
        case PUGL_KEY_RELEASE:        eventName = "KEY_RELEASE"; break;
        case PUGL_POINTER_IN:         eventName = "POINTER_IN"; break;
        case PUGL_POINTER_OUT:        eventName = "POINTER_OUT"; break;
        case PUGL_MOTION:             eventName = "MOTION"; break;
        case PUGL_SCROLL:             eventName = "SCROLL"; break;
        case PUGL_FOCUS_IN:           eventName = "FOCUS_IN"; break;
        case PUGL_FOCUS_OUT:          eventName = "FOCUS_OUT"; break;
        case PUGL_EXPOSE:             eventName = "EXPOSE"; break;
        case PUGL_CLOSE:              eventName = "CLOSE"; break;
        case PUGL_MUST_FREE:          closeView(L, udata, udataIdx); break;
        case PUGL_DATA_RECEIVED:      eventName = "DATA_RECEIVED"; break;
        
        case PUGL_NOTHING:
        case PUGL_DESTROY:
        case PUGL_UPDATE:
        case PUGL_CLIENT:
        case PUGL_LOOP_ENTER:
        case PUGL_LOOP_LEAVE:         eventName = NULL; break;
    }
    if (eventName) {
        world->hadEvent = true;
        
        lua_pushcfunction(L, lpugl_world_errormsghandler);
        int msgh = lua_gettop(L);
        for (int i = 0; i <= nargs; ++i) {
            lua_rawgeti(L, uservalue, LPUGL_VIEW_UV_EVENTFUNC + i);
        }
        lua_pushvalue(L, udataIdx); ++nargs;
        lua_pushstring(L, eventName); ++nargs;
        bool lastExposure = false;
        switch (event->type) {
            case PUGL_BUTTON_PRESS:    
            case PUGL_BUTTON_RELEASE: {
                lua_pushinteger(L, (int)floor(event->button.x + 0.5)); ++nargs;
                lua_pushinteger(L, (int)floor(event->button.y + 0.5)); ++nargs;
                lua_pushinteger(L, event->button.button);              ++nargs;
                lua_pushinteger(L, event->button.state);               ++nargs;
                break;
            }
            case PUGL_KEY_PRESS:    
            case PUGL_KEY_RELEASE: {
                if (event->key.key) {
                    const char* keyName = puglKeyToName(event->key.key);
                    if (keyName) {
                        lua_pushstring(L, keyName); ++nargs;
                    } else {
                        char keyAsString[4];
                        int len = convertUnicodeCharToUtf8(event->key.key, keyAsString);
                        lua_pushlstring(L, keyAsString, len); ++nargs;
                    }
                } else {
                    lua_pushnil(L); ++nargs;
                }
                lua_pushinteger(L, event->key.state);  ++nargs;
                if (event->key.inputLength > 0) {
                    if (event->key.inputLength <= 8) {
                        lua_pushlstring(L, event->key.input.data,
                                           event->key.inputLength); ++nargs;
                    } else {
                        lua_pushlstring(L, event->key.input.ptr,
                                           event->key.inputLength); ++nargs;
                    }
                } else {
                    lua_pushnil(L); ++nargs;
                }
                break;
            }
            case PUGL_POINTER_IN: 
            case PUGL_POINTER_OUT: {
                lua_pushinteger(L, (int)floor(event->crossing.x + 0.5)); ++nargs;
                lua_pushinteger(L, (int)floor(event->crossing.y + 0.5)); ++nargs;
                break;
            }
            case PUGL_MOTION: {
                lua_pushinteger(L, (int)floor(event->motion.x + 0.5)); ++nargs;
                lua_pushinteger(L, (int)floor(event->motion.y + 0.5)); ++nargs;
                break;
            }
            case PUGL_SCROLL: {
                lua_pushnumber(L, event->scroll.dx); ++nargs;
                lua_pushnumber(L, event->scroll.dy); ++nargs;
                break;
            }
            case PUGL_FOCUS_IN:
            case PUGL_FOCUS_OUT: {
                break;
            }
            case PUGL_EXPOSE: {
                int x1 = (int)floor(event->expose.x);
                int y1 = (int)floor(event->expose.y);
                int x2 = (int)ceil (event->expose.x + event->expose.width);
                int y2 = (int)ceil (event->expose.y + event->expose.height);
                lua_pushinteger(L, x1);                  ++nargs;
                lua_pushinteger(L, y1);                  ++nargs;
                lua_pushinteger(L, x2 - x1);             ++nargs;
                lua_pushinteger(L, y2 - y1);             ++nargs;
                lua_pushinteger(L, event->expose.count); ++nargs;
                lua_pushboolean(L, !udata->drawing);     ++nargs; // isFirst
                udata->drawing = true;
                lastExposure = (event->expose.count == 0);
                break;
            }
            case PUGL_CONFIGURE: {
                int x1 = (int)floor(event->configure.x);
                int y1 = (int)floor(event->configure.y);
                int x2 = (int)ceil (event->configure.x + event->configure.width);
                int y2 = (int)ceil (event->configure.y + event->configure.height);
                lua_pushinteger(L, x1);                  ++nargs;
                lua_pushinteger(L, y1);                  ++nargs;
                lua_pushinteger(L, x2 - x1);             ++nargs;
                lua_pushinteger(L, y2 - y1);             ++nargs;
                break;
            }
            case PUGL_DATA_RECEIVED: {
                lua_pushlstring(L, event->received.data, 
                                   event->received.len); ++nargs;
                break;
            }
            default: 
                break;
        }
        int rc = lua_pcall(L, nargs, 0, msgh);

        world->inCallback = wasInCallback;
        if (!wasInCallback && world->mustClosePugl) {
            lpugl_world_close_pugl(world);
        }
    
        if (udata->drawing && lastExposure) {
            udata->drawing = false;
            if (lua_rawgeti(L, uservalue, LPUGL_VIEW_UV_DRAWCTX) == LUA_TUSERDATA)
            {                                                     /* -> context */
                if (udata->backend->finishDrawContext) {
                    udata->backend->finishDrawContext(L, lua_gettop(L));
                }
                lua_pushnil(L);                                   /* -> context, nil */
                lua_setmetatable(L, -2);                          /* -> context */
                lua_pushnil(L);                                   /* -> context, nil */
                lua_rawseti(L, uservalue, LPUGL_VIEW_UV_DRAWCTX); /* -> context */
            }
            lua_pop(L, 1);                                        /* -> */
        }
        
        if (rc != 0) {                                                              /* -> error */
            bool handled = false;
            int error = lua_gettop(L);
            if (lua_rawgeti(L, weakWorld, 0) == LUA_TUSERDATA                       /* -> error, world */
             && lua_getuservalue(L, -1) == LUA_TTABLE)                              /* -> error, world, worldUserValue */
            {
                if (lua_rawgeti(L, -1, LPUGL_WORLD_UV_ERRFUNC) == LUA_TFUNCTION)     /* -> error, world, worldUserValue, errFunc */
                {
                    lua_rawgeti(L, -4, 1);                                          /* -> error, world, worldUserValue, errFunc, error[1] */
                    lua_rawgeti(L, -5, 2);                                          /* -> error, world, worldUserValue, errFunc, error[1], error[2] */
                    int rc2 = lua_pcall(L, 2, 0, msgh);                             /* -> error, world, worldUserValue, ? */
                    if (rc2 == 0) {
                        handled = true;                                             /* -> error, world, worldUserValue */
                    } else {                                                        /* -> error, world, worldUserValue, error2 */
                        lua_rawgeti(L, -1, 1);                                      /* -> error, world, worldUserValue, error2, errmsg2 */
                        fprintf(stderr, 
                                "%s: %s\n", LPUGL_ERROR_ERROR_IN_ERROR_HANDLING,
                                lua_tostring(L, -1));
                    }
                }
            } else {
                fprintf(stderr, "lpugl: internal error in view.c:%d\n", __LINE__);
                abort();
            }
            if (!handled) {
                lua_rawgeti(L, error, 1);                                          /* -> errmsg */
                fprintf(stderr, 
                        "%s: %s\n", LPUGL_ERROR_ERROR_IN_EVENT_HANDLING,
                        lua_tostring(L, -1));
                abort();
            }
        }
    }
    
    lua_settop(L, oldTop);
    return PUGL_SUCCESS;
}

/* ============================================================================================ */

// value must be on top of stack
static bool checkArgTableValueType(lua_State* L, int argTable, const char* key, const char* expectedKey, int expectedType)
{
    if (strcmp(key, expectedKey) == 0) {
        if (lua_type(L, -1) == expectedType) {
            return true;
        } else {
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "%s expected as '%s' value, got %s", 
                                             lua_typename(L, expectedType),
                                             expectedKey, 
                                             luaL_typename(L, -1)));
        }
    }
    return false;
}
// value must be on top of stack
static bool checkArgTableValueType2(lua_State* L, int argTable, const char* key, const char* expectedKey, int expectedType1, int expectedType2)
{
    if (strcmp(key, expectedKey) == 0) {
        if (lua_type(L, -1) == expectedType1 || lua_type(L, -1) == expectedType2) {
            return true;
        } else {
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "%s or %s expected as '%s' value, got %s", 
                                             lua_typename(L, expectedType1),
                                             lua_typename(L, expectedType2),
                                             expectedKey, 
                                             luaL_typename(L, -1)));
        }
    }
    return false;
}

// value must be on top of stack
static bool checkArgTableValueUdata(lua_State* L, int argTable, const char* key, const char* expectedKey, const char* expectedType)
{
    if (strcmp(key, expectedKey) == 0) {
        if (luaL_testudata(L, -1, expectedType)) {
            return true;
        } else {
            const char* tname = luaL_typename(L, -1);
            void* udata = lua_touserdata(L, -1);
            if (udata && lua_getmetatable(L, -1)) {                 /* -> meta */
                if (lua_getfield(L, -1, "__name") == LUA_TSTRING) { /* -> meta, name */
                    tname = lua_tostring(L, -1);
                }
            }
            return luaL_argerror(L, argTable, 
                                    lua_pushfstring(L, "%s expected as '%s' value, got %s", 
                                             expectedType,
                                             expectedKey, 
                                             tname));
        }
    }
    return false;
}

// value must be on top of stack
static bool checkArgTableValueUdataOrLightUdata(lua_State* L, int argTable, const char* key, const char* expectedKey, const char* expectedType)
{
    if (strcmp(key, expectedKey) == 0) {
        if (lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
            return true;
        } else {
            return checkArgTableValueUdata(L, argTable, key, expectedKey, expectedType);
        }
    }
    return false;
}

int lpugl_view_new(lua_State* L, LpuglWorld* world, int initArg, int viewLookup)
{
    ViewUserData* udata = lua_newuserdata(L, sizeof(ViewUserData));
    memset(udata, 0, sizeof(ViewUserData));
    udata->eventFuncNargs = -1;
    pushViewMeta(L);         /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    
    udata->world = world;
    world->viewCount += 1;
    
    udata->puglView = puglNewView(world->puglWorld);
    if (!udata->puglView) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    puglSetHandle(udata->puglView, udata);
    puglSetEventFunc(udata->puglView, handleEvent);
    
    lua_pushvalue(L, -1);               /* -> udata, udata */
    lua_rawsetp(L, viewLookup, udata);  /* -> udata */

    lua_rawgeti(L, LUA_REGISTRYINDEX, world->weakWorldRef); /* -> udata, weakWorld */
    lua_pushvalue(L, -2);                                   /* -> udata, weakWorld, udata */
    lua_rawsetp(L, -2, udata);                              /* -> udata, weakWorld */
    lua_pop(L, 1);                                          /* -> udata */
    
    // uservalue[-1]  = cairo_context
    // uservalue[ 0]  = eventFunc
    // uservalue[ 1]  = eventFuncArg1...
    lua_newtable(L);                    /* -> udata, uservalue */
    lua_setuservalue(L, -2);            /* -> udata */
    
    char* title = NULL;
    LpuglBackend* backend = NULL;
    bool hasEventFunc = false;
    bool hasTransient = false;
    bool hasPopup = false;
    bool hasParent = false;
    bool isResizable =  false;
    bool dontMergeRects = false;
    bool useDoubleBuffer = false;
    {
        lua_pushnil(L);                 /* -> udata, nil */
        while (lua_next(L, initArg)) {  /* -> udata, key, value */
            if (!lua_isstring(L, -2)) {
                return luaL_argerror(L, initArg, 
                                     lua_pushfstring(L, "got table key of type %s, but string expected", 
                                                     lua_typename(L, lua_type(L, -2))));
            }
            const char* key = lua_tostring(L, -2);
            
            if (checkArgTableValueType(L, initArg, key, "title", LUA_TSTRING)) 
            {
                size_t len;
                const char* value = lua_tolstring(L, -1, &len);
                title = malloc(len + 1);
                if (title) {
                    memcpy(title, value, len + 1);
                }
            }
            else if (checkArgTableValueType(L, initArg, key, "resizable", LUA_TBOOLEAN))
            {
                isResizable = lua_toboolean(L, -1);
            }
            else if (checkArgTableValueType(L, initArg, key, "useDoubleBuffer", LUA_TBOOLEAN))
            {
                useDoubleBuffer = lua_toboolean(L, -1);
            }
            else if (checkArgTableValueType(L, initArg, key, "dontMergeRects", LUA_TBOOLEAN))
            {
                dontMergeRects = lua_toboolean(L, -1);
                puglSetViewHint(udata->puglView, PUGL_DONT_MERGE_RECTS, dontMergeRects);
            }
            else if (checkArgTableValueType(L, initArg, key, "backgroundColor", LUA_TNUMBER))
            {
                puglSetBackgroundColor(udata->puglView, lua_tointeger(L, -1));
            }
            else if (checkArgTableValueType(L, initArg, key, "size", LUA_TTABLE)) 
            {                                                 /* -> udata, key, value */
                if (   lua_rawgeti(L, -1, 1) != LUA_TNUMBER   /* -> udata, key, value, w */
                    || lua_rawgeti(L, -2, 2) != LUA_TNUMBER)  /* -> udata, key, value, w, h */
                {
                    return luaL_argerror(L, initArg, "invalid 'size' value");
                }
                int w = floor(lua_tonumber(L, -2) + 0.5);
                int h = floor(lua_tonumber(L, -1) + 0.5);
                lua_pop(L, 2);                                /* -> udata, key, value */
                
                puglSetSize(udata->puglView, w, h);
            }
            else if (checkArgTableValueType(L, initArg, key, "minSize", LUA_TTABLE)) 
            {                                                 /* -> udata, key, value */
                if (   lua_rawgeti(L, -1, 1) != LUA_TNUMBER   /* -> udata, key, value, w */
                    || lua_rawgeti(L, -2, 2) != LUA_TNUMBER)  /* -> udata, key, value, w, h */
                {
                    return luaL_argerror(L, initArg, "invalid 'minSize' value");
                }
                int w = floor(lua_tonumber(L, -2) + 0.5);
                int h = floor(lua_tonumber(L, -1) + 0.5);
                lua_pop(L, 2);                                /* -> udata, key, value */
                
                puglSetMinSize(udata->puglView, w, h);
            }
            else if (checkArgTableValueType(L, initArg, key, "maxSize", LUA_TTABLE)) 
            {                                                 /* -> udata, key, value */
                if (   lua_rawgeti(L, -1, 1) != LUA_TNUMBER   /* -> udata, key, value, w */
                    || lua_rawgeti(L, -2, 2) != LUA_TNUMBER)  /* -> udata, key, value, w, h */
                {
                    return luaL_argerror(L, initArg, "invalid 'maxSize' value");
                }
                int w = lua_tonumber(L, -2);
                int h = lua_tonumber(L, -1);
                lua_pop(L, 2);                                /* -> udata, key, value */
                
                if (w > 0 && h > 0) {
                    w = floor(w + 0.5);
                    h = floor(h + 0.5);
                }
                puglSetMaxSize(udata->puglView, w, h);
            }
            else if (checkArgTableValueType(L, initArg, key, "frame", LUA_TTABLE)) 
            {                                                 /* -> udata, key, value */
                if (   lua_rawgeti(L, -1, 1) != LUA_TNUMBER   /* -> udata, key, value, x */
                    || lua_rawgeti(L, -2, 2) != LUA_TNUMBER   /* -> udata, key, value, x, y */
                    || lua_rawgeti(L, -3, 3) != LUA_TNUMBER   /* -> udata, key, value, x, y, w */
                    || lua_rawgeti(L, -4, 4) != LUA_TNUMBER)  /* -> udata, key, value, x, y, w, h */
                {
                    return luaL_argerror(L, initArg, "invalid 'frame' value");
                }
                lua_Number x = lua_tonumber(L, -4);
                lua_Number y = lua_tonumber(L, -3);
                lua_Number w = lua_tonumber(L, -2);
                lua_Number h = lua_tonumber(L, -1);
                lua_pop(L, 4);                                /* -> udata, key, value */
                
                PuglRect rect;
                rect.x      = floor(x + 0.5);
                rect.y      = floor(y + 0.5);
                rect.width  = floor(x + w + 0.5) - rect.x;
                rect.height = floor(y + h + 0.5) - rect.y;
                puglSetFrame(udata->puglView, rect);
            }
            else if (checkArgTableValueType2(L, initArg, key, "eventFunc", LUA_TTABLE, LUA_TFUNCTION)) 
            {                                                 /* -> udata, key, value */
                if (lua_type(L, -1) == LUA_TFUNCTION) {
                    lua_getuservalue(L, -3);                  /* -> udata, key, value, uservalue */
                    lua_pushvalue(L, -2);                     /* -> udata, key, value, uservalue, func */
                    lua_rawseti(L, -2, 0);                    /* -> udata, key, value, uservalue */
                    udata->eventFuncNargs = 0;
                    lua_pop(L, 1);                            /* -> udata, key, value */
                }
                else {                                        /* -> udata, key, value */
                    lua_getuservalue(L, -3);                  /* -> udata, key, value, uservalue */
                    if (lua_rawgeti(L, -2, 1) != LUA_TFUNCTION) {
                        return luaL_argerror(L, initArg, "invalid 'eventFunc' value");
                    }                                         /* -> udata, key, value, uservalue, func */
                    lua_rawseti(L, -2, 0);                    /* -> udata, key, value, uservalue */
                    size_t nargs = lua_rawlen(L, -2) - 1;
                    for (size_t i = 1; i <= nargs; ++i) {     /* -> udata, key, value, uservalue */
                        lua_rawgeti(L, -2, 1 + i);            /* -> udata, key, value, uservalue, arg */
                        lua_rawseti(L, -2, i);                /* -> udata, key, value, uservalue */
                    }
                    udata->eventFuncNargs = nargs;
                    lua_pop(L, 1);                            /* -> udata, key, value */
                }
                hasEventFunc = true;
            }
            else if (checkArgTableValueUdata            (L, initArg, key, "transientFor", LPUGL_VIEW_CLASS_NAME)
                  || checkArgTableValueUdata            (L, initArg, key, "popupFor",     LPUGL_VIEW_CLASS_NAME)
                  || checkArgTableValueUdataOrLightUdata(L, initArg, key, "parent",       LPUGL_VIEW_CLASS_NAME))
            {
                bool isPopup     = (strcmp(key, "popupFor")     == 0);
                bool isTransient = (strcmp(key, "transientFor") == 0);
                bool isChild     = (strcmp(key, "parent")       == 0);
                
                if ((isPopup     && (hasTransient || hasParent)) 
                 || (isTransient && (hasPopup     || hasParent))
                 || (isChild     && (hasTransient || hasPopup))) 
                {
                    luaL_argerror(L, initArg, "only one parameter 'transientFor', 'popupFor' or 'parent' is allowed");
                }
                if (isPopup) {
                    udata->isPopup = true;
                }
                if (isChild) {
                    udata->isChild = true;
                }
                
                hasPopup     = hasPopup     || isPopup;
                hasTransient = hasTransient || isTransient;
                hasParent    = hasParent    || isChild;

                if (lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
                    PuglNativeView nativeWid = (uintptr_t)lua_touserdata(L, -1);
                    if (nativeWid) {
                        puglSetParentWindow(udata->puglView, nativeWid);
                    } else {
                        udata->isChild = false;
                    }
                }
                else {
                    ViewUserData* udata2 = lua_touserdata(L, -1);
                    if (!udata2->puglView) {
                        return lpugl_ERROR_ILLEGAL_STATE(L, lua_pushfstring(L, "%s: view closed", key));
                    }
                    if (isPopup || isTransient) {
                        puglSetTransientFor(udata->puglView, puglGetNativeWindow(udata2->puglView));
                    }
                    if (isChild) {
                        puglSetParentWindow(udata->puglView, puglGetNativeWindow(udata2->puglView));
                    }
                    lua_getuservalue(L, -1);                                        /* -> udata, key, value, uservalue */
                    if (lua_rawgeti(L, -1, LPUGL_VIEW_UV_CHILDVIEWS) != LUA_TTABLE) /* -> udata, key, value, uservalue, ? */
                    {                                                               /* -> udata, key, value, uservalue, nil */
                        lua_pop(L, 1);                                              /* -> udata, key, value, uservalue */
                        lua_newtable(L);                                            /* -> udata, key, value, uservalue, childviews */
                        lua_newtable(L);                                            /* -> udata, key, value, uservalue, childviews, meta */
                        lua_pushstring(L, "__mode");                                /* -> udata, key, value, uservalue, childviews, meta, key */
                        lua_pushstring(L, "v");                                     /* -> udata, key, value, uservalue, childviews, meta, key, value */
                        lua_rawset(L, -3);                                          /* -> udata, key, value, uservalue, childviews, meta */
                        lua_setmetatable(L, -2);                                    /* -> udata, key, value, uservalue, childviews */
                        lua_pushvalue(L, -1);                                       /* -> udata, key, value, uservalue, childviews, childviews */
                        lua_rawseti(L, -3, LPUGL_VIEW_UV_CHILDVIEWS);               /* -> udata, key, value, uservalue, childviews */
                    }                                                               /* -> udata, key, value, uservalue, childviews */
                    lua_pushvalue(L, -5);                                           /* -> udata, key, value, uservalue, childviews, childview */
                    lua_rawsetp(L, -2, udata);                                      /* -> udata, key, value, uservalue, childviews */
                    lua_pop(L, 2);                                                  /* -> udata, key, value */
                    if (isPopup) {
                        puglSetViewHint(udata->puglView, PUGL_IS_POPUP, lua_toboolean(L, -1));
                    }
                }
            }
            else if (checkArgTableValueType(L, initArg, key, "backend", LUA_TUSERDATA))
            {
                backend = lua_touserdata(L, -1);
                if (!lua_getmetatable(L, -1) || lua_getfield(L, -1, "lpugl.backend") != LUA_TBOOLEAN || !lua_toboolean(L, -1)
                 || !backend || backend->magic != LPUGL_BACKEND_MAGIC)
                {
                    return luaL_argerror(L, initArg, "invalid backend");
                }                                                               /* -> udata, key, value, meta, backendFlag */
                lua_pop(L, 2);                                                  /* -> udata, key, value */
                if (strcmp(backend->versionId, "lpugl.backend-" LPUGL_PLATFORM_STRING "-" LPUGL_VERSION_STRING) != 0) {
                    return luaL_argerror(L, initArg, "backend version mismatch");
                }
                if (!backend->world) {
                    return luaL_argerror(L, 2, "backend closed");
                }
                if (backend->world != udata->world) {
                    return luaL_argerror(L, 2, "backend belongs to other lpugl.world");
                }
                lua_getuservalue(L, -3);                                        /* -> udata, key, value, uservalue */
                lua_pushvalue(L, -2);                                           /* -> udata, key, value, uservalue, backend */
                lua_rawseti(L, -2, LPUGL_VIEW_UV_BACKEND);                      /* -> udata, key, value, uservalue */
                lua_pop(L, 1);                                                  /* -> udata, key, value*/
                udata->backend = backend;
            }
            else {
                return luaL_argerror(L, initArg, 
                                     lua_pushfstring(L, "unexpected table key '%s'", 
                                                     key));
            }
            lua_pop(L, 1);              /* -> udata, key */
        }                               /* -> udata */
    }
    if (!backend) {
        return luaL_argerror(L, initArg, "missing backend parameter and no default backend available");
    }
    if (!hasEventFunc) {
        return luaL_argerror(L, initArg, "missing 'eventFunc' parameter");
    }
    puglSetViewHint(udata->puglView, PUGL_RESIZABLE, isResizable);
    puglSetViewHint(udata->puglView, PUGL_DOUBLE_BUFFER, useDoubleBuffer);
    puglSetBackend(udata->puglView, backend->puglBackend);
    backend->used += 1;
    
    if (title) {
        puglSetWindowTitle(udata->puglView, title);
    }
    bool ok = puglRealize(udata->puglView) == PUGL_SUCCESS;
    if (title) free(title);
    if (!ok) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    return 1;
}

/* ============================================================================================ */

bool lpugl_view_close(lua_State* L, ViewUserData* udata, int udataIdx)
{
    if (udata->puglView) 
    {
        lua_getuservalue(L, udataIdx);                          /* -> uservalue */
        if (lua_rawgeti(L, -1, LPUGL_VIEW_UV_CHILDVIEWS) 
            == LUA_TTABLE)                                      /* -> uservalue, ? */
        {                                                       /* -> uservalue, childviews */
            lua_pushnil(L);                                     /* -> uservalue, childviews, nil */
            while (lua_next(L, -2)) {                           /* -> uservalue, childviews, key, value */
                ViewUserData* v = lua_touserdata(L, -1);
                if (v) closeView(L, v, lua_gettop(L));
                lua_pop(L, 1);                                  /* -> uservalue, childviews, key */
            }                                                   /* -> uservalue, childviews */
        }                                                       /* -> uservalue, ? */

        puglFreeView(udata->puglView);
        udata->puglView = NULL;

        lua_rawgeti(L, -2, LPUGL_VIEW_UV_BACKEND);              /* -> uservalue, ?, backend */
        LpuglBackend* backend = lua_touserdata(L, -1);
        if (backend) backend->used -= 1;       
        lua_pushnil(L);                                         /* -> uservalue, ?, backend, nil */
        lua_rawseti(L, -4, LPUGL_VIEW_UV_BACKEND);              /* -> uservalue, ?, backend */
        lua_pop(L, 3);                                          /* -> */

        lua_newtable(L);                                        /* -> empty table */
        lua_setuservalue(L, udataIdx);                          /* -> */
    }
    bool wasClosedNow = (udata->world != NULL);
    udata->world = NULL;
    return wasClosedNow;
}

/* ============================================================================================ */

static void closeView(lua_State* L, ViewUserData* udata, int udataIdx)
{
    LpuglWorld* world = udata->world;
    if (lpugl_view_close(L, udata, udataIdx)) {
        world->viewCount -= 1; 
    }
    if (world) {
        if (world->weakWorldRef != LUA_REFNIL) {
            if (lua_rawgeti(L, LUA_REGISTRYINDEX, world->weakWorldRef) 
                                                    == LUA_TTABLE) {    /* -> weakWorld */
                lua_pushnil(L);                                         /* -> weakWorld, nil */
                lua_rawsetp(L, -2, udata);                              /* -> weakWorld */
                
                if (lua_rawgeti(L, -1, 0) == LUA_TUSERDATA) {           /* -> weakWorld, world */
                    if (lua_getuservalue(L, -1) == LUA_TTABLE) {        /* -> weakWorld, world, worldUservalue */
                        lua_rawgeti(L, -1, LPUGL_WORLD_UV_VIEWS);       /* -> weakWorld, world, worldUservalue, viewLookup */
                        lua_pushnil(L);                                 /* -> weakWorld, world, worldUservalue, viewLookup, nil */
                        lua_rawsetp(L, -2, udata);                      /* -> weakWorld, world, worldUservalue, viewLookup */
                        lua_pop(L, 4);                                  /* -> */
                    } else {                                            /* -> ? */
                        lua_pop(L, 1);                                  /* -> */
                    }
                } else {                                                /* -> ? */
                    lua_pop(L, 1);                                      /* -> */
                }
            } else {                                                    /* -> ? */
                lua_pop(L, 1);                                          /* -> */
            }
        }
    }
}

/* ============================================================================================ */

static int View_close(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    closeView(L, udata, 1);
    return 0;
}

/* ============================================================================================ */

static int View_isClosed(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    lua_pushboolean(L, udata->puglView == NULL);
    return 1;
}

/* ============================================================================================ */

static int View_isVisible(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_pushboolean(L, puglGetVisible(udata->puglView));
    return 1;
}
/* ============================================================================================ */

static int View_hasFocus(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_pushboolean(L, puglHasFocus(udata->puglView));
    return 1;
}

/* ============================================================================================ */

static int View_grabFocus(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    if (puglGrabFocus(udata->puglView) != PUGL_SUCCESS) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    return 0;
}

/* ============================================================================================ */

static int View_show(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    if (puglShow(udata->puglView) != PUGL_SUCCESS) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    return 0;
}

/* ============================================================================================ */

static int View_hide(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    if (puglHide(udata->puglView) != PUGL_SUCCESS) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    return 0;
}

/* ============================================================================================ */

static int View_getDrawContext(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    
    if (!udata->drawing) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "only allowed within exposure event handling");
    }
    void* ctx = puglGetContext(udata->puglView);
    if (ctx && udata->backend->newDrawContext) {
        lua_getuservalue(L, 1);                                                 /* -> view, uservalue */
        if (lua_rawgeti(L, -1, LPUGL_VIEW_UV_DRAWCTX) == LUA_TNIL) {            /* -> view, uservalue, context */
            lua_pop(L, 1);                                                      /* -> view, uservalue */
            udata->backend->newDrawContext(L, ctx);                             /* -> view, uservalue, context */
        }
        return 1;
    } else {
        return 0;
    }
}

/* ============================================================================================ */

static int View_getLayoutContext(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }

    lua_getuservalue(L, 1);                                           /* -> uservalue */
    lua_rawgeti(L, -1, LPUGL_VIEW_UV_BACKEND);                        /* -> uservalue, backend */
    if (lua_getfield(L, -1, "getLayoutContext") == LUA_TFUNCTION) {   /* -> uservalue, backend, getLayoutContext */
        lua_pushvalue(L, -2);                                         /* -> uservalue, backend, getLayoutContext, backend */
        lua_call(L, 1, 1);                                            /* -> uservalue, backend, rslt */
        return 1;
    } else {
        return 0;
    }
}


/* ============================================================================================ */

static int View_setFrame(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    PuglRect rect;
    rect.x      = floor(luaL_checknumber(L, 2));
    rect.y      = floor(luaL_checknumber(L, 3));
    rect.width  = floor(luaL_checknumber(L, 4) + 0.5);
    rect.height = floor(luaL_checknumber(L, 5) + 0.5);
    
    puglSetFrame(udata->puglView, rect);
    
    return 0;
}

/* ============================================================================================ */

static int View_setSize(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    int w = floor(luaL_checknumber(L, 2) + 0.5);
    int h = floor(luaL_checknumber(L, 3) + 0.5);
    
    puglSetSize(udata->puglView, w, h);
    
    return 0;
}

/* ============================================================================================ */

static int View_getSize(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    PuglRect rect = puglGetFrame(udata->puglView);
    
    lua_pushinteger(L, rect.width);
    lua_pushinteger(L, rect.height);
    return 2;
}

/* ============================================================================================ */

static int View_setMinSize(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    int w = floor(luaL_checknumber(L, 2) + 0.5);
    int h = floor(luaL_checknumber(L, 3) + 0.5);
    
    puglSetMinSize(udata->puglView, w, h);
    
    return 0;
}

/* ============================================================================================ */

static int View_setMaxSize(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    int w = floor(luaL_checknumber(L, 2) + 0.5);
    int h = floor(luaL_checknumber(L, 3) + 0.5);
    
    puglSetMaxSize(udata->puglView, w, h);
    
    return 0;
}

/* ============================================================================================ */

static int View_getFrame(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    
    PuglRect rect = puglGetFrame(udata->puglView);
    
    lua_pushinteger(L, rect.x);
    lua_pushinteger(L, rect.y);
    lua_pushinteger(L, rect.width);
    lua_pushinteger(L, rect.height);
    return 4;
}

/* ============================================================================================ */

static int View_postRedisplay(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    PuglStatus rc = PUGL_SUCCESS;
    
    if (lua_gettop(L) > 1) {
        lua_Number arg_x = luaL_checknumber(L, 2);
        lua_Number arg_y = luaL_checknumber(L, 3);
        lua_Number arg_w = luaL_checknumber(L, 4);
        lua_Number arg_h = luaL_checknumber(L, 5);
        
        int x  = (int)floor(arg_x);
        int y  = (int)floor(arg_y);
        int w  = (int)ceil(arg_x + arg_w) - x;
        int h  = (int)ceil(arg_y + arg_h) - y;
       
        if (x < 0)  { w += x; x = 0; }
        if (y < 0)  { h += y; y = 0; }
    
        if (w > 0 && h > 0) {
            PuglRect rect;
            rect.x      = x;
            rect.y      = y;
            rect.width  = w;
            rect.height = h;
    
            rc = puglPostRedisplayRect(udata->puglView, rect);
        }
    } else {
        rc = puglPostRedisplay(udata->puglView);
    }
    if (rc != PUGL_SUCCESS) {
        return lpugl_ERROR_FAILED_OPERATION(L);
    }
    return 0;
}

/* ============================================================================================ */

static int View_requestClipboard(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    puglRequestClipboard(udata->puglView);
    
    return 0;
}

/* ============================================================================================ */

static int View_getBackend(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);

    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_getuservalue(L, 1);                         /* -> uservalue */
    lua_rawgeti(L, -1, LPUGL_VIEW_UV_BACKEND);      /* -> uservalue, backend */
    return 1;
}

/* ============================================================================================ */

static int View_setTitle(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    if (udata->isPopup) {
        return luaL_error(L, "Title attribute not allowed for popup views");
    }
    else if (udata->isChild) {
        return luaL_error(L, "Title attribute not allowed for child views");
    }
    const char* title = luaL_checkstring(L, 2);
    puglSetWindowTitle(udata->puglView, title);
    return 0;   
}

/* ============================================================================================ */

static const char* cursorOptions[] = 
{
    "ARROW",
    "CARET",
    "CROSSHAIR",
    "HAND",
    "NO",
    "LEFT_RIGHT",
    "UP_DOWN",
    "HIDDEN",
    NULL
};

static int View_setCursor(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    int opt = luaL_checkoption(L, 2, NULL, cursorOptions);
    puglSetCursor(udata->puglView, (PuglCursor)opt);

    return 0;   
}

/* ============================================================================================ */


static int View_toString(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    lua_pushfstring(L, "%s: %p", LPUGL_VIEW_CLASS_NAME, udata);
    return 1;
}

/* ============================================================================================ */

static int View_getScreenScale(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_pushnumber(L, puglGetScreenScale(udata->puglView));
    return 1;
}

/* ============================================================================================ */

static int View_getNativeHandle(lua_State* L)
{
    ViewUserData* udata = luaL_checkudata(L, 1, LPUGL_VIEW_CLASS_NAME);
    if (!udata->puglView) {
        return lpugl_ERROR_ILLEGAL_STATE(L, "closed");
    }
    lua_pushlightuserdata(L, (void*)puglGetNativeWindow(udata->puglView));
    return 1;
}

/* ============================================================================================ */

static const luaL_Reg ViewMethods[] = 
{
    { "show",               View_show         },
    { "hide",               View_hide         },
    { "close",              View_close        },
    { "isClosed",           View_isClosed     },
    { "isVisible",          View_isVisible    },
    { "hasFocus",           View_hasFocus     },
    { "grabFocus",          View_grabFocus    },
    { "getDrawContext",     View_getDrawContext   },
    { "getLayoutContext",   View_getLayoutContext },
    { "setFrame",           View_setFrame        },
    { "getFrame",           View_getFrame        },
    { "setSize",            View_setSize         },
    { "getSize",            View_getSize         },
    { "getScreenScale",     View_getScreenScale  },
    { "setMinSize",         View_setMinSize      },
    { "setMaxSize",         View_setMaxSize      },
    { "setTitle",           View_setTitle        },
    { "setCursor",          View_setCursor       },
    { "getBackend",         View_getBackend      },
    { "postRedisplay",      View_postRedisplay   },
    { "requestClipboard",   View_requestClipboard},
    { "getNativeHandle",    View_getNativeHandle },
    
    { NULL,           NULL } /* sentinel */
};

static const luaL_Reg ViewMetaMethods[] = 
{
    { "__tostring",   View_toString },
    { "__gc",         View_close  },
    { NULL,           NULL        } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { NULL,           NULL } /* sentinel */
};

static void setupViewMeta(lua_State* L)
{
    lua_pushstring(L, LPUGL_VIEW_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, ViewMetaMethods, 0);
    
    lua_newtable(L);  /* BackendClass */
        luaL_setfuncs(L, ViewMethods, 0);
    lua_setfield (L, -2, "__index");
}

int lpugl_view_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, LPUGL_VIEW_CLASS_NAME)) {
        setupViewMeta(L);
    }
    lua_pop(L, 1);

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}
