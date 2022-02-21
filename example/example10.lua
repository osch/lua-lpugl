--[[
     This examples demonstrates the usage of the [Notify C API] in a multithreading scenario.
     
     LPugl world objects implement the *Notify C API*, see [src/notify_capi.h](../src/notify_capi.h),
     i.e. the world object has an an associated meta table entry *_capi_notify* delivered by
     the C API function *notify_get_capi()* and the associated C API function *toNotifier()* returns
     a valid pointer for a given LPugl world object.
     
     In this example the *Notify C API* is used to notify the event loop by calling *world:awake()*
     each time a message is added by another thread to a [mtmsg](https://github.com/osch/lua-mtmsg)
     buffer object. 
     
     This is done by connecting the LPugl world object as a notifier object to the mtmsg buffer 
     object.

     [Notify C API]:   https://github.com/lua-capis/lua-notify-capi
--]]

local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local lpugl     = require("lpugl_cairo")

local world = lpugl.newWorld("example10.lua")
local scale = world:getScreenScale()

local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()

threadOut:notifier(world)
threadOut:nonblock(true)
      
local thread = llthreads.new(function(inId, outId)
                                    local mtmsg      = require("mtmsg")
                                    local threadIn   = mtmsg.buffer(inId)
                                    local threadOut  = mtmsg.buffer(outId)
                                    local started    = mtmsg.time()
                                    local lastReport = mtmsg.time()
                                    local paused     = false
                                    threadOut:addmsg("working", 0)
                                    threadIn:nonblock(true)
                                    while true do
                                        local msg = threadIn:nextmsg()
                                        if msg == "start" then
                                            if not started or paused then
                                                if paused then
                                                    started = mtmsg.time() - paused
                                                else
                                                    started = mtmsg.time()
                                                end
                                                paused = false
                                                threadOut:addmsg("working", mtmsg.time() - started)
                                                lastReport = mtmsg.time()
                                                threadIn:nonblock(true)
                                            end
                                        elseif msg == "pause" then
                                            if started and not paused then
                                                paused = mtmsg.time() - started
                                                threadOut:addmsg("paused", paused)
                                                threadIn:nonblock(false)
                                            end
                                        elseif msg == "terminate" then
                                            threadOut:addmsg("terminated")
                                            return
                                        end
                                        if started and not paused then
                                            -- do some "work" here ...
                                            if mtmsg.time() - lastReport >= 1 then
                                                threadOut:addmsg("working", mtmsg.time() - started)
                                                lastReport = mtmsg.time()
                                            end
                                            if mtmsg.time() - started > 10 then
                                                -- work has finished after 10 seconds
                                                started = false
                                                threadOut:addmsg("finished")
                                                threadIn:nonblock(false)
                                            end
                                        end
                                    end
                                end,
                                threadIn:id(), threadOut:id())
thread:start()

local threadState = "initial"
local threadProgress = nil

local view = world:newView 
{
    title     = "example10",
    size      = {300*scale, 100*scale},
    resizable = true,
    
    eventFunc = function(view, event, ...)
        print("Event", event, ...)
        if event == "EXPOSE" then
            local cairo = view:getDrawContext()
            local w, h  = view:getSize()
            if threadState == "working" then
                cairo:set_source_rgb(1.0, 0.9, 0.9)
            else
                cairo:set_source_rgb(0.9, 0.9, 0.9)
            end
            cairo:rectangle(0, 0, w, h)
            cairo:fill()
            cairo:set_source_rgb(0, 0, 0)
            cairo:select_font_face("sans-serif", "normal", "normal")
            cairo:set_font_size(16*scale)
            local text = "Thread is "..threadState
            if threadProgress then
                text = string.format("%s (%d%%)", text, math.floor(threadProgress/10*100))
            end
            local ext = cairo:font_extents()
            local fonth = ext.ascent + ext.descent
            cairo:move_to((w - 180*scale)/2, (h - fonth)/2 + ext.ascent)
            cairo:show_text(text)
        elseif event == "BUTTON_PRESS" then
            local mx, my, button = ...
            if button == 1 then
                if threadState == "paused" or threadState == "finished" then
                    threadIn:addmsg("start")
                else
                    threadIn:addmsg("pause")
                end
            end
        elseif event == "CLOSE" then
            threadIn:addmsg("terminate")
            view:close()
        end
    end
}
view:show()

local counter = 0
while world:hasViews() do
    world:update()
    local msg, progress = threadOut:nextmsg()
    if msg then
        print("Received from thread", msg, progress)
        threadState = msg
        if msg == "paused" or msg == "working" then
            threadProgress = progress
        else
            threadProgress = nil 
        end
        if not view:isClosed() then
            view:postRedisplay()
        end
    end
    print("----- Updated", counter)
    counter = counter + 1
end

