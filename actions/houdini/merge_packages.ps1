#!/usr/bin/env pwsh
param (
    [Parameter(Mandatory=$true)][string]$Prefix,
    [Parameter(Mandatory=$true)][string]$DestFolder
)

$SourcePackages =  Get-ChildItem -Path "." -Filter "$($Prefix)*" | Where-Object { $_.PSIsContainer } | ForEach-Object { $_.FullName }

if ($SourcePackages.Count -eq 0)
{
    throw "Source Packages can't be empty"
}

Write-Output "Merging $($SourcePackages.Count) packages"
foreach ($Package in $SourcePackages)
{
    Write-Output "  Source Package: $Package"
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
New-Item -ItemType Directory -Path $DestFolder -Force | Out-Null
New-Item -ItemType Directory -Path "$DestFolder/ZibraVDB" -Force | Out-Null

$ResultingConfig | ConvertTo-Json -Depth 100 | Out-File "$DestFolder/ZibraVDB.json"

function Copy-SkipExisting($SourceRoot, $DestRoot)
{
    $SourceRoot = (Resolve-Path $SourceRoot).Path
    $DestRoot = (Resolve-Path $DestRoot).Path
    if (-not (Test-Path $SourceRoot)) { throw "$SourceRoot does not exist" }
    if (-not (Test-Path $DestRoot)) { throw "$DestRoot does not exist" }

    Get-ChildItem -Path $SourceRoot -Recurse -File -Force | ForEach-Object {
        $RelativePath = $_.FullName.Substring($SourceRoot.Length).TrimStart('\','/')
        $DestPath = Join-Path $DestRoot $RelativePath
        $DestDir = Split-Path $DestPath -Parent
        if (Test-Path $DestPath) { return }
        if (-not (Test-Path $DestDir)) { New-Item -ItemType Directory -Path $DestDir -Force | Out-Null }
        Copy-Item -Path $_.FullName -Destination $DestPath -Force | Out-Null
    }
}

$SourcePackages | ForEach-Object {
    Copy-SkipExisting "$_/ZibraVDB" "$DestFolder/ZibraVDB"
}
