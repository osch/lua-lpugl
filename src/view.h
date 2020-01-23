#ifndef LPUGL_VIEW_H
#define LPUGL_VIEW_H

#include "base.h"

#include "pugl/pugl.h"

struct LpuglWorld;
struct ViewUserData;

/* ============================================================================================ */

//  uservalue indices
#define LPUGL_VIEW_UV_BACKEND     -3
#define LPUGL_VIEW_UV_CHILDVIEWS  -2
#define LPUGL_VIEW_UV_DRAWCTX     -1
#define LPUGL_VIEW_UV_EVENTFUNC    0

/* ============================================================================================ */

int lpugl_view_init_module(lua_State* L, int module);

int lpugl_view_new(lua_State* L, struct LpuglWorld* world, int initArg, int viewLookup);

bool lpugl_view_close(lua_State* L, struct ViewUserData* udata, int udataIdx);

PuglStatus lpugl_view_handle_event(PuglView* view, const PuglEvent* event);

#endif /* LPUGL_VIEW_H */
