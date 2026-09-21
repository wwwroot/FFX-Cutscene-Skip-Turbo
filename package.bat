@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  Packaging FFX Cutscene Skip ^& Turbo for Nexus Mods
echo ========================================================

if not exist "bin\ff10-cutscene-skip.dll" (
    echo [ERROR] bin\ff10-cutscene-skip.dll not found. Run build.bat first!
    exit /b 1
)

if not exist "dist" mkdir "dist"
if exist "dist\temp_pkg" rmdir /s /q "dist\temp_pkg"
mkdir "dist\temp_pkg\modules\config"

:: Copy module files
copy "bin\ff10-cutscene-skip.dll" "dist\temp_pkg\modules\" >nul
copy "config\ff10-cutscene-skip.ini" "dist\temp_pkg\modules\config\" >nul
copy "README.md" "dist\temp_pkg\README.txt" >nul
copy "LICENSE" "dist\temp_pkg\LICENSE.txt" >nul

:: Create Standard Package using PowerShell Compress-Archive
echo Creating Standard Package: dist\FFX_Cutscene_Skip_v1.0.0.zip...
if exist "dist\FFX_Cutscene_Skip_v1.0.0.zip" del "dist\FFX_Cutscene_Skip_v1.0.0.zip"
powershell -Command "Compress-Archive -Path 'dist\temp_pkg\*' -DestinationPath 'dist\FFX_Cutscene_Skip_v1.0.0.zip'"

:: Create Standalone Package (includes dinput8.dll and hook.ini from game folder if available)
set "GAME_DIR=C:\Games\Steam\steamapps\common\FINAL FANTASY FFX&FFX-2 HD Remaster"
if exist "%GAME_DIR%\dinput8.dll" (
    echo Adding dinput8.dll loader for All-in-One Standalone package...
    copy "%GAME_DIR%\dinput8.dll" "dist\temp_pkg\" >nul
    copy "%GAME_DIR%\hook.ini" "dist\temp_pkg\" >nul
    if exist "%GAME_DIR%\simpleLog.dll" copy "%GAME_DIR%\simpleLog.dll" "dist\temp_pkg\" >nul

    echo Creating All-in-One Package: dist\FFX_Cutscene_Skip_AllInOne_v1.0.0.zip...
    if exist "dist\FFX_Cutscene_Skip_AllInOne_v1.0.0.zip" del "dist\FFX_Cutscene_Skip_AllInOne_v1.0.0.zip"
    powershell -Command "Compress-Archive -Path 'dist\temp_pkg\*' -DestinationPath 'dist\FFX_Cutscene_Skip_AllInOne_v1.0.0.zip'"
)

rmdir /s /q "dist\temp_pkg"

echo.
echo ========================================================
echo  Packaging Complete!
echo  Check the dist/ folder for your Nexus Mods upload zips.
echo ========================================================
exit /b 0
