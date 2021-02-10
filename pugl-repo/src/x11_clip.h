static void
handleSelectionRequestForOwner(Display*                      display,
                               const PuglX11Atoms* const     atoms,
                               const XSelectionRequestEvent* request,
                               PuglBlob*                     clipboard,
                               PuglX11IncrTarget*            incrTarget)
{
    XSelectionEvent note = {0};
    note.type            = SelectionNotify;
    note.requestor       = request->requestor;
    note.selection       = request->selection;
    note.target          = request->target;
    note.time            = request->time;
    
    if (request->target == atoms->TARGETS)
    {
        Atom myTargets[] = { atoms->UTF8_STRING, XA_STRING };
        static const int XSERVER_ATOM_BITSIZE = 32; // sizeof(Atom) == 64 on 64bit systems, but this value must be 32
        note.property = request->property;
        XChangeProperty(display, note.requestor, note.property,
            XA_ATOM, XSERVER_ATOM_BITSIZE, 0, (unsigned char*)myTargets,
            sizeof(myTargets)/sizeof(myTargets[0]));
    }
    else
    {
        size_t      len  = clipboard->len;
        const void* data = clipboard->data;
        if (data && request->selection == atoms->CLIPBOARD 
                 && (   request->target == atoms->UTF8_STRING
                     || request->target == XA_STRING)) 
        {
            note.property = request->property;
            
            if (len < 200*1000) {
                XChangeProperty(display, note.requestor,
                                note.property, note.target, 8, PropModeReplace,
                                (const uint8_t*)data, len);
            } else {
                
                XChangeProperty(display, note.requestor,
                                note.property, atoms->INCR, 32, PropModeReplace,
                                0, 0);
                if (incrTarget->win) {
                    XSelectInput(display, incrTarget->win, 0);
                }
                incrTarget->win  = note.requestor;
                incrTarget->prop = note.property;
                incrTarget->type = note.target;
                incrTarget->pos = 0;
                XSelectInput(display, incrTarget->win, PropertyChangeMask);
            }
        } else {
            note.property = None;
        }
    }
    
    XSendEvent(display, note.requestor, True, 0, (XEvent*)&note);
}

static void
handlePropertyNotifyForOwner(Display*                      display,
                             PuglBlob*                     clipboard,
                             PuglX11IncrTarget*            incrTarget)
{
    if (incrTarget->pos < clipboard->len) {
        // send next part
        size_t len = clipboard->len - incrTarget->pos;
        if (len > 200*1000) len = 200*1000;
        XChangeProperty(display, incrTarget->win, incrTarget->prop, 
                        incrTarget->type, 8, PropModeReplace, 
                        (const uint8_t*)(clipboard->data + incrTarget->pos), len);
        incrTarget->pos += len;
    } else {
        // finished
        XChangeProperty(display, incrTarget->win, incrTarget->prop, 
                        incrTarget->type, 8, PropModeReplace, 
                        0, 0);
        XSelectInput(display, incrTarget->win, 0);
        incrTarget->win = 0;
    }
}

static void
handleSelectionNotifyForRequestor(PuglView* view, XSelectionEvent* event)
{
    const PuglX11Atoms* const atoms = &view->world->impl->atoms;
    PuglInternals* const impl = view->impl;
    
    puglSetBlob(&view->clipboard, NULL, 0);
    impl->incrClipboardRequest = false;
    
    if (event->selection == atoms->CLIPBOARD &&
        (event->target == atoms->UTF8_STRING || event->target == XA_STRING) &&
        event->property == XA_PRIMARY) 
    {
        uint8_t*      str  = NULL;
        Atom          type = 0;
        int           fmt  = 0;
        unsigned long len  = 0;
        unsigned long left = 0;
        XGetWindowProperty(impl->display, impl->win, XA_PRIMARY,
                           0, 0x1FFFFFFF, True, AnyPropertyType,
                           &type, &fmt, &len, &left, &str);
    
        if (str && fmt == 8 && (type == atoms->UTF8_STRING || type == XA_STRING) && left == 0) {
            puglSetBlob(&view->clipboard, str, len);
            impl->clipboardRequested = 0; // finished
        }
        else if (type == atoms->INCR) {
            // multi part
            impl->incrClipboardRequest = true;
        } else {
            impl->clipboardRequested = 0; // finished
        }
        XFree(str);
    
    }
    else {
        // sender does not support format
        if (impl->clipboardRequested == 1) {
            impl->clipboardRequested = 2; // second try
            XConvertSelection(impl->display,
                              atoms->CLIPBOARD,
                              XA_STRING,
                              XA_PRIMARY,
                              impl->win,
                              CurrentTime);
        } else {
            impl->clipboardRequested = 0;
        }
    }
    if (!impl->clipboardRequested) {
        // finished
        impl->clipboardRequested = 0;
        PuglEvent event;
        event.received.type  = PUGL_DATA_RECEIVED;
        event.received.flags = 0;
        event.received.data  = view->clipboard.data;
        event.received.len   = view->clipboard.len;
        view->clipboard.data = NULL;
        view->clipboard.len = 0;
        puglDispatchEvent(view, &event);
    }

}

static void
handlePropertyNotifyForRequestor(PuglView* view)
{
    const PuglX11Atoms* const atoms = &view->world->impl->atoms;
    PuglInternals* const impl = view->impl;

    uint8_t*      str  = NULL;
    Atom          type = 0;
    int           fmt  = 0;
    unsigned long len  = 0;
    unsigned long left = 0;
    XGetWindowProperty(impl->display, impl->win, XA_PRIMARY,
                       0, 0x1FFFFFFF, True, AnyPropertyType,
                       &type, &fmt, &len, &left, &str);
    
    if (str && fmt == 8 && (type == atoms->UTF8_STRING || type == XA_STRING) && left == 0) {
        if (len > 0) {
            void* newPtr = realloc(view->clipboard.data, view->clipboard.len + len);
            if (newPtr) {
                memcpy(newPtr + view->clipboard.len, str, len);
                view->clipboard.data = newPtr;
                view->clipboard.len += len;
            }
        } else {
            impl->incrClipboardRequest = false;
            impl->clipboardRequested = 0;
            PuglEvent event;
            event.received.type  = PUGL_DATA_RECEIVED;
            event.received.flags = 0;
            event.received.data  = view->clipboard.data;
            event.received.len   = view->clipboard.len;
            view->clipboard.data = NULL;
            view->clipboard.len = 0;
            puglDispatchEvent(view, &event);
        }
    }
    XFree(str);
}
