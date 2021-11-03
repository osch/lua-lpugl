#ifndef NOTIFY_CAPI_H
#define NOTIFY_CAPI_H

#define NOTIFY_CAPI_VERSION_MAJOR 0
#define NOTIFY_CAPI_VERSION_MINOR 1
#define NOTIFY_CAPI_VERSION_PATCH 0

typedef struct notify_notifier notify_notifier;
typedef struct notify_capi     notify_capi;

#ifndef NOTIFY_CAPI_IMPLEMENT_GET_CAPI
#  define NOTIFY_CAPI_IMPLEMENT_GET_CAPI 0
#endif

/**
 * Type for pointer to function that may be called if an error occurs.
 * ehdata: void pointer that is given in notify method (see below)
 * msg:    detailed error message
 * msglen: length of error message
 */
typedef void (*notifier_error_handler)(void* ehdata, const char* msg, size_t msglen);

/**
 *  Notify C API.
 */
struct notify_capi
{
    int version_major;
    int version_minor;
    int version_patch;
    
    /**
     * May point to another (incompatible) version of this API implementation.
     * NULL if no such implementation exists.
     *
     * The usage of next_capi makes it possible to implement two or more
     * incompatible versions of the C API.
     *
     * An API is compatible to another API if both have the same major 
     * version number and if the minor version number of the first API is
     * greater or equal than the second one's.
     */
    void* next_capi;
    
    /**
     * Must return a valid pointer if the Lua object at the given stack
     * index is a valid notifier, otherwise must return NULL.
     *
     * The returned notifier object must be valid as long as the Lua 
     * object at the given stack index remains valid.
     * To keep the notifier object beyond this call, the function 
     * retainNotifier() should be called (see below).
     */
    notify_notifier* (*toNotifier)(lua_State* L, int index);
    
    /**
     * Increase the reference counter of the notifier object.
     * Must be thread safe.
     *
     * This function must be called after the function toNotifier()
     * as long as the Lua object on the given stack index is
     * valid (see above).
     */
    void (*retainNotifier)(notify_notifier* n);

    /**
     * Decrease the reference counter of the notifier object and
     * destructs the notifier object if no reference is left.
     * Must be thread safe.
     */
    void (*releaseNotifier)(notify_notifier* n);

    /**
     * Calls the notify method of the given notifier object.
     * Must be thread safe.
     * Returns 0 on success otherwise an error code != 0. 
     * Should return 1 if the notifier should not be called again.
     * The caller is expected to release the notifier object 
     * in this case.
     * All other error codes are implementation specific.
     *
     * eh:     error handling function, may be NULL
     * ehdata: additional data that is given to error handling function.
     */
    int (*notify)(notify_notifier* n, notifier_error_handler eh, void* ehdata);
};

#if NOTIFY_CAPI_IMPLEMENT_GET_CAPI
/**
 * Gives the associated Notify C API for the object at the given stack index.
 */
static const notify_capi* notify_get_capi(lua_State* L, int index, int* versionError)
{
    if (luaL_getmetafield(L, index, "_capi_notify") != LUA_TNIL) { /* -> _capi */
        const notify_capi* capi = lua_touserdata(L, -1);           /* -> _capi */
        while (capi) {
            if (   capi->version_major == NOTIFY_CAPI_VERSION_MAJOR
                && capi->version_minor >= NOTIFY_CAPI_VERSION_MINOR)
            {                                                      /* -> _capi */
                lua_pop(L, 1);                                     /* -> */
                return capi;
            }
            capi = capi->next_capi;
        }
        if (versionError) {
            *versionError = 1;
        }                                                           /* -> _capi */
        lua_pop(L, 1);                                              /* -> */
    }                                                               /* -> */
    return NULL;
}
#endif /* NOTIFY_CAPI_IMPLEMENT_GET_CAPI */

#endif /* NOTIFY_CAPI_H */
