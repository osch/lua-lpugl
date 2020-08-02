#ifndef LPUGL_ERROR_H
#define LPUGL_ERROR_H

#include "util.h"

#define LPUGL_ERROR_ERROR_IN_EVENT_HANDLING  "error_in_event_handling"
#define LPUGL_ERROR_ERROR_IN_ERROR_HANDLING  "error_in_error_handling"
#define LPUGL_ERROR_ERROR_IN_LOG_FUNC        "error_in_log_func"
#define LPUGL_ERROR_OUT_OF_MEMORY            "out_of_memory"

#define LPUGL_ERROR_UNKNOWN_OBJECT           "unknown_object"
#define LPUGL_ERROR_RESTRICTED_ACCESS        "restricted_access"
#define LPUGL_ERROR_FAILED_OPERATION         "failed_operation"
#define LPUGL_ERROR_ILLEGAL_STATE            "illegal_state"

#define lpugl_error(L, errorLiteral) luaL_error((L), "lpugl.error." errorLiteral)

int lpugl_ERROR_UNKNOWN_OBJECT_world_id(lua_State* L, lua_Integer id);

int lpugl_ERROR_OUT_OF_MEMORY(lua_State* L);
int lpugl_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes);

int lpugl_ERROR_RESTRICTED_ACCESS(lua_State* L);

int lpugl_ERROR_FAILED_OPERATION(lua_State* L);

int lpugl_ERROR_FAILED_OPERATION_ex(lua_State* L, const char* message);

int lpugl_ERROR_ILLEGAL_STATE(lua_State* L, const char* state);

int lpugl_error_init_module(lua_State* L, int errorModule);


#endif /* LPUGL_ERROR_H */
