@echo off

mkdir bin
pushd bin
del -f -q *.i *.map *.pdb *.ilk *.exp *.obj *.dll > NUL 2> NUL


set includes= /I "%CUDA_PATH%\include"

cl -P -Zi %includes% ..\src\win32.c
cl -P -Zi %includes% ..\src\game.c

set common_compiler_flags=     ^
  -GR- -EHa- -Oi -WX -W4       ^
  -FC -Zi                      ^
  -DGAME_WIN32 -DGAME_INTERNAL ^
  -wd4702                      ^
  -wd4101                      ^
  -wd4201                      ^
  -wd4100                      ^
  -wd4189 

set common_linker_flags=-opt:ref ^
  user32.lib                     ^
  Gdi32.lib                      ^
  Winmm.lib                      ^
  /LIBPATH:"%CUDA_PATH%\lib\x64" cuda.lib

set game_exports=                 ^
  -EXPORT:game_init               ^
  -EXPORT:game_get_sound_samples  ^
  -EXPORT:game_update_and_render  ^
  -EXPORT:game_set_platform

cl %includes% %common_compiler_flags% ^
  ..\src\game.c -Fmgame.map           ^
  -LD -link                           ^
  -DLL %game_exports%                 ^
  %common_linker_flags%               ^
  -PDB:game_%random%.pdb

cl %includes% %common_compiler_flags% ^
  ..\src\win32.c -Fmwin32.map         ^
  -link %common_linker_flags%

popd

