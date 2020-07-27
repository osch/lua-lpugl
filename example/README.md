# lpugl Examples
<!-- ---------------------------------------------------------------------------------------- -->

   * [`example01.lua`](./example01.lua)
     
     Simple example using the Cairo rendering backend. The Cairo backend 
     requires [OOCairo].

     ![Screenshot example01](./screenshot01.png)
     

<!-- ---------------------------------------------------------------------------------------- -->

   * [`example02.lua`](./example02.lua)

     Simple example using the OpenGL rendering backend. This example
     uses [LuaGL] Lua binding (see also [enhanced LuaGL rockspec]).

     ![Screenshot example02](./screenshot02.png)


<!-- ---------------------------------------------------------------------------------------- -->

   * [`example03.lua`](./example03.lua)

     A mini game demonstrating smooth animations using pre-rendered cairo
     surfaces.

     ![Screenshot example03](./screenshot03.png)


<!-- ---------------------------------------------------------------------------------------- -->

   * [`example04.lua`](./example04.lua)

     Same mini game as in `example03.lua` but here the OpenGL backend is used.
     This example uses [lua-nanovg] as vector graphics rendering library for OpenGL.

     ![Screenshot example04](./screenshot04.png)


<!-- ---------------------------------------------------------------------------------------- -->

   * [`example05.lua`](./example05.lua)

     An interactive demonstration for partial redrawing using the cairo rendering backend.


     ![Screenshot example05](./screenshot05.png)


<!-- ---------------------------------------------------------------------------------------- -->

[OOCairo]:                  https://luarocks.org/modules/osch/oocairo
[LuaGL]:                    https://luarocks.org/modules/blueowl04/opengl
[enhanced LuaGL rockspec]:  https://github.com/osch/luarocks-build-extended/blob/master/example/opengl-1.11-2.rockspec
[lua-nanovg]:               https://luarocks.org/modules/xavier-wang/nanovg

<!-- ---------------------------------------------------------------------------------------- -->
