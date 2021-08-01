package = "nanovg"
version = "0.1.2-2"
source = {
   url = "https://github.com/starwing/lua-nanovg/releases/download/0.1.2/lua-nanovg-0.1.2.zip",
   tag = "0.1.2-0",
   dir = "lua-nanovg-0.1.2"
}
description = {
   summary = "Lua binding for NanoVG",
   homepage = "https://github.com/starwing/lua-nanovg",
   license = "MIT"
}
dependencies = {
   "lua >= 5.1, <= 5.4",
   "luarocks-build-extended"
}
build = {
   type = "extended",

   external_dependencies = {
     GL  = { header = "GL/gl.h"  }
   },

   modules = {
      nvg = {
         sources = {
            "lua-nanovg.c",
            "nanovg/src/nanovg.c"
         },
         incdirs = { "include", "$(GL_INCDIR)" },
         libdirs = { "$(GL_LIBDIR)" },
      }
   },
   platforms = {
      linux = {
         modules = {
            nvg = {
               libraries = {
                  "GL"
               }
            }
         }
      },
      windows = {
         modules = {
            nvg = {
               libraries = {
                  "opengl32"
               }
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
            nvg = {
               libraries = { },
               variables = {
                 LIBFLAG_EXTRAS = { "-framework", "OpenGL" }
               }
            }
         }
      }
   }
}
