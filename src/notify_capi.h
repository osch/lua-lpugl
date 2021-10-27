#ifndef NOTIFY_CAPI_H
#define NOTIFY_CAPI_H

#define NOTIFY_CAPI_VERSION_MAJOR 0
#define NOTIFY_CAPI_VERSION_MINOR 1
#define NOTIFY_CAPI_VERSION_PATCH 0

typedef struct notify_notifier notify_notifier;
typedef struct notify_capi     notify_capi;

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
     * Must return a valid pointer if the object at the given stack
     * index is a valid notifier, otherwise must return NULL,
     */
    notify_notifier* (*toNotifier)(lua_State* L, int index);
    
    /**
     * Increase the reference counter of the notifier object.
     * Must be thread safe.
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
     */
    void (*notify)(notify_notifier* n);

};

/**
 * Gives the associated Notify C API for the object at the given stack index.
 */
static const notify_capi* notify_get_capi(lua_State* L, int index, int* versionError)
{
    if (luaL_getmetafield(L, index, "_capi_notify") != LUA_TNIL) {  /* -> _capi */
        const notify_capi* capi = lua_touserdata(L, -1);
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
        }
    }                                                              /* -> _capi */
    lua_pop(L, 1);                                                 /* -> */
    return NULL;
}

#endif /* NOTIFY_CAPI_H */
