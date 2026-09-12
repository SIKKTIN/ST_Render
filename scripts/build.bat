@echo off
setlocal
set "ST_PRESET=%~1"
if "%ST_PRESET%"=="" set "ST_PRESET=debug"
call "%~dp0configure.bat" %ST_PRESET% || exit /b 1
pushd "%~dp0.." || exit /b 1
cmake --build --preset %ST_PRESET% --parallel 4
set "ST_RESULT=%ERRORLEVEL%"
popd
exit /b %ST_RESULT%
