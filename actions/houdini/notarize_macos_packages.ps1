#!/usr/bin/env pwsh

param (
    [Parameter(Mandatory=$true)][string]$Prefix
)

# Find all folders that start with the given prefix
$Packages = Get-ChildItem -Path "." -Filter "$($Prefix)*" | Where-Object { $_.PSIsContainer -and $_.Name -match "macosx_arm64" } | ForEach-Object { $_.FullName }
Write-Output "Merging $($Packages.Count) pairs of macOS packages"

$NotarizationScriptPath = Join-Path $PSScriptRoot "../apple/sign_and_notarize_dylib.sh"

foreach ($Package in $Packages) {
    Write-Output "Notarizing package: $Package"

    if (-not (Test-Path "$Package/ZibraVDB.json")) {
        throw "Package config not found"
    }

    $PackageConfig = Get-Content -Raw "$Package/ZibraVDB.json" | ConvertFrom-Json
    $Target = $PackageConfig.env[2].HOUDINI_PATH[1].PSObject.Properties | Where-Object -Property Name -CNE -Value "method"
    $Target = $Target.Value.Replace('$ZIBRAVDB_PLUGIN_PATH/', '')

    $DSOPath = "$Package/ZibraVDB/$Target/dso/ZibraVDBForHoudini.dylib"

    if (-not (Test-Path $DSOPath)) {
        throw "DSO not found at path: $DSOPath"
    }

    & bash "$NotarizationScriptPath" "$DSOPath"
    Write-Output "Notarized $DSOPath successfully"
}
