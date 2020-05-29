package = "lpugl_opengl"
version = "scm-0"
source = {
  url = "https://github.com/osch/lua-lpugl/archive/master.zip",
  dir = "lua-lpugl-master",
}
description = {
  summary = "OpenGL backend for LPugl, a minimal API for building GUIs",
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
  "lua >= 5.1, < 5.4",
  "luarocks-build-extended",
  "lpugl",
}
build = {
  type = "extended",

  external_dependencies = {
    GL  = { header = "GL/gl.h"  },
  },
  platforms = {
    linux = {
      modules = {
        ["lpugl_opengl"] = {
          libraries = { "GL", "pthread" },
        }
      }
    },
    windows = {
      modules = {
        ["lpugl_opengl"] = {
          libraries = { "opengl32", "kernel32", "gdi32", "user32" },
        }
      }
    },
    macosx = {
      external_dependencies = {
        -- for Mac OS: OpenGL headers are under  "$(xcrun --sdk macosx --show-sdk-path)/System/Library/Frameworks/OpenGL.framework/Headers"
        -- (e.g. /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework/Headers)
        -- They must be included using "OpenGL/gl.h" and cannot be found by luarocks in the file system
        GL  = { header = false },
      },
      modules = {
        ["lpugl_opengl"] = {
          sources   = { "src/pugl_opengl.m" },
          libraries = { "pthread" },
          variables = {
            LIBFLAG_EXTRAS = { "-framework", "Cocoa",
                               "-framework", "OpenGL",
            }
          }
        }
      }
    },
  },
  modules = {
    ["lpugl_opengl"] = {
      sources = { "src/pugl_opengl.c",
                  "src/lpugl_opengl.c",
                  "src/lpugl_compat.c" },
      defines = { 
        "LPUGL_VERSION="..version:gsub("^(.*)-.-$", "%1"),
        "LPUGL_BUILD_DATE=$(BUILD_DATE)"
      },
      incdirs = { "pugl-repo", "$(GL_INCDIR)" },
      libdirs = { "$(GL_LIBDIR)" },
    },
  }
}
