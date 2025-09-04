# CopyEngineResources.ps1
# Copy Engine resources to the App's .enigma folder
# Usage: CopyEngineResources.ps1 <EngineEnigmaPath> <AppEnigmaPath>

param(
    [Parameter(Mandatory = $true)]
    [string]$EngineEnigmaPath,

    [Parameter(Mandatory = $true)]
    [string]$AppEnigmaPath
)

function Write-BuildLog
{
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "HH:mm:ss"
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $(
    switch ($Level)
    {
        "INFO" {
            "White"
        }
        "SUCCESS" {
            "Green"
        }
        "WARNING" {
            "Yellow"
        }
        "ERROR" {
            "Red"
        }
    }
    )
}

function Copy-EngineResources
{
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    Write-BuildLog "Starting Engine resource copy process..."

    # Check if the source path exists
    if (-not (Test-Path $SourcePath))
    {
        Write-BuildLog "Engine .enigma folder not found at: $SourcePath" "WARNING"
        Write-BuildLog "Skipping Engine resource copy." "WARNING"
        return
    }

    Write-BuildLog "Found Engine resources at: $SourcePath" "SUCCESS"

    # Ensure that the destination directory exists
    if (-not (Test-Path $DestinationPath))
    {
        New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
        Write-BuildLog "Created App .enigma directory: $DestinationPath" "INFO"
    }

    # Get all Engine resources
    $engineItems = Get-ChildItem -Path $SourcePath -Recurse
    $copiedCount = 0
    $skippedCount = 0

    foreach ($item in $engineItems)
    {
        # Calculate relative paths
        $relativePath = $item.FullName.Substring($SourcePath.Length + 1)
        $destinationItem = Join-Path $DestinationPath $relativePath

        if ($item.PSIsContainer)
        {
            # Processing catalogs
            if (-not (Test-Path $destinationItem))
            {
                New-Item -ItemType Directory -Path $destinationItem -Force | Out-Null
                Write-BuildLog "Created directory: assets/$($relativePath.Replace('\', '/') )" "INFO"
            }
        }
        else
        {
            # Processing documents
            $destinationDir = Split-Path $destinationItem -Parent
            if (-not (Test-Path $destinationDir))
            {
                New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null
            }

            # Check if the file already exists (to avoid overwriting the app's custom resources)
            if (Test-Path $destinationItem)
            {
                # 检查是否为引擎特定文件（可以安全覆盖）
                $isEngineSpecific = $relativePath -match "^assets[\\\/]engine[\\\/]" -or
                        $relativePath -match "^data[\\\/]" -or
                        $relativePath -match "^config[\\\/]engine"

                if ($isEngineSpecific)
                {
                    Copy-Item $item.FullName $destinationItem -Force
                    Write-BuildLog "Updated engine file: assets/$($relativePath.Replace('\', '/') )" "INFO"
                    $copiedCount++
                }
                else
                {
                    Write-BuildLog "Skipped existing app file: assets/$($relativePath.Replace('\', '/') )" "WARNING"
                    $skippedCount++
                }
            }
            else
            {
                # New file, direct copy
                Copy-Item $item.FullName $destinationItem -Force
                Write-BuildLog "Copied: assets/$($relativePath.Replace('\', '/') )" "SUCCESS"
                $copiedCount++
            }
        }
    }

    Write-BuildLog "Engine resource copy completed!" "SUCCESS"
    Write-BuildLog "Files copied: $copiedCount, Files skipped: $skippedCount" "INFO"

    # Generate resource inventory files
    Generate-ResourceManifest -AppEnigmaPath $DestinationPath
}

function Generate-ResourceManifest
{
    param([string]$AppEnigmaPath)

    Write-BuildLog "Generating resource manifest..." "INFO"

    $manifestPath = Join-Path $AppEnigmaPath "resource_manifest.json"
    $manifest = @{
        generated_at = Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ"
        engine_resources = @()
        app_resources = @()
        namespaces = @()
    }

    # Scanning all resources
    $assetsPath = Join-Path $AppEnigmaPath "assets"
    if (Test-Path $assetsPath)
    {
        $namespaces = Get-ChildItem -Path $assetsPath -Directory | ForEach-Object { $_.Name }
        $manifest.namespaces = $namespaces

        foreach ($namespace in $namespaces)
        {
            $namespacePath = Join-Path $assetsPath $namespace
            $resources = Get-ChildItem -Path $namespacePath -Recurse -File | ForEach-Object {
                $relativePath = $_.FullName.Substring($namespacePath.Length + 1)
                "${namespace}:$($relativePath.Replace('\', '/') )"
            }

            if ($namespace -eq "engine")
            {
                $manifest.engine_resources += $resources
            }
            else
            {
                $manifest.app_resources += $resources
            }
        }
    }

    # Write the manifest file
    $manifest | ConvertTo-Json -Depth 3 | Set-Content $manifestPath -Encoding UTF8
    Write-BuildLog "Resource manifest generated: $manifestPath" "SUCCESS"
}

# Main execution logic
try
{
    Write-BuildLog "=== Engine Resource Copy Process ===" "INFO"
    Write-BuildLog "Source: $EngineEnigmaPath" "INFO"
    Write-BuildLog "Destination: $AppEnigmaPath" "INFO"

    Copy-EngineResources -SourcePath $EngineEnigmaPath -DestinationPath $AppEnigmaPath

    Write-BuildLog "=== Process Completed Successfully ===" "SUCCESS"
}
catch
{
    Write-BuildLog "Error during resource copy: $( $_.Exception.Message )" "ERROR"
    exit 1
}