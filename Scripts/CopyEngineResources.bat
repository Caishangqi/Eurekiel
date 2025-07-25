@echo off
REM CopyEngineResources.bat - Engine Resources Copy Script
REM Usage: CopyEngineResources.bat <EngineEnigmaPath> <AppEnigmaPath>
REM Placement: Engine/Scripts/CopyEngineResources.bat

set "ENGINE_PATH=%~1"
set "APP_PATH=%~2"

echo [INFO] === Engine Resource Copy Process ===
echo [INFO] Source: %ENGINE_PATH%
echo [INFO] Target: %APP_PATH%

REM Checks if the source path exists
if not exist "%ENGINE_PATH%" (
    echo [WARNING] Engine .enigma folder not found at: %ENGINE_PATH%
    echo [WARNING] This is normal if Engine resources haven't been set up yet
    echo [INFO] To set up Engine resources, create the folder structure:
    echo [INFO]   Engine/.enigma/assets/engine/
    goto :end
)

echo [SUCCESS] Found Engine resources at: %ENGINE_PATH%

REM Ensure that the destination directory exists
if not exist "%APP_PATH%" (
    mkdir "%APP_PATH%" 2>nul
    echo [INFO] Created App .enigma directory: %APP_PATH%
)

REM Replication Engine Namespace Resources
if exist "%ENGINE_PATH%\assets\engine" (
    echo [INFO] Copying Engine assets...
    if not exist "%APP_PATH%\assets" mkdir "%APP_PATH%\assets" 2>nul
    if not exist "%APP_PATH%\assets\engine" mkdir "%APP_PATH%\assets\engine" 2>nul
    
    xcopy /E /Y /I /Q "%ENGINE_PATH%\assets\engine\*" "%APP_PATH%\assets\engine\" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [SUCCESS] Engine assets copied successfully
    ) else (
        echo [WARNING] Failed to copy some Engine assets ^(error code: %errorlevel%^)
    )
) else (
    echo [INFO] No Engine assets found - create Engine/.enigma/assets/engine/ to add Engine resources
)

REM Copy Engine data catalog
if exist "%ENGINE_PATH%\data" (
    echo [INFO] Copying Engine data...
    if not exist "%APP_PATH%\data" mkdir "%APP_PATH%\data" 2>nul
    
    xcopy /E /Y /I /Q "%ENGINE_PATH%\data\*" "%APP_PATH%\data\" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [SUCCESS] Engine data copied successfully
    ) else (
        echo [WARNING] Failed to copy some Engine data ^(error code: %errorlevel%^)
    )
)

REM Copy the Engine config directory (if it exists)
if exist "%ENGINE_PATH%\config\engine" (
    echo [INFO] Copying Engine config...
    if not exist "%APP_PATH%\config" mkdir "%APP_PATH%\config" 2>nul
    if not exist "%APP_PATH%\config\engine" mkdir "%APP_PATH%\config\engine" 2>nul
    
    xcopy /E /Y /I /Q "%ENGINE_PATH%\config\engine\*" "%APP_PATH%\config\engine\" >nul 2>&1
    if %errorlevel% equ 0 (
        echo [SUCCESS] Engine config copied successfully
    ) else (
        echo [WARNING] Failed to copy Engine config ^(error code: %errorlevel%^)
    )
)

REM Generate a simple list of resources
echo [INFO] Generating resource manifest...
set "MANIFEST_FILE=%APP_PATH%\engine_resources.txt"
echo Engine Resource Copy Report > "%MANIFEST_FILE%"
echo Generated: %date% %time% >> "%MANIFEST_FILE%"
echo Source: %ENGINE_PATH% >> "%MANIFEST_FILE%"
echo Target: %APP_PATH% >> "%MANIFEST_FILE%"
echo. >> "%MANIFEST_FILE%"

if exist "%APP_PATH%\assets\engine" (
    echo Engine Assets: >> "%MANIFEST_FILE%"
    dir /s /b "%APP_PATH%\assets\engine\*.*" 2>nul | find /v "File Not Found" >> "%MANIFEST_FILE%"
)

echo [SUCCESS] Resource manifest generated: %MANIFEST_FILE%
echo [SUCCESS] === Engine Resource Copy Completed ===

:end
exit /b 0