@echo off
setlocal enabledelayedexpansion

REM ================= CONFIGURATION =================

REM 1. Your 64-bit Compiler
set COMPILER="D:\Download\winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64ucrt-13.0.0-r4\mingw64\bin\g++.exe"

REM 2. Your 64-bit SFML Path
set SFML_PATH="C:\Users\Fix\Desktop\SFML-3.0.2-windows-gcc-14.2.0-mingw-64-bit"

REM 3. JSON Library Path
set JSON_PATH="C:\Users\Fix\Desktop\map\MyBlahBlahProject\include"

REM =================================================

echo Gathering source files...

set "SRC_FILES="

REM Loop through folders. 
REM Note: %%f ALREADY contains the folder path (e.g., "Character\Character.cpp")

for %%f in (*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Character\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Config\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Input\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Manager\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (MapLoader\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (QuizGame\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Renderer\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"
for %%f in (Utils\*.cpp) do set "SRC_FILES=!SRC_FILES! %%f"

echo Compiling...

%COMPILER% ^
    -fdiagnostics-color=always ^
    -g -std=c++17 ^
    !SRC_FILES! ^
    -I. ^
    -I%SFML_PATH%\include ^
    -I%JSON_PATH% ^
    -L%SFML_PATH%\lib ^
    -lsfml-graphics ^
    -lsfml-window ^
    -lsfml-system ^
    -lsfml-audio ^
    -lsfml-network ^
    -o Game.exe

IF %ERRORLEVEL% NEQ 0 (
    echo.
    echo ****************************
    echo *** BUILD FAILED! ***
    echo ****************************
    pause
) ELSE (
    echo.
    echo *** SUCCESS! Launching Game... ***
    echo.
    Game.exe
)