@echo off

echo ========================================================
echo  Compiling and Running FFX Cutscene Skip Unit Tests
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

echo Compiling Unit Test Runner...
cl /nologo /O2 /MD /EHsc /std:c++17 /W3 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /DWIN32 /D_CONSOLE ^
   /I"src" /I"src\include" /I"src\minhook" /I"src\minhook\include" ^
   tests\test_runner.cpp ^
   src\config.cpp ^
   src\minhook\buffer.c ^
   src\minhook\hook.c ^
   src\minhook\trampoline.c ^
   src\minhook\hde\hde32.c ^
   /link /OUT:bin\test_runner.exe ^
   kernel32.lib user32.lib gdi32.lib

if errorlevel 1 (
    echo [ERROR] Unit test compilation failed!
    exit /b 1
)

del *.obj 2>nul

echo.
echo Running Unit Tests:
echo --------------------------------------------------------
bin\test_runner.exe
set "TEST_RESULT=%ERRORLEVEL%"
echo --------------------------------------------------------

if %TEST_RESULT% NEQ 0 (
    echo [FAILURE] Unit tests failed!
    exit /b %TEST_RESULT%
)

echo [SUCCESS] All unit tests completed successfully!
exit /b 0
