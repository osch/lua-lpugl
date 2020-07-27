#ifndef PUGL_DETAIL_RECT_H
#define PUGL_DETAIL_RECT_H

#include <math.h>

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

#endif // PUGL_DETAIL_RECT_H
