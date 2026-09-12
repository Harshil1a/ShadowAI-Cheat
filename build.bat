@echo off
setlocal enabledelayedexpansion
echo ============================================
echo   ShadowAI - Pro Build ^& Package Script
echo ============================================
echo.

REM Adjust these paths to your Qt installation
SET QT_DIR=C:\Qt\6.11.0\mingw_64
SET CMAKE_PATH=C:\Qt\Tools\CMake_64\bin
SET MINGW_PATH=C:\Qt\Tools\mingw1310_64\bin

REM Add Qt, CMake, and MinGW to PATH
SET PATH=%QT_DIR%\bin;%CMAKE_PATH%;%MINGW_PATH%;%PATH%

echo [1/4] Preparing build directory...
if not exist build mkdir build
cd build

echo [2/4] Running CMake...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_DIR%
if errorlevel 1 (
    echo [ERROR] CMake failed!
    pause
    exit /b 1
)

echo [3/4] Compiling ShadowAI...
cmake --build . --config Release --parallel 8
if errorlevel 1 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo [4/4] Deploying Dependencies (windeployqt)...
cd bin
if exist "ShadowAI.exe" (
    %QT_DIR%\bin\windeployqt.exe --network --multimedia --no-translations --compiler-runtime ShadowAI.exe
) else (
    echo [ERROR] ShadowAI.exe not found in build\bin!
    pause
    exit /b 1
)

echo.
echo [CHECK] Looking for OpenSSL DLLs...
set SSL_FOUND=0
REM Try location 1: Qt Bin
if exist "%QT_DIR%\bin\libcrypto-*.dll" (
    copy "%QT_DIR%\bin\libcrypto-*.dll" . >nul
    copy "%QT_DIR%\bin\libssl-*.dll" . >nul
    set SSL_FOUND=1
)
REM Try location 2: MinGW Opt Bin
if %SSL_FOUND%==0 (
    if exist "C:\Qt\Tools\mingw1310_64\opt\bin\libcrypto-*.dll" (
        copy "C:\Qt\Tools\mingw1310_64\opt\bin\libcrypto-*.dll" . >nul
        copy "C:\Qt\Tools\mingw1310_64\opt\bin\libssl-*.dll" . >nul
        set SSL_FOUND=1
    )
)

if %SSL_FOUND%==1 (
    echo [SUCCESS] OpenSSL DLLs included.
) else (
    echo [WARNING] OpenSSL DLLs NOT found. 
    echo HTTPS requests to Gemini/Groq might fail.
    echo Please ensure libcrypto-3-x64.dll and libssl-3-x64.dll are in the folder.
)

echo.
echo =======================================================
echo   BUILD COMPLETE!
echo =======================================================
echo   Location: build\bin\
echo.
echo   To create the UNIVERSAL INSTALLER:
echo   1. Open "ShadowAI_Installer.iss" in Inno Setup.
echo   2. Press [Compile] (or F9).
echo   3. The installer will be in "installer_output\".
echo =======================================================
pause
