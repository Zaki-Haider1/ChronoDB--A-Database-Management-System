@echo off
echo Compiling ChronoDB GUI...

g++ -std=c++17 -o chronodb_gui.exe -I. -I "raylib-5.5_win64_mingw-w64/include" -L "raylib-5.5_win64_mingw-w64/lib" src/gui.cpp query/lexer.cpp query/parser.cpp storage/storage.cpp graph/graph.cpp utils/helpers.cpp utils/sorting.cpp -lraylib -lgdi32 -lwinmm

if %ERRORLEVEL% EQU 0 (
    echo.
    echo =========================================
    echo   COMPILATION SUCCESS!
    echo =========================================
    echo Run chronodb_gui.exe to start.
) else (
    echo.
    echo =========================================
    echo   COMPILATION FAILED
    echo =========================================
    echo Check the errors above.
)
pause
