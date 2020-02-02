-- For Windows: load oocairo DLL from Lua EXE and not from
-- lpugl.cairo DLL. Otherwise strange problems occur since
-- Windows unloads oocairo DLL before lpugl.cairo DLL.
require("oocairo") -- load oocairo DLL
return require("lpugl.cairo_intern") -- return lpugl.cairo DLL
