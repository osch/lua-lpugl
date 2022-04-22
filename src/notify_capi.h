#ifndef NOTIFY_CAPI_H
#define NOTIFY_CAPI_H

#define NOTIFY_CAPI_ID_STRING     "_capi_notify"
#define NOTIFY_CAPI_VERSION_MAJOR  0
#define NOTIFY_CAPI_VERSION_MINOR  0
#define NOTIFY_CAPI_VERSION_PATCH  1

typedef struct notify_notifier notify_notifier;
typedef struct notify_capi     notify_capi;

#ifndef NOTIFY_CAPI_IMPLEMENT_SET_CAPI
#  define NOTIFY_CAPI_IMPLEMENT_SET_CAPI 0
#endif

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
     *
     * eh:     error handling function, may be NULL
     * ehdata: additional data that is given to error handling function.
     
     * Returns 0 - on success otherwise an error code != 0. 
     *         1 - if notifier is closed. The caller is expected to release the notifier.
     *             Subsequent calls will always result this return code again.
     * All other error codes are implementation specific.
     */
    int (*notify)(notify_notifier* n, notifier_error_handler eh, void* ehdata);
};

#if NOTIFY_CAPI_IMPLEMENT_SET_CAPI
/**
 * Sets the Notify C API into the metatable at the given index.
 * 
 * index: index of the table that is be used as metatable for objects 
 *        that are associated to the given capi.
 */
static int notify_set_capi(lua_State* L, int index, const notify_capi* capi)
{
    lua_pushlstring(L, NOTIFY_CAPI_ID_STRING, strlen(NOTIFY_CAPI_ID_STRING));             /* -> key */
    void** udata = lua_newuserdata(L, sizeof(void*) + strlen(NOTIFY_CAPI_ID_STRING) + 1); /* -> key, value */
    *udata = (void*)capi;
    strcpy((char*)(udata + 1), NOTIFY_CAPI_ID_STRING);    /* -> key, value */
    lua_rawset(L, (index < 0) ? (index - 2) : index);     /* -> */
    return 0;
}
#endif /* NOTIFY_CAPI_IMPLEMENT_SET_CAPI */

#if NOTIFY_CAPI_IMPLEMENT_GET_CAPI
/**
 * Gives the associated Notify C API for the object at the given stack index.
 * Returns NULL, if the object at the given stack index does not have an 
 * associated Notify C API or only has a Notify C API with incompatible version
 * number. If errorReason is not NULL it receives the error reason in this case:
 * 1 for incompatible version nummber and 2 for no associated C API at all.
 */
static const notify_capi* notify_get_capi(lua_State* L, int index, int* errorReason)
{
    if (luaL_getmetafield(L, index, NOTIFY_CAPI_ID_STRING) != LUA_TNIL)      /* -> _capi */
    {
        void** udata = lua_touserdata(L, -1);                                /* -> _capi */

        if (   udata
            && (lua_rawlen(L, -1) >= sizeof(void*) + strlen(NOTIFY_CAPI_ID_STRING) + 1)
            && (memcmp((char*)(udata + 1), NOTIFY_CAPI_ID_STRING, 
                       strlen(NOTIFY_CAPI_ID_STRING) + 1) == 0))
        {
            const notify_capi* capi = *udata;                                /* -> _capi */
            while (capi) {
                if (   capi->version_major == NOTIFY_CAPI_VERSION_MAJOR
                    && capi->version_minor >= NOTIFY_CAPI_VERSION_MINOR)
                {                                                            /* -> _capi */
                    lua_pop(L, 1);                                           /* -> */
                    return capi;
                }
                capi = capi->next_capi;
            }
            if (errorReason) {
                *errorReason = 1;
            }
        } else {                                                             /* -> _capi */
            if (errorReason) {
                *errorReason = 2;
            }
        }
        lua_pop(L, 1);                                                       /* -> */
    } else {                                                                 /* -> */
        if (errorReason) {
            *errorReason = 2;
        }
    }
    return NULL;
}
#endif /* NOTIFY_CAPI_IMPLEMENT_GET_CAPI */

#endif /* NOTIFY_CAPI_H */
