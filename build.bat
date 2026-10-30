@echo off
setlocal enabledelayedexpansion
REM tibia Windows build. Qt comes from tools\qt\install-qt.sh.
REM Other libraries are fetched by cmake\TibiaDeps.cmake.
cd /d "%~dp0"

echo ==^> Locating Visual Studio (C++ toolchain)...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Installer not found.
    echo        Install Visual Studio 2022 or Build Tools with "Desktop development with C++".
    goto :fail
)
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo ERROR: No Visual Studio install with the C++ toolset.
    goto :fail
)

set "VCVARS=%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found at "%VCVARS%".
    goto :fail
)
call "%VCVARS%" >nul
if errorlevel 1 goto :fail
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where ninja >nul 2>&1
if errorlevel 1 set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not found.
    goto :fail
)
where ninja >nul 2>&1
if errorlevel 1 (
    echo ERROR: ninja not found.
    goto :fail
)

echo ==^> Installing pinned Qt...
where bash >nul 2>&1
if errorlevel 1 (
    echo ERROR: bash is required to run tools\qt\install-qt.sh.
    goto :fail
)
bash tools/qt/install-qt.sh
if errorlevel 1 goto :fail
for /f "delims=" %%p in ('bash tools/qt/qt-prefix.sh') do set "QT_PREFIX=%%p"
if not defined QT_PREFIX (
    echo ERROR: Qt prefix not found after install.
    goto :fail
)

set "EXTRA="
where sccache >nul 2>&1
if errorlevel 1 set "EXTRA=-DOPTIONS_ENABLE_SCCACHE=OFF"

echo ==^> Configuring windows-release...
cmake --preset windows-release %EXTRA% -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DTOGGLE_BIN_FOLDER=ON -DTIBIA_BUILD_TESTS=OFF
if errorlevel 1 goto :fail

echo ==^> Building tibia...
cmake --build --preset windows-release --target tibia
if errorlevel 1 goto :fail

echo.
echo Build OK. Binary is under build\windows-release\bin\
echo.
pause
exit /b 0

:fail
echo.
echo Build FAILED - see the messages above.
pause
exit /b 1
