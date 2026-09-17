@echo off
REM =====================================================
REM bgfx Shader Batch Compilation Script
REM Generates *.h header files with embedded binary arrays
REM for all platforms (spirv, dx9, dx11, metal, essl).
REM
REM Usage:
REM   compile_shaders_all.bat                  -- compile all shaders
REM   compile_shaders_all.bat -ShaderDir <dir> -- specify shader source dir
REM   compile_shaders_all.bat -Debug           -- compile with debug info
REM   compile_shaders_all.bat -Filter <name>   -- only compile matching *<name>*
REM   compile_shaders_all.bat -Force           -- force rebuild even if .h exists
REM   compile_shaders_all.bat -Clean           -- delete all generated .h files
REM   compile_shaders_all.bat -Help            -- show help
REM =====================================================

setlocal EnableExtensions EnableDelayedExpansion

REM Capture this before argument parsing shifts %0 away.
set "SCRIPT_DIR=%~dp0"

REM Default parameters
set "SHADER_DIR=shaders"
set "DEBUG_MODE=0"
set "FILTER=*"
set "FORCE_REBUILD=0"
set "DO_CLEAN=0"

REM Parse command line arguments
:parse_args
if "%~1"=="" goto end_parse_args
if /i "%~1"=="-ShaderDir" (
    set "SHADER_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-Debug" (
    set "DEBUG_MODE=1"
    shift
    goto parse_args
)
if /i "%~1"=="-Filter" (
    set "FILTER=*%~2*"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-Force" (
    set "FORCE_REBUILD=1"
    shift
    goto parse_args
)
if /i "%~1"=="-Clean" (
    set "DO_CLEAN=1"
    shift
    goto parse_args
)
if /i "%~1"=="-Help" goto show_help
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="/?" goto show_help
shift
goto parse_args

:end_parse_args
if "%DO_CLEAN%"=="1" goto do_clean
goto main

:show_help
echo.
echo Usage: compile_shaders_all.bat [options]
echo.
echo Options:
echo   -ShaderDir ^<path^>   Shader source directory (default: shaders)
echo   -Filter ^<name^>     Only compile shaders matching *^<name^>*
echo   -Debug              Compile in debug mode (optimization=0)
echo   -Force              Force rebuild even if .h exists
echo   -Clean              Delete all generated .h files
echo   -Help               Show this help
echo.
echo Output:
echo   *.h header files generated next to each *.sc source file
echo   Each header contains binary arrays for spv/dx11/dx12/mtl/essl
echo.
goto :eof

:do_clean
echo Cleaning generated shader headers...
REM Resolve CLEAN_DIR using for loop to handle relative paths
set "CLEAN_DIR=%SHADER_DIR%"
REM Check if relative path (doesn't contain colon)
echo %SHADER_DIR% | find ":" >nul
if errorlevel 1 (
    REM Relative path - prepend script directory
    set "CLEAN_DIR=%~dp0%SHADER_DIR%"
)
REM Normalize path (remove trailing backslash issues)
if not exist "%CLEAN_DIR%" (
    echo [ERROR] Shader directory not found: %CLEAN_DIR%
    echo [INFO] SHADER_DIR=%SHADER_DIR%
    exit /b 1
)
echo Cleaning in: %CLEAN_DIR%
for /r "%CLEAN_DIR%" %%F in (*.h) do (
    echo   DEL %%~nxF
    del /f /q "%%F" >nul 2>nul
)
echo Done.
exit /b 0

:main

REM -- Resolve SHADER_DIR to absolute path --
if "%SHADER_DIR%"=="" (
    set "SHADER_DIR_ABS=%SCRIPT_DIR%"
) else (
    set "SHADER_DIR_ABS=%SHADER_DIR%"
    REM Check if it's a relative path and prepend SCRIPT_DIR if needed
    echo %SHADER_DIR% | findstr /C:":" >nul
    if errorlevel 1 set "SHADER_DIR_ABS=%SCRIPT_DIR%\%SHADER_DIR%"
)

if not exist "%SHADER_DIR_ABS%" (
    echo [ERROR] Shader directory not found: %SHADER_DIR_ABS%
    exit /b 1
)
set "INCLUDE_DIR=%SHADER_DIR_ABS%\headers"

REM -- Find shaderc.exe --
set "SHADERC="
for %%P in (
    "%SCRIPT_DIR%\tools\shaderc.exe"
    "%SCRIPT_DIR%\..\tools\shaderc.exe"
    "%SCRIPT_DIR%\..\tools\Windows\shaderc.exe"
    "%SCRIPT_DIR%\..\build\tools\shaderc.exe"
    "%SCRIPT_DIR%\..\build\Release\shaderc.exe"
    "%SCRIPT_DIR%\..\build\Debug\shaderc.exe"
) do (
    if exist "%%~P" (
        set "SHADERC=%%~fP"
        goto :found_shaderc
    )
)
echo [ERROR] shaderc.exe not found. SCRIPT_DIR=%SCRIPT_DIR%
exit /b 1

:found_shaderc

REM -- Optimization/debug --
if "%DEBUG_MODE%"=="1" (
    set "OPT=-O 0 --debug"
    set "MODE_TEXT=Debug"
) else (
    set "OPT=-O 3"
    set "MODE_TEXT=Release"
)

echo.
echo ============================================================
echo   bgfx Shader Compiler  (generate embedded .h headers)
echo ============================================================
echo   SHADER_DIR  = %SHADER_DIR_ABS%
echo   INCLUDE_DIR = %INCLUDE_DIR%
echo   SHADERC     = %SHADERC%
echo   MODE        = %MODE_TEXT%
echo   FILTER      = %FILTER%
echo ============================================================
echo.

set /a "TOTAL=0"
set /a "SUCCESS=0"
set /a "FAILED=0"
set "FAILED_LIST="

REM -- Compile vertex shaders (both *.vertex.sc and vs_*.sc patterns) --
for /r "%SHADER_DIR_ABS%" %%F in (*.sc) do (
    if /i "%%~nxF"=="vertex.sc" call :compile_one "%%F" vertex v
)
for /r "%SHADER_DIR_ABS%" %%F in (*.sc) do (
    if /i "%%~nxF"=="*.vertex.sc" call :compile_one "%%F" vertex v
)
for /r "%SHADER_DIR_ABS%" %%F in (vs_*.sc) do (
    call :compile_one "%%F" vertex v
)

REM -- Compile fragment shaders (both *.fragment.sc and fs_*.sc patterns) --
for /r "%SHADER_DIR_ABS%" %%F in (*.sc) do (
    if /i "%%~nxF"=="frag.sc" call :compile_one "%%F" fragment p
)
for /r "%SHADER_DIR_ABS%" %%F in (*.sc) do (
    if /i "%%~nxF"=="*.fragment.sc" call :compile_one "%%F" fragment p
)
for /r "%SHADER_DIR_ABS%" %%F in (fs_*.sc) do (
    call :compile_one "%%F" fragment p
)

REM -- Compile compute shaders (both *.compute.sc and cs_*.sc patterns) --
for /r "%SHADER_DIR_ABS%" %%F in (*.compute.sc) do (
    call :compile_one "%%F" compute c
)
for /r "%SHADER_DIR_ABS%" %%F in (cs_*.sc) do (
    call :compile_one "%%F" compute c
)

echo ============================================================
echo   Total: %TOTAL%   Success: %SUCCESS%   Failed: %FAILED%
if %FAILED% gtr 0 (
    echo   Failed list:%FAILED_LIST%
)
echo ============================================================
echo Done. Total=%TOTAL% Success=%SUCCESS% Failed=%FAILED%
if %FAILED% gtr 0 exit /b 1
exit /b 0

REM ============================================================================
REM :compile_one "source.sc" vertex|fragment|compute v|p|c
REM
REM Compile one .sc file into a .h header with all platform binary arrays.
REM The .h is written to the same directory as the .sc source.
REM ============================================================================
:compile_one
setlocal EnableDelayedExpansion

set "INPUT=%~1"
set "TYPE=%~2"
set "T=%~3"

REM -- Output: same dir, same base name, .h extension --
set "OUTPUT=%~dpn1.h"
set "TEMP=%~dpn1.temp.h"

REM -- Delete existing .h file if it exists, then rebuild --
if exist "%OUTPUT%" (
    del /f /q "%OUTPUT%" >nul 2>nul
)

REM -- Build variable name: replace non-alnum with _ --
set "VARNAME=%~n1"
set "VARNAME=!VARNAME:.=_!"
set "VARNAME=!VARNAME:-=_!"
set "VARNAME=!VARNAME: =_!"

REM -- Locate varying.def.sc in same directory --
set "VARYING_DEF=%~dp1varying.def.sc"
set "VARYING_ARG="
if exist "!VARYING_DEF!" set "VARYING_ARG=--varyingdef "!VARYING_DEF!""

set /a "TOTAL+=1"
echo [!TOTAL!] !TYPE!: %~nx1

REM -- Write header preamble --
> "!OUTPUT!" (
    echo // Auto-generated shader header. Do not edit.
    echo // Source: %~nx1
    echo #pragma once
    echo #include ^<cstdint^>
)

set "PASS=0"
set "FAIL=0"

REM ---- Platform: SPIR-V (Vulkan) ----
call :run_shaderc linux spirv "!VARNAME!_spv"
if !ERRORLEVEL! equ 0 (set /a "PASS+=1") else (set /a "FAIL+=1")

REM ---- Platform: DX11 (Windows) ----
REM Compute shaders need SM 5.0 for typed UAVs, vs/fs can use SM 4.0
if "%TYPE%"=="compute" (
    call :run_shaderc windows s_5_0 "!VARNAME!_dx11"
) else (
    call :run_shaderc windows s_4_0 "!VARNAME!_dx11"
)
if !ERRORLEVEL! equ 0 (set /a "PASS+=1") else (set /a "FAIL+=1")

REM ---- Platform: DX12 (Windows, SM 5.0) ----
call :run_shaderc windows s_5_0 "!VARNAME!_dx12"
if !ERRORLEVEL! equ 0 (set /a "PASS+=1") else (set /a "FAIL+=1")

REM ---- Platform: Metal (macOS) ----
call :run_shaderc osx metal "!VARNAME!_mtl"
if !ERRORLEVEL! equ 0 (set /a "PASS+=1") else (set /a "FAIL+=1")

REM ---- Platform: ESSL 320 (Android) ----
call :run_shaderc android 320_es "!VARNAME!_essl"
if !ERRORLEVEL! equ 0 (set /a "PASS+=1") else (set /a "FAIL+=1")

REM -- Cleanup temp --
if exist "!TEMP!" del /f /q "!TEMP!" >nul 2>nul

if !FAIL! equ 0 (
    echo       [OK] !PASS!/!PASS! platforms
    set /a "SUCCESS+=1"
) else (
    echo       [WARN] !PASS!/6 platforms ok, !FAIL! failed
    set /a "FAILED+=1"
    set "FAILED_LIST=!FAILED_LIST! %~n1"
)
echo.

endlocal & (
    set /a "TOTAL=%TOTAL%"
    set /a "SUCCESS=%SUCCESS%"
    set /a "FAILED=%FAILED%"
    set "FAILED_LIST=%FAILED_LIST%"
)
goto :eof


REM ============================================================================
REM :run_shaderc <platform_flag> <profile> <bin2c_name>
REM ============================================================================
:run_shaderc
set "RS_PLATFORM=%~1"
set "RS_PROFILE=%~2"
set "RS_BIN2C=%~3"

REM Get the directory of the input shader file
set "RS_INPUT_DIR=%~dp1"
set "RS_LOG=%RS_INPUT_DIR%%RS_BIN2C%.log"

"%SHADERC%" ^
    -f "%INPUT%" ^
    -o "%TEMP%" ^
    --type %TYPE% ^
    --platform %RS_PLATFORM% ^
    --profile %RS_PROFILE% ^
    %OPT% ^
    -i "%SHADER_DIR_ABS%" ^
    -i "%INCLUDE_DIR%" ^
    -i "%SHADER_DIR_ABS%\common" ^
    -i "%SHADER_DIR%\common" ^
    -i "%SCRIPT_DIR%\bgfx\src" ^
    %VARYING_ARG% ^
    --bin2c %RS_BIN2C% ^
    >"%RS_LOG%" 2>&1

if errorlevel 1 (
    echo       [FAIL] %RS_BIN2C% ^(%RS_PLATFORM% %RS_PROFILE%^)
    if exist "%RS_LOG%" (
        type "%RS_LOG%"
        move "%RS_LOG%" "%~dp1%RS_BIN2C%.log" >nul 2>nul
    )
    if exist "%TEMP%" del /f /q "%TEMP%" >nul 2>nul
    exit /b 1
)

REM -- Append compiled binary array to the output header --
if exist "%TEMP%" (
    echo.>> "%OUTPUT%"
    type "%TEMP%" >> "%OUTPUT%"
    del /f /q "%TEMP%" >nul 2>nul
)
if exist "%RS_LOG%" del /f /q "%RS_LOG%" >nul 2>nul
if exist "%~dp1%RS_BIN2C%.log" del /f /q "%~dp1%RS_BIN2C%.log" >nul 2>nul
exit /b 0
