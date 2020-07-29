package = "lpugl_cairo"
version = "scm-0"
source = {
  url = "https://github.com/osch/lua-lpugl/archive/master.zip",
  dir = "lua-lpugl-master",
}
description = {
  summary = "Cairo backend for LPugl, a minimal API for building GUIs",
  homepage = "https://github.com/osch/lua-lpugl",
  license = "MIT/X11",
  detailed = [[
    LPugl is a minimal portable Lua-API for building GUIs.
    Supported platforms: X11, Windows and Mac OS X. 
    LPugl is based on Pugl, a minimal portable API for embeddable GUIs,
    see: https://drobilla.net/software/pugl
  ]],
}
dependencies = {
  "lua >= 5.1, <= 5.4",
  "luarocks-build-extended",
  "lpugl", "oocairo"
}
external_dependencies = {
  CAIRO = {
    header  = "cairo/cairo.h",
    library = "cairo",
  }
}
build = {
  type = "extended",
  platforms = {
    linux = {
      modules = {
        ["lpugl_cairo"] = {
          libraries = { "cairo", "pthread" },
        }
      }
    },
    windows = {
      modules = {
        ["lpugl_cairo"] = {
          libraries = { "cairo", "kernel32", "gdi32", "user32" },
        }
      }
    },
    macosx = {
      modules = {
        ["lpugl_cairo"] = {
          sources   = { "src/pugl_cairo.m" },
          libraries = { "cairo", "pthread"  },
          variables = {
            LIBFLAG_EXTRAS = { 
              "-framework", "Cocoa" 
            }
          }
        }
      }
    },
  },
  modules = {
    ["lpugl_cairo"] = {
      sources = { "src/pugl_cairo.c",
                  "src/lpugl_cairo.c",
                  "src/lpugl_compat.c" },
      defines = { 
        "LPUGL_VERSION="..version:gsub("^(.*)-.-$", "%1"),
        "LPUGL_BUILD_DATE=$(BUILD_DATE)"
      },
      incdirs = { "pugl-repo", "$(CAIRO_INCDIR)/cairo" },
      libdirs = { "$(CAIRO_LIBDIR)" },
    },
  }
}
