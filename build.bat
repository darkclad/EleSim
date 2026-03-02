@echo off
setlocal enabledelayedexpansion
set PATH=C:\msys64\ucrt64\bin;%PATH%

:: Defaults
set BUILD_TYPE=0
set LINK_TAG=shared
set DO_CLEAN=0
set DO_RUN=0
set DO_INSTALLER=0

:: Parse arguments in a loop
:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="debug"   ( set BUILD_TYPE=Debug&   shift& goto :parse_args )
if /i "%~1"=="release" ( set BUILD_TYPE=Release& shift& goto :parse_args )
if /i "%~1"=="clean"   ( set DO_CLEAN=1&         shift& goto :parse_args )
if /i "%~1"=="static"  ( set LINK_TAG=static&    shift& goto :parse_args )
if /i "%~1"=="shared"  ( set LINK_TAG=shared&    shift& goto :parse_args )
if /i "%~1"=="run"     ( set DO_RUN=1&           shift& goto :parse_args )
if /i "%~1"=="install" ( set DO_INSTALLER=1&     shift& goto :parse_args )
echo Unknown argument: %~1
shift
goto :parse_args
:done_args

:: Run-only mode: no build type specified, just launch last Debug build
if "!BUILD_TYPE!"=="0" (
    if !DO_RUN!==1 (
        echo Starting EleSim...
        start "" "build-Debug\src\EleSim.exe"
    ) else (
        echo No build type specified. Usage: build.bat [debug^|release] [clean] [static^|shared] [run] [install]
    )
    goto :eof
)

set BUILD_DIR=build-!BUILD_TYPE!

:: Process: clean first if requested
if !DO_CLEAN!==1 (
    echo Cleaning !BUILD_DIR!...
    if exist !BUILD_DIR! rmdir /s /q !BUILD_DIR!
    echo Done.
)

:: Detect link type change — reconfigure if needed
if exist !BUILD_DIR!\.link_type (
    set /p PREV_LINK=<!BUILD_DIR!\.link_type
    if /i not "!PREV_LINK!"=="!LINK_TAG!" (
        echo Link type changed from !PREV_LINK! to !LINK_TAG!, reconfiguring...
        rmdir /s /q !BUILD_DIR!
    )
)

if /i "!LINK_TAG!"=="static" (
    set QT_PREFIX=C:/msys64/ucrt64/qt6-static
    set EXTRA_FLAGS=-DCMAKE_EXE_LINKER_FLAGS=-static
    echo Building !BUILD_TYPE! [STATIC]...
) else (
    set QT_PREFIX=C:/msys64/ucrt64
    set EXTRA_FLAGS=
    echo Building !BUILD_TYPE! [SHARED]...
)

if not exist !BUILD_DIR! (
    cmake -B !BUILD_DIR! -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=!BUILD_TYPE! -DCMAKE_PREFIX_PATH=!QT_PREFIX! !EXTRA_FLAGS!
    echo !LINK_TAG!>!BUILD_DIR!\.link_type
)

cmake --build !BUILD_DIR! -j
if errorlevel 1 goto :eof

:: For static builds, clean any leftover DLLs and report result
if /i "!LINK_TAG!"=="static" (
    set OUT_DIR=!BUILD_DIR!\src
    del /Q !OUT_DIR!\*.dll >nul 2>&1
    echo Fully static build. Output: !OUT_DIR!\EleSim.exe
)

:: Create installer if requested
if !DO_INSTALLER!==1 (
    echo Creating installer...
    set PKG_DATA=installer\packages\com.elesim.app\data
    if exist !PKG_DATA! rmdir /s /q !PKG_DATA!
    mkdir !PKG_DATA!

    :: Copy exe
    copy /Y !BUILD_DIR!\src\EleSim.exe !PKG_DATA!\ >nul

    :: For shared builds, also bundle DLLs
    if /i "!LINK_TAG!"=="shared" (
        for %%D in (!BUILD_DIR!\src\*.dll) do (
            copy /Y "%%D" !PKG_DATA!\ >nul
        )
    )

    :: Build installer
    set INSTALLER_NAME=EleSim-0.1.0-Setup
    binarycreator -c installer\config\config.xml -p installer\packages !INSTALLER_NAME!.exe
    if errorlevel 1 (
        echo Installer creation failed.
        goto :eof
    )
    echo Installer created: !INSTALLER_NAME!.exe
)

:: Run the app if requested
if !DO_RUN!==1 (
    echo Starting EleSim...
    start "" "!BUILD_DIR!\src\EleSim.exe"
)
