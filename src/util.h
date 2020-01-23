#ifndef LPUGL_UTIL_H
#define LPUGL_UTIL_H

#include "base.h"

lua_Number lpugl_current_time_seconds();

typedef struct MemBuffer {
    lua_Number         growFactor;
    char*              bufferData;
    char*              bufferStart;
    size_t             bufferLength;
    size_t             bufferCapacity;
} MemBuffer;

bool lpugl_membuf_init(MemBuffer* b, size_t initialCapacity, lua_Number growFactor);

void lpugl_membuf_free(MemBuffer* b);

/**
 * 0 : ok
 * 1 : buffer should not grow
 * 2 : buffer can   not grow
 */
int lpugl_membuf_reserve(MemBuffer* b, size_t additionalLength);


void lpugl_util_quote_lstring(lua_State* L, const char* s, size_t len);

void lpugl_util_quote_string(lua_State* L, const char* s);

#endif /* LPUGL_UTIL_H */
