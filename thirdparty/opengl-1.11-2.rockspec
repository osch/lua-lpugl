--
-- Modified rockspec for building LuaGL for Linux, Windows and Mac OS,
-- see the original: https://luarocks.org/modules/blueowl04/opengl/1.11-1
--
package = "OpenGL"
version = "1.11-2"
source = {
  url = "https://downloads.sourceforge.net/project/luagl/1.11/Docs%20and%20Sources/luagl-1.11_Sources.tar.gz",
  dir="luagl",
  md5 = "65c849dbd60007689d427cd019f95d17"
}
description = {
  summary = "Lua bindings for OpenGL",
  detailed = [[
LuaGL is a library that provides access to the OpenGL 1.x
functionality from Lua 5.x. OpenGL is a portable software
interface to graphics hardware.
]],
  homepage = "http://luagl.sourceforge.net/",
  license = "MIT/X11"
}
dependencies = {
  "lua >= 5.1",
  "luarocks-build-extended"
}
build = {
  type = "extended",
  
  external_dependencies = {
    GL  = { header = "GL/gl.h"  },
    GLU = { header = "GL/glu.h" }
  },
  platforms = {
    linux = {
      modules = {
        luagl = {
          libraries = { "GL" }
        },
        luaglu = {
          libraries = { "GLU" }
        }
      }
    },
    windows = {
      modules = {
        luagl = {
          libraries = { "opengl32" }
        },
        luaglu = {
          libraries = { "glu32" }
        }
      }
    },
    macosx = {
      external_dependencies = {
        -- for Mac OS: OpenGL headers are under  "$(xcrun --sdk macosx --show-sdk-path)/System/Library/Frameworks/OpenGL.framework/Headers"
        -- (e.g. /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework/Headers)
        -- They must be included using "OpenGL/gl.h" and cannot be found by luarocks in the file system
        GL  = { header = false },
        GLU = { header = false }
      },
      modules = {
        luagl = {
          defines = { "MacOS" },
          libraries = { },
          variables = {
            LIBFLAG_EXTRAS = { "-framework", "OpenGL" }
          }
        },
        luaglu = {
          defines = { "MacOS" },
          libraries = { },
          variables = {
            LIBFLAG_EXTRAS = { "-framework", "OpenGL" }
          }
        }
      }
    }
  },
  modules = {
    luagl = {
      sources = { "src/luagl.c", "src/luagl_util.c", "src/luagl_const.c" },
      incdirs = { "include", "$(GL_INCDIR)" },
      libdirs = { "$(GL_LIBDIR)" },
    },
    luaglu = {
      sources = { "src/luaglu.c", "src/luagl_util.c", "src/luagl_const.c" },
      incdirs = { "include", "$(GLU_INCDIR)", "$(GL_INCDIR)" },
      libdirs = { "$(GLU_LIBDIR)" },
    }
  }
}
