#!/usr/bin/env pwsh
param (
    [Parameter(Mandatory=$true)][string[]]$SourcePackages,
    [Parameter(Mandatory=$true)][string]$DestFolder
)

if ($SourcePackages.Count -eq 0)
{
    throw "Source Packages can't be empty"
}

foreach ($Package in $SourcePackages)
{
    if (-not (Test-Path "$Package/ZibraVDB.json")) {
        throw "Package config not found in $Package"
    }
}

$PackageConfigs = $SourcePackages | ForEach-Object { Get-Content -Raw "$_/ZibraVDB.json" | ConvertFrom-Json }

$ResultingConfig = $PackageConfigs[0]

$ResultingConfig.enable = $PackageConfigs | ForEach-Object { "($($_.enable))" } | Join-String -Separator ' or '
$ResultingConfig.env[2].HOUDINI_PATH = @($PackageConfigs[0].env[2].HOUDINI_PATH[0]; $PackageConfigs | ForEach-Object { $_.env[2].HOUDINI_PATH[1] })

if (Test-Path $DestFolder) {
    Remove-Item $DestFolder
}
New-Item -ItemType Directory -Path $DestFolder -Force

$ResultingConfig | ConvertTo-Json -Depth 100 | Out-File "$DestFolder/ZibraVDB.json"

$SourcePackages | ForEach-Object {
    & robocopy "$_/ZibraVDB" "$DestFolder/ZibraVDB" /xc /xn /xo /s
}
