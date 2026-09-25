@echo off
setlocal EnableDelayedExpansion
set "SHADERC=E:\infinite-grid\lib\tools\shaderc.exe"
set "SHADER_DIR=E:\infinite-grid\lib\shaders"
set "INCLUDE_DIR=%SHADER_DIR%\headers"

call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\vertex.sc" vertex "E:\infinite-grid\lib\shaders\viewcubeFace\varying.def.sc" vertex
call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\frag.sc" fragment "E:\infinite-grid\lib\shaders\viewcubeFace\varying.def.sc" frag
call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\vs_line.sc" vertex "E:\infinite-grid\lib\shaders\viewcubeFace\line_varying.def.sc" vs_line
call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\fs_line.sc" fragment "E:\infinite-grid\lib\shaders\viewcubeFace\line_varying.def.sc" fs_line
exit /b 0

:compile
set "INPUT=%~1"
set "TYPE=%~2"
set "VARYING=%~3"
set "NAME=%~4"
set "OUTPUT=%~dpn1.h"
set "TEMP=%~dpn1.temp.h"
if exist "%OUTPUT%" del /f /q "%OUTPUT%"
> "%OUTPUT%" (
    echo // Auto-generated shader header. Do not edit.
    echo // Source: %~nx1
    echo #pragma once
    echo #include ^<cstdint^>
)
set PASS=0
set FAIL=0
call :run linux spirv || set /a FAIL+=1
call :run windows s_4_0 || set /a FAIL+=1
call :run windows s_5_0 || set /a FAIL+=1
call :run windows 120 || set /a FAIL+=1
call :run osx metal || set /a FAIL+=1
call :run android 320_es || set /a FAIL+=1
echo %NAME%: PASS=6 FAIL=!FAIL!
if !FAIL! neq 0 exit /b 1
exit /b 0

:run
set "PLATFORM=%~1"
set "PROFILE=%~2"
if /i "%PLATFORM%"=="linux" set "BIN2C=%NAME%_spv"
if /i "%PLATFORM%"=="windows" if /i "%PROFILE%"=="s_4_0" set "BIN2C=%NAME%_dx11"
if /i "%PLATFORM%"=="windows" if /i "%PROFILE%"=="s_5_0" set "BIN2C=%NAME%_dx12"
if /i "%PLATFORM%"=="windows" if /i "%PROFILE%"=="120" set "BIN2C=%NAME%_glsl"
if /i "%PLATFORM%"=="osx" set "BIN2C=%NAME%_mtl"
if /i "%PLATFORM%"=="android" set "BIN2C=%NAME%_essl"
set "LOG=E:\infinite-grid\%NAME%_%PLATFORM%.log"
"%SHADERC%" -f "%INPUT%" -o "%TEMP%" --type %TYPE% --platform %PLATFORM% --profile %PROFILE% -O 3 -i "%SHADER_DIR%" -i "%INCLUDE_DIR%" -i "%SHADER_DIR%\common" -i "E:\infinite-grid\lib\bgfx\src" --varyingdef "%VARYING%" --bin2c %BIN2C% > "%LOG%" 2>&1
if errorlevel 1 (
    echo FAIL %PLATFORM% %PROFILE%
    type "%LOG%"
    exit /b 1
)
echo.>> "%OUTPUT%"
type "%TEMP%" >> "%OUTPUT%"
del /f /q "%TEMP%"
del /f /q "%LOG%"
exit /b 0
