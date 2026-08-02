@echo off

mkdir bin
pushd bin
del -f -q *.i *.map *.pdb *.ilk *.exp *.obj *.dll

cl -P -Zi ..\src\win32.c

set common_compiler_flags=-GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -FC -Zi -DGAME_WIN32 -DGAME_INTERNAL
set common_linker_flags=-opt:ref user32.lib Gdi32.lib Winmm.lib

set game_exports=-EXPORT:game_update_and_render -EXPORT:game_get_sound_samples

set pdb_file=game_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb

cl  %common_compiler_flags%  ..\src\game.c   -Fmgame.map  -LD -link -PDB:%pdb_file% -DLL  %game_exports%
cl  %common_compiler_flags%  ..\src\win32.c  -Fmwin32.map -link %common_linker_flags%

popd
