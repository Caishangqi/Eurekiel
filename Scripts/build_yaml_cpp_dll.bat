@echo off
echo Building yaml-cpp as DLL...

cd "F:\p4\Personal\SD\Engine\Code\ThirdParty\yaml-cpp-src"

REM Clean existing build directory
if exist "build-dll" rmdir /s /q "build-dll"
mkdir "build-dll"
cd build-dll

REM Configure with CMake for DLL build
echo Configuring yaml-cpp as DLL with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DBUILD_SHARED_LIBS=ON ^
    -DYAML_CPP_BUILD_TESTS=OFF ^
    -DYAML_CPP_BUILD_TOOLS=OFF ^
    -DYAML_CPP_BUILD_CONTRIB=OFF ^
    -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON

if errorlevel 1 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build Debug and Release versions
echo Building Debug version...
cmake --build . --config Debug
if errorlevel 1 (
    echo Debug build failed!
    pause
    exit /b 1
)

echo Building Release version...
cmake --build . --config Release  
if errorlevel 1 (
    echo Release build failed!
    pause
    exit /b 1
)

REM Copy DLLs and libs to .enigma/lib
echo Copying DLLs to .enigma/lib...
if not exist "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib" mkdir "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib"

copy "Debug\yaml-cpp.dll" "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib\yaml-cpp_d.dll" /Y
copy "Debug\yaml-cpp.lib" "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib\yaml-cpp_d.lib" /Y
copy "Release\yaml-cpp.dll" "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib\yaml-cpp.dll" /Y  
copy "Release\yaml-cpp.lib" "F:\p4\Personal\SD\EurekielFeatureTest\Run\.enigma\lib\yaml-cpp.lib" /Y

echo yaml-cpp DLL build completed successfully!
echo DLLs and import libraries copied to .enigma/lib/
pause