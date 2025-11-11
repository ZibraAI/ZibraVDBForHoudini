#!/usr/bin/env pwsh
# arm64 and x64 packages on macos share every value of every filter we have in package configs
# so we can't merge them via conventional means, hence this special case
param (
    [Parameter(Mandatory=$true)][string]$ARM64Package,
    [Parameter(Mandatory=$true)][string]$X64Package,
    [Parameter(Mandatory=$true)][string]$DestFolder
)

if (-not (Test-Path "$ARM64Package/ZibraVDB.json")) {
    throw "ARM64 package config not found"
}

if (-not (Test-Path "$X64Package/ZibraVDB.json")) {
    throw "X64 package config not found"
}

$PackageConfig = Get-Content -Raw "$ARM64Package/ZibraVDB.json" | ConvertFrom-Json

$Prefix = $PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | Where-Object -Property Name -CNE -Value "method"
$Prefix = $Prefix.Value.Replace('$ZIBRAVDB_PLUGIN_PATH/', '')

$ARM64DSOPath = "$ARM64Package/ZibraVDB/$Prefix/dso/ZibraVDBForHoudini.dylib"
$X64DSOPath = "$X64Package/ZibraVDB/$Prefix/dso/ZibraVDBForHoudini.dylib"
$DestDSOPath = "$DestFolder/ZibraVDB/$Prefix/dso/ZibraVDBForHoudini.dylib"

if (Test-Path $DestFolder) {
    Remove-Item $DestFolder
}
New-Item -ItemType Directory -Path $DestFolder -Force

Copy-Item -Path "$ARM64Package/*" -Destination $DestFolder -Recurse
Remove-Item $DestDSOPath

& lipo -create -output "$DestDSOPath" "$ARM64DSOPath" "$X64DSOPath"

$NotarizationScriptPath = Join-Path $PSScriptRoot "../apple/sign_and_notarize_dylib.sh"
& bash "$NotarizationScriptPath" "$DestDSOPath"
