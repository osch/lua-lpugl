#include "error.h"

static const char* const LPUGL_ERROR_CLASS_NAME = "lpugl.error";

typedef struct Error {
    const char* name;
    int         details;
    int         traceback;
} Error;

static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    if (details != 0) {
        lua_pushfstring(L, "%s: %s", name, 
                                     lua_tostring(L, details));
    } else {
        lua_pushfstring(L, "%s",     name);
    }
}

/* error message details must be on top of stack */
static int throwErrorMessage(lua_State* L, const char* errorName)
{
    int messageDetails = lua_gettop(L);
    pushErrorMessage(L, errorName, messageDetails);
    lua_remove(L, messageDetails);
    return lua_error(L);
}

static int throwError(lua_State* L, const char* errorName)
{
    pushErrorMessage(L, errorName, 0);
    return lua_error(L);
}


int lpugl_ERROR_UNKNOWN_OBJECT_world_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, "world id %d", (int)id);
    return throwErrorMessage(L, LPUGL_ERROR_UNKNOWN_OBJECT);
}


int lpugl_ERROR_OUT_OF_MEMORY(lua_State* L)
{
    return throwError(L, LPUGL_ERROR_OUT_OF_MEMORY);
}

int lpugl_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, "failed to allocate %I bytes", (lua_Integer)bytes);
#else
    lua_pushfstring(L, "failed to allocate %f bytes", (lua_Number)bytes);
#endif
    return throwErrorMessage(L, LPUGL_ERROR_OUT_OF_MEMORY);
}

int lpugl_ERROR_RESTRICTED_ACCESS(lua_State* L)
{
    return throwError(L, LPUGL_ERROR_RESTRICTED_ACCESS);
}

int lpugl_ERROR_FAILED_OPERATION(lua_State* L)
{
    return throwError(L, LPUGL_ERROR_FAILED_OPERATION);
}

int lpugl_ERROR_FAILED_OPERATION_ex(lua_State* L, const char* message) 
{
    lua_pushstring(L, message);
    return throwErrorMessage(L, LPUGL_ERROR_FAILED_OPERATION);
}

int lpugl_ERROR_ILLEGAL_STATE(lua_State* L, const char* state)
{
    lua_pushstring(L, state);
    return throwErrorMessage(L, LPUGL_ERROR_ILLEGAL_STATE);
}

static void publishError(lua_State* L, int module, const char* errorName)
{
    lua_pushfstring(L, "lpugl.error.%s", errorName);
    lua_setfield(L, module, errorName);
}

int lpugl_error_init_module(lua_State* L, int errorModule)
{
    publishError(L, errorModule, LPUGL_ERROR_ERROR_IN_EVENT_HANDLING);
    publishError(L, errorModule, LPUGL_ERROR_ERROR_IN_ERROR_HANDLING);
    publishError(L, errorModule, LPUGL_ERROR_OUT_OF_MEMORY);

    publishError(L, errorModule, LPUGL_ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, LPUGL_ERROR_RESTRICTED_ACCESS);
    publishError(L, errorModule, LPUGL_ERROR_FAILED_OPERATION);
    publishError(L, errorModule, LPUGL_ERROR_ILLEGAL_STATE);
    
    return 0;
}

