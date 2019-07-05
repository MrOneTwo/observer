@echo off


setlocal

set platform=x64

if "%1"=="x86" set platform=x86

set warnFlags=/FC
set includeFlags=/I..\src\SDL2-2.0.8\include /I..\src\glew-2.1.0\include /I..\src\dirent_win
set libsFlags=..\src\SDL2-2.0.8\VisualC\x64\Release\SDL2.lib ..\src\glew-2.1.0\lib\Release\x64\glew32.lib opengl32.lib

if not exist build mkdir build
pushd build
rem All cl options https://docs.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-by-category?view=vs-2019
rem /Zi option produces a separate PDB file that contains all the symbolic debugging information for use with the debugger.
rem /MTd whether a lib is static or dll and if production or debug.
if "%1"=="debug" (
  rem Fill me in.
) else (
  cl ..\src\main.cpp /Zi /MT %warnFlags% %includeFlags% /link %libsFlags%
)
popd