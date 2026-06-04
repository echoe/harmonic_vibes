@echo off
echo --- STARTING WINDOWS CMAKE BUILD ---

:: 1. Completely nuke the old build cache folder if it exists
if exist build (
    echo Clearing existing build directory...
    rmdir /s /q build
)

:: 2. Recreate a clean build folder
mkdir build
cd build

:: 3. Configure the project using the default Visual Studio generator
cmake ..

:: 4. Build the Release binaries using all available CPU cores
cmake --build . --config Release --parallel

echo --- BUILD COMPLETE ---
pause
