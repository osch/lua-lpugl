package = "lpugl"
version = "scm-0"
source = {
  url = "https://github.com/osch/lua-lpugl/archive/master.zip",
  dir = "lua-lpugl-master",
}
description = {
  summary = "Minimal API for building GUIs",
  homepage = "https://github.com/osch/lua-lpugl",
  license = "MIT/X11",
  detailed = [[
    LPugl is a minimal portable Lua-API for building GUIs. 
    Currently there are two drawing backends available, 
    see packages "lpugl_cairo" and "lpugl_opengl".
    Supported platforms: X11, Windows and Mac OS X. 
    LPugl is based on Pugl, a minimal portable API for embeddable GUIs,
    see: https://drobilla.net/software/pugl
  ]],
}
dependencies = {
  "lua >= 5.1, <= 5.4",
  "luarocks-build-extended"
}
build = {
  type = "extended",
  platforms = {
    linux = {
      modules = {
        lpugl = {
          libraries = { "pthread", "X11" },
        }
      }
    },
    windows = {
      modules = {
        lpugl = {
          libraries = { "kernel32", "gdi32", "user32" },
        }
      }
    },
    macosx = {
      modules = {
        lpugl = {
          sources   = { "src/pugl.m" },
          libraries = { "pthread" },
          variables = {
            LIBFLAG_EXTRAS = {  "-framework", "Cocoa" }
          }
        }
      }
    },
  },
  modules = {
    lpugl = {
      sources = { "src/pugl.c",
                  "src/async_util.c",
                  "src/compat-5.3.c",
                  "src/error.c",
                  "src/lpugl.c",
                  "src/lpugl_compat.c",
                  "src/util.c",
                  "src/view.c",
                  "src/world.c" },
      defines = { 
        "LPUGL_VERSION="..version:gsub("^(.*)-.-$", "%1"),
        "LPUGL_BUILD_DATE=$(BUILD_DATE)"
      },
      incdirs = { "pugl-repo" },
      libdirs = { },
    },
  }
}
