@echo off

mkdir bin
pushd bin
del -f -q *.i *.map *.pdb *.ilk *.exp *.obj *.dll

cl -P -Zi ..\src\win32.c
cl -P -Zi ..\src\game.c

set common_compiler_flags=-GR- -EHa- -Oi -WX -W4 -wd4702 -wd4101 -wd4201 -wd4100 -wd4189 -FC -Zi -DGAME_WIN32 -DGAME_INTERNAL
set common_linker_flags=-opt:ref user32.lib Gdi32.lib Winmm.lib

set game_exports=-EXPORT:game_update_and_render -EXPORT:game_get_sound_samples -EXPORT:game_get_platform

set pdb_file=game_%random%.pdb

cl  %common_compiler_flags%  ..\src\game.c   -Fmgame.map  -LD -link -PDB:%pdb_file% -DLL  %game_exports%  %common_linker_flags%
cl  %common_compiler_flags%  ..\src\win32.c  -Fmwin32.map -link %common_linker_flags%

popd
