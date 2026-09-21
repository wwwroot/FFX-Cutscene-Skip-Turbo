@echo off

echo ========================================================
echo  Building FFX Cutscene Skip ^& Turbo Mod (x86 Release)
echo ========================================================

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    echo [ERROR] vcvarsall.bat not found.
    exit /b 1
)

call "%VCVARS%" x86
if errorlevel 1 (
    echo [ERROR] Failed to initialize x86 build environment.
    exit /b 1
)

if not exist "bin" mkdir "bin"

echo Compiling FFX Cutscene Skip ^& Turbo...
cl /nologo /O2 /MD /EHsc /std:c++17 /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /DWIN32 /D_WINDOWS /D_USRDLL ^
   /I"src" /I"src\include" /I"src\minhook" /I"src\minhook\include" ^
   src\dllmain.cpp ^
   src\config.cpp ^
   src\speedhack.cpp ^
   src\input.cpp ^
   src\osd.cpp ^
   src\minhook\buffer.c ^
   src\minhook\hook.c ^
   src\minhook\trampoline.c ^
   src\minhook\hde\hde32.c ^
   /link /DLL /DEF:module.def /OUT:bin\ff10-cutscene-skip.dll ^
   kernel32.lib user32.lib gdi32.lib

if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

del *.obj 2>nul

echo.
echo ========================================================
echo  Build SUCCESS! Output: bin\ff10-cutscene-skip.dll
echo ========================================================
exit /b 0
