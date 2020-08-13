#include "util.h"

lua_Number lpugl_current_time_seconds()
{
    lua_Number rslt;
#ifdef LPUGL_ASYNC_USE_WIN32
    struct _timeb timeval;
    _ftime(&timeval);
    rslt = ((lua_Number)timeval.time) + ((lua_Number)timeval.millitm) * 0.001;
#else
    struct timeval timeval;
    gettimeofday(&timeval, NULL);
    rslt = ((lua_Number)timeval.tv_sec) + ((lua_Number)timeval.tv_usec) * 0.000001;
#endif
    return rslt;
}


bool lpugl_membuf_init(MemBuffer* b, size_t initialCapacity, lua_Number growFactor)
{
    memset(b, 0, sizeof(MemBuffer));
    b->growFactor = growFactor;
    if (initialCapacity > 0) {
        char* data = malloc(initialCapacity);
        if (data != NULL) {
            b->bufferData     = data;
            b->bufferStart    = data;
            b->bufferCapacity = initialCapacity;
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

void lpugl_membuf_free(MemBuffer* b)
{
    if (b->bufferData) {
        free(b->bufferData);
        b->bufferData     = NULL;
        b->bufferStart    = NULL;
        b->bufferLength   = 0;
        b->bufferCapacity = 0;
    }
}

/**
 * 0 : ok
 * 1 : buffer should not grow
 * 2 : buffer can   not grow
 */
int lpugl_membuf_reserve(MemBuffer* b, size_t additionalLength)
{
    size_t newLength = b->bufferLength + additionalLength;
    
    if (b->bufferStart - b->bufferData + newLength > b->bufferCapacity) 
    {
        memmove(b->bufferData, b->bufferStart, b->bufferLength);
        b->bufferStart = b->bufferData;

        if (newLength > b->bufferCapacity) {
            if (b->bufferData == NULL) {
                size_t newCapacity = 2 * (newLength);
                b->bufferData = malloc(newCapacity);
                if (b->bufferData == NULL) {
                    return 2;
                }
                b->bufferStart    = b->bufferData;
                b->bufferCapacity = newCapacity;
            } else if (b->growFactor > 0) {
                size_t newCapacity = b->bufferCapacity * b->growFactor;
                if (newCapacity < newLength) {
                    newCapacity = newLength;
                }
                char* newData = realloc(b->bufferData, newCapacity);
                if (newData == NULL) {
                    return 2;
                }
                b->bufferData     = newData;
                b->bufferStart    = newData;
                b->bufferCapacity = newCapacity;
            } else {
                return 1;
            }
        }
    }
    return 0;
}

void lpugl_util_quote_lstring(lua_State* L, const char* s, size_t len)
{
    if (s) {
        luaL_Buffer tmp;
        luaL_buffinit(L, &tmp);
        luaL_addchar(&tmp, '"');
        size_t i;
        for (i = 0; i < len; ++i) {
            char c = s[i];
            if (c == 0) {
                luaL_addstring(&tmp, "\\0");
            } else if (c == '"') {
                luaL_addstring(&tmp, "\\\"");
            } else if (c == '\\') {
                luaL_addstring(&tmp, "\\\\");
            } else {
                luaL_addchar(&tmp, c);
            }
        }
        luaL_addchar(&tmp, '"');
        luaL_pushresult(&tmp);
    } else {
        lua_pushstring(L, "nil");
    }
}

void lpugl_util_quote_string(lua_State* L, const char* s)
{
    lpugl_util_quote_lstring(L, s, (s != NULL) ? strlen(s) : 0);
}

