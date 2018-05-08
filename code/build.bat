@echo off

set CodeDir=..\code
set OutputDir=..\build_win32

set CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4127 -wd4201 -wd4100 -wd4189 -wd4505 -Z7 -FC
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib Winmm.lib opengl32.lib DbgHelp.lib

IF NOT EXIST %OutputDir% mkdir %OutputDir%

pushd %OutputDir%

del *.pdb > NUL 2> NUL

REM Asset File Builder
cl %CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS %CodeDir%\coco_asset_builder.cpp /link %CommonLinkerFlags%

REM 64-bit build
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% %CodeDir%\coco.cpp -Fmcoco.map -LD /link %CommonLinkerFlags% -incremental:no -opt:ref -PDB:coco_%random%.pdb -EXPORT:GameInit -EXPORT:GameUpdateAndRender
del lock.tmp
cl %CommonCompilerFlags% %CodeDir%\win32_coco.cpp -Fmwin32_coco.map /link %CommonLinkerFlags%

popd
