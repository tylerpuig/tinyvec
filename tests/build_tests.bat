@echo off
setlocal enabledelayedexpansion

:: Create build directories if they don't exist
if not exist build mkdir build
if not exist build\tests mkdir build\tests

:: Compile and run all tests
for %%f in (tests\test_*.c) do (
    echo.
    echo Compiling %%f...
    
    :: Extract base name without 'test_' prefix
    set "source_name=%%~nf"
    set "source_name=!source_name:test_=!"
    
    :: Compile
    gcc -I./src/core/include "%%f" "src/core/src/!source_name!.c" -mavx2 -O2 -Wall -o "build/tests/%%~nf.exe"
    
    if !errorlevel! equ 0 (
        echo Running %%~nf.exe...
        echo -----------------------------
        build\tests\%%~nf.exe
        echo -----------------------------
    ) else (
        echo Error: Compilation failed for %%f
        echo -----------------------------
    )
)

endlocal