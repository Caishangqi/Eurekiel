@echo off
REM Build script for yaml-cpp library
echo Building yaml-cpp library...

REM Create build directory
if not exist "F:\p4\Personal\SD\Engine\Code\ThirdParty\yaml-cpp\build" mkdir "F:\p4\Personal\SD\Engine\Code\ThirdParty\yaml-cpp\build"

REM Clone yaml-cpp source if not present
cd "F:\p4\Personal\SD\Engine\Code\ThirdParty\"
if not exist "yaml-cpp-src" (
    echo Cloning yaml-cpp source...
    git clone https://github.com/jbeder/yaml-cpp.git yaml-cpp-src
)

cd yaml-cpp-src

REM Create build directory
if not exist "build" mkdir "build"
cd build

REM Configure with CMake
echo Configuring yaml-cpp with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF

REM Build the library
echo Building yaml-cpp library...
msbuild yaml-cpp.sln /p:Configuration=Debug /p:Platform=x64
msbuild yaml-cpp.sln /p:Configuration=Release /p:Platform=x64

REM Copy built libraries to the original yaml-cpp directory
echo Copying built libraries...
xcopy "Debug\*.lib" "..\..\yaml-cpp\" /Y /I
xcopy "Release\*.lib" "..\..\yaml-cpp\" /Y /I

echo yaml-cpp build completed!
pause