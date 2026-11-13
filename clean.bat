@echo off
mkdir bin
pushd bin
del -f -q *.i *.map *.pdb *.ilk *.exp *.obj *.dll *.lib *.exe > NUL 2> NUL
popd
