.PHONY: default lpugl lpugl_cairo
default: lpugl lpugl_cairo lpugl_opengl

BUILD_DATE=$(shell date "+%Y-%m-%dT%H:%M:%S")

X11_GCC_RUN := gcc -shared -fPIC 
WIN_GCC_RUN := gcc -shared -fPIC 
MAC_GCC_RUN := MACOSX_DEPLOYMENT_TARGET=10.8 gcc -bundle -undefined dynamic_lookup -all_load

X11_PUGLC_EXT := c
WIN_PUGLC_EXT := c
MAC_PUGLC_EXT := m

X11_SO_EXT := so
WIN_SO_EXT := dll
MAC_SO_EXT := so


X11_COPTS :=
WIN_COPTS := -I/mingw64/include/lua5.1 -I/mingw64/include/cairo
MAC_COPTS := -I/usr/local/opt/lua/include/lua5.3 -I/usr/local/include/cairo

X11_LOPTS        := -lpthread -lX11
WIN_LOPTS        := -lkernel32 -lgdi32 -luser32 /mingw64/lib/liblua5.1.dll.a
MAC_LOPTS        := -lpthread -framework Cocoa

X11_LOPTS_CAIRO  := -lcairo
WIN_LOPTS_CAIRO  := -lcairo
MAC_LOPTS_CAIRO  := -lcairo

X11_LOPTS_OPENGL := -lGL 
WIN_LOPTS_OPENGL := -lopengl32
MAC_LOPTS_OPENGL := -framework OpenGL

GCC_RUN       :=
PUGLC_EXT     :=
SO_EXT        :=
COPTS         :=
LOPTS         :=
LOPTS_CAIRO   :=
LOPTS_OPENGL  :=

PLATFORM := X11

-include sandbox.mk

GCC_RUN       := $(or $(GCC_RUN),       $($(PLATFORM)_GCC_RUN))
PUGLC_EXT     := $(or $(PUGLC_EXT),     $($(PLATFORM)_PUGLC_EXT))
SO_EXT        := $(or $(SO_EXT),        $($(PLATFORM)_SO_EXT))
COPTS         := $(or $(COPTS),         $($(PLATFORM)_COPTS))
LOPTS         := $(or $(LOPTS),         $($(PLATFORM)_LOPTS))
LOPTS_CAIRO   := $(or $(LOPTS_CAIRO),   $($(PLATFORM)_LOPTS_CAIRO))
LOPTS_OPENGL  := $(or $(LOPTS_OPENGL),  $($(PLATFORM)_LOPTS_OPENGL))

lpugl:
	@mkdir -p build
	$(GCC_RUN) -g \
	    $(COPTS) \
	    -I../pugl-repo/ \
	    -o build/lpugl.$(SO_EXT) lpugl.c -D LPUGL_VERSION=Makefile-1 \
	    -DLPUGL_BUILD_DATE="$(BUILD_DATE)" \
	    -DPUGL_DISABLE_DEPRECATED \
	    async_util.c   util.c world.c view.c error.c pugl.$(PUGLC_EXT) \
	    lpugl_compat.c \
	    $(LOPTS)

lpugl_cairo:
	@mkdir -p build
	$(GCC_RUN) -g \
	    $(COPTS) \
	    -I /usr/include/cairo \
	    -I../pugl-repo/ \
	    -o build/lpugl_cairo.$(SO_EXT) lpugl_cairo.c -D LPUGL_VERSION=Makefile-1  \
	    -DLPUGL_BUILD_DATE="$(BUILD_DATE)" \
	    -DPUGL_DISABLE_DEPRECATED \
	    async_util.c   util.c pugl_cairo.$(PUGLC_EXT) \
	    lpugl_compat.c \
	    $(LOPTS) $(LOPTS_CAIRO) 
	    
lpugl_opengl:
	@mkdir -p build
	$(GCC_RUN) -g \
	    $(COPTS) \
	    -I../pugl-repo/ \
	    -o build/lpugl_opengl.$(SO_EXT) lpugl_opengl.c -D LPUGL_VERSION=Makefile-1  \
	    -DLPUGL_BUILD_DATE="$(BUILD_DATE)" \
	    -DPUGL_DISABLE_DEPRECATED \
	    async_util.c   util.c pugl_opengl.$(PUGLC_EXT) \
	    lpugl_compat.c \
	    $(LOPTS) $(LOPTS_OPENGL)
