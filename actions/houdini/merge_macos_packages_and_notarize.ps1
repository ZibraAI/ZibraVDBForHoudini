#!/usr/bin/env pwsh
# arm64 and x64 packages on macos share every value of every filter we have in package configs
# so we can't merge them via conventional means, hence this special case
param (
    [Parameter(Mandatory=$true)][string]$Prefix
)

# Find all folders that start with the given prefix
$ARM64Packages = Get-ChildItem -Path "." -Filter "$($Prefix)*" | Where-Object { $_.PSIsContainer -and $_.Name -match "macosx_arm64" } | ForEach-Object { $_.FullName }
Write-Output "Merging $($ARM64Packages.Count) pairs of macOS packages"

foreach ($ARM64Package in $ARM64Packages) {
    $X64Package = $ARM64Package -replace "macosx_arm64", "macosx_x86_64"
    $DestFolder = $ARM64Package -replace "macosx_arm64", "macos_universal"

    Write-Output "Merging packages:"
    Write-Output "  ARM64 Package: $ARM64Package"
    Write-Output "  X64 Package: $X64Package"

    if (-not (Test-Path "$ARM64Package/ZibraVDB.json")) {
        throw "ARM64 package config not found"
    }

    if (-not (Test-Path "$X64Package/ZibraVDB.json")) {
        throw "X64 package config not found"
    }

    $PackageConfig = Get-Content -Raw "$ARM64Package/ZibraVDB.json" | ConvertFrom-Json

    # WIP
    Write-Output "PackageConfig: $($PackageConfig | ConvertTo-Json -Depth 10)"
    Write-Output "PackageConfig.env[2] $($PackageConfig.env[2] | ConvertTo-Json -Depth 10)"
    Write-Output "PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties $($PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | ConvertTo-Json -Depth 10)"

    $Target = $PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | Where-Object -Property Name -CNE -Value "method"

    Write-Output "Target 1: $($Target | ConvertTo-Json -Depth 10)"
    Write-Output "Target.Value: $($Target.Value | ConvertTo-Json -Depth 10)"

    $Target = $Target.Value.Replace('$ZIBRAVDB_PLUGIN_PATH/', '')
    Write-Output "Target 2: $($Target | ConvertTo-Json -Depth 10)"

    $ARM64DSOPath = "$ARM64Package/ZibraVDB/$Target/dso/ZibraVDBForHoudini.dylib"
    $X64DSOPath = "$X64Package/ZibraVDB/$Target/dso/ZibraVDBForHoudini.dylib"
    $DestDSOPath = "$DestFolder/ZibraVDB/$Target/dso/ZibraVDBForHoudini.dylib"

    if (-not (Test-Path $ARM64DSOPath)) {
        throw "ARM64 DSO not found at path: $ARM64DSOPath"
    }
    if (-not (Test-Path $X64DSOPath)) {
        throw "X64 DSO not found at path: $X64DSOPath"
    }

    if (Test-Path $DestFolder) {
        Remove-Item $DestFolder -Recurse -Force
    }
    New-Item -ItemType Directory -Path $DestFolder -Force | Out-Null

    Copy-Item -Path "$ARM64Package/*" -Destination $DestFolder -Recurse
    Remove-Item $DestDSOPath

    & lipo -create -output "$DestDSOPath" "$ARM64DSOPath" "$X64DSOPath"

    $NotarizationScriptPath = Join-Path $PSScriptRoot "../apple/sign_and_notarize_dylib.sh"
    & bash "$NotarizationScriptPath" "$DestDSOPath"

    Remove-Item $ARM64Package -Recurse -Force
    Remove-Item $X64Package -Recurse -Force
}
