#ifndef PUGL_DETAIL_RECT_H
#define PUGL_DETAIL_RECT_H

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pugl/detail/implementation.h"
#include "pugl/pugl.h"

static void integerRect(PuglRect* rect) 
{
    double x     = floor(rect->x);
    double y     = floor(rect->y);
    rect->width  = ceil(rect->x + rect->width)  - x;
    rect->height = ceil(rect->y + rect->height) - y;
    rect->x      = x;
    rect->y      = y;
}

static inline bool doesRectContain(PuglRect* r1, PuglRect* r2)
{
    double myX0 = r1->x;             double myY0 = r1->y;
    double myX1 = r1->x + r1->width; double myY1 = r1->y + r1->height;
    
    double otherX0 = r2->x;             double otherY0 = r2->y;
    double otherX1 = r2->x + r2->width; double otherY1 = r2->y + r2->height;
    
    return myX0 <= otherX0  &&  otherX0 <  myX1  
        && myY0 <= otherY0  &&  otherY0 <  myY1
       
        && myX0 <  otherX1  &&  otherX1 <= myX1
        && myY0 <  otherY1  &&  otherY1 <= myY1;
}

static bool addRect(PuglRects* rects, PuglRect* rect) 
{
    bool added = false;
    for (int i = 0; i < rects->rectsCount; ++i) {
        if (doesRectContain(rects->rectsList + i, rect)) {
            added = true;
            break;
        }
    }
    if (!added) {
        for (int i = 0; i < rects->rectsCount;) {
            if (doesRectContain(rect, rects->rectsList + i)) {
                if (!added) {
                    rects->rectsList[i] = *rect;
                    added = true;
                    ++i;
                } else {
                    memmove(rects->rectsList + i, rects->rectsList + i + 1, 
                            (rects->rectsCount - (i + 1)) * sizeof(PuglRect));
                    rects->rectsCount -= 1;
                }
            } else {
                ++i;
            }
        }
    }
    if (!added) {
        if (!puglRectsAppend(rects, rect)) {
            return false;
        }
    }
    return true;
}

#endif // PUGL_DETAIL_RECT_H
