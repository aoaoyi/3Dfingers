# Bundled FreeGLUT 3.4.0 (Windows x64)

Upstream: https://github.com/freeglut/freeglut/tree/v3.4.0

The public headers, GNU MinGW x64 import library and DLL are supplied so this
project does not depend on a separately configured GLUT installation.
`LICENSE.txt` is the upstream license. Only FreeGLUT is redistributed here;
Windows OpenGL and GLU are operating-system libraries.

Built from the tagged upstream source with MinGW GCC 14.2.0 (Debian package
14.2.0-19+27+b1), CMake 3.31.6, Release, using these CMake settings:

```
-DCMAKE_SYSTEM_NAME=Windows
-DFREEGLUT_BUILD_DEMOS=OFF
-DFREEGLUT_BUILD_SHARED_LIBS=ON
-DFREEGLUT_BUILD_STATIC_LIBS=OFF
-DCMAKE_BUILD_TYPE=Release
```

`-static` in the application project embeds the MinGW C++/thread support runtime;
it still imports `libfreeglut.dll`, `OPENGL32.dll`, `GLU32.dll` and standard Windows
DLLs. Place `bin/x64/libfreeglut.dll` beside the executable. Code::Blocks and CMake
copy it after building. This bundled library targets **64-bit MinGW**, not MSVC or
32-bit MinGW. Rebuild FreeGLUT for other architectures.
