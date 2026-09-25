@echo off
setlocal EnableDelayedExpansion
set "SHADERC=E:\infinite-grid\lib\tools\shaderc.exe"
set "SHADER_DIR=E:\infinite-grid\lib\shaders"
set "INCLUDE_DIR=%SHADER_DIR%\headers"

call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\text_vertex.sc" vertex text_vertex
call :compile "E:\infinite-grid\lib\shaders\viewcubeFace\text_frag.sc" fragment text_frag
exit /b 0

:compile
set "INPUT=%~1"
set "TYPE=%~2"
set "NAME=%~3"
set "OUTPUT=%~dpn1.h"
set "TEMP=%~dpn1.temp.h"
if exist "%OUTPUT%" del /f /q "%OUTPUT%" >nul
> "%OUTPUT%" (
    echo // Auto-generated shader header. Do not edit.
    echo // Source: %~nx1
    echo #pragma once
    echo #include ^<cstdint^>
)
set PASS=0
set FAIL=0
call :run linux spirv "!NAME!_spv" || set /a FAIL+=1
call :run windows s_4_0 "!NAME!_dx11" || set /a FAIL+=1
call :run windows s_5_0 "!NAME!_dx12" || set /a FAIL+=1
call :run windows 120 "!NAME!_glsl" || set /a FAIL+=1
call :run osx metal "!NAME!_mtl" || set /a FAIL+=1
call :run android 320_es "!NAME!_essl" || set /a FAIL+=1
echo !NAME!: PASS=6 FAIL=!FAIL!
exit /b 0

:run
set "PLATFORM=%~1"
set "PROFILE=%~2"
set "BIN2C=%~3"
set "LOG=E:\infinite-grid\%~3.log"
"%SHADERC%" -f "%INPUT%" -o "%TEMP%" --type %TYPE% --platform %PLATFORM% --profile %PROFILE% -O 3 -i "%SHADER_DIR%" -i "%INCLUDE_DIR%" -i "%SHADER_DIR%\common" -i "E:\infinite-grid\lib\bgfx\src" --varyingdef "%SHADER_DIR%\viewcubeFace\varying.def.sc" --bin2c %BIN2C% > "%LOG%" 2>&1
if errorlevel 1 (
    echo FAIL %PLATFORM% %PROFILE% %BIN2C%
    type "%LOG%"
    exit /b 1
)
echo.>> "%OUTPUT%"
type "%TEMP%" >> "%OUTPUT%"
del /f /q "%TEMP%" >nul
del /f /q "%LOG%" >nul
exit /b 0
