@echo off
setlocal
set "ST_PRESET=%~1"
if "%ST_PRESET%"=="" set "ST_PRESET=debug"
if not "%ST_PRESET%"=="debug" if not "%ST_PRESET%"=="release" (
    echo Usage: configure.bat [debug^|release]
    exit /b 1
)
pushd "%~dp0.." || exit /b 1
cmake --preset %ST_PRESET%
set "ST_RESULT=%ERRORLEVEL%"
popd
exit /b %ST_RESULT%
