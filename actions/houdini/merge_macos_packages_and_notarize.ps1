#!/usr/bin/env pwsh
# arm64 and x64 packages on macos share every value of every filter we have in package configs
# so we can't merge them via conventional means, hence this special case
param (
    [Parameter(Mandatory=$true)][string]$Prefix
)

# Find all folders that start with the given prefix
$ARM64Packages = Get-ChildItem -Path "." -Filter "$($Prefix)*" | Where-Object { $_.PSIsContainer -and $_.Name -match "macosx_arm64" } | ForEach-Object { $_.FullName }
Write-Output "Merging $($ARM64Packages.Count) pairs of macOS packages"

$NotarizationScriptPath = Join-Path $PSScriptRoot "../apple/sign_and_notarize_dylib.sh"

foreach ($ARM64Package in $ARM64Packages) {
    $X64Package = $ARM64Package -replace "macosx_arm64", "macosx_x86_64"

    if (-not (Test-Path "$X64Package")) {
        throw "X64 package not found for ARM64 package: $ARM64Package"
    }

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

    $ARM64PackageConfig = Get-Content -Raw "$ARM64Package/ZibraVDB.json" | ConvertFrom-Json
    $ARM64Target = $ARM64PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | Where-Object -Property Name -CNE -Value "method"
    $ARM64Target = $ARM64Target.Value.Replace('$ZIBRAVDB_PLUGIN_PATH/', '')

    $X64PackageConfig = Get-Content -Raw "$X64Package/ZibraVDB.json" | ConvertFrom-Json
    $X64Target = $X64PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | Where-Object -Property Name -CNE -Value "method"
    $X64Target = $X64Target.Value.Replace('$ZIBRAVDB_PLUGIN_PATH/', '')

    $ARM64DSOPath = "$ARM64Package/ZibraVDB/$ARM64Target/dso/ZibraVDBForHoudini.dylib"
    $X64DSOPath = "$X64Package/ZibraVDB/$X64Target/dso/ZibraVDBForHoudini.dylib"

    if (-not (Test-Path $ARM64DSOPath)) {
        throw "ARM64 DSO not found at path: $ARM64DSOPath"
    }
    if (-not (Test-Path $X64DSOPath)) {
        throw "X64 DSO not found at path: $X64DSOPath"
    }

    if ($ARM64Target -ne $X64Target) {
        Write-Output "Mismatched targets between ARM64 and X64 packages: '$ARM64Target' != '$X64Target'"
        Write-Output "Notarizing separately"
        & bash "$NotarizationScriptPath" "$ARM64DSOPath"
        Write-Output "Notarized $ARM64DSOPath - ARM64 binary"
        & bash "$NotarizationScriptPath" "$X64DSOPath"
        Write-Output "Notarized $X64DSOPath - X64 binary"
        # Not deleting original packages in this case
        continue
    }

    Write-Output "Target platform for both packages: $ARM64Target"

    $DestDSOPath = "$DestFolder/ZibraVDB/$ARM64Target/dso/ZibraVDBForHoudini.dylib"

    if (Test-Path $DestFolder) {
        Remove-Item $DestFolder -Recurse -Force
    }
    New-Item -ItemType Directory -Path $DestFolder -Force | Out-Null

    Copy-Item -Path "$ARM64Package/*" -Destination $DestFolder -Recurse
    Remove-Item $DestDSOPath

    & lipo -create -output "$DestDSOPath" "$ARM64DSOPath" "$X64DSOPath"

    & bash "$NotarizationScriptPath" "$DestDSOPath"
    Write-Output "Notarized $DestDSOPath - Universal binary"

    Remove-Item $ARM64Package -Recurse -Force
    Remove-Item $X64Package -Recurse -Force
}
