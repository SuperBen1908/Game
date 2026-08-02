@echo off
call misc\ctags.exe -R src\*
subst w: C:\Game
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=w:\misc;%path%
