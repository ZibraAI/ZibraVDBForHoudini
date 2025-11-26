#!/usr/bin/env pwsh
param (
    [Parameter(Mandatory=$true)][string]$HFS,
    [Parameter(Mandatory=$true)][string]$PluginVersion,
    [Parameter(Mandatory=$true)][string]$DSOLibrary,
    [Parameter(Mandatory=$true)][string]$AssetResolverLibrary,
    [Parameter(Mandatory=$true)][string]$AssetResolverConfig,
    [Parameter(Mandatory=$true)][string]$DestFolder
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

Write-Output "Running post-build step"
Write-Output "HFS: $HFS"
Write-Output "Plugin Version: $PluginVersion"
Write-Output "DSO Library: $DSOLibrary"
Write-Output "Asset Resolver Library: $AssetResolverLibrary"
Write-Output "Destination Folder: $DestFolder"

# We need to parse houdini_setup_bash to get few required parameters
# Input we parse has following format:
#    export HHP="${HH}/python3.11libs"
#    export HOUDINI_MAJOR_RELEASE=20
#    export HOUDINI_MINOR_RELEASE=5
#    export HOUDINI_BUILD_VERSION=684
#    export HOUDINI_BUILD_COMPILER="19.35.32217.1"
$HoudiniSetupBashPath = "$($HFS)/houdini_setup_bash"
$HoudiniSetupBashContent = Get-Content -Path $HoudiniSetupBashPath
$HoudiniSetupMatches = $HoudiniSetupBashContent | Select-String -Pattern "export (.*)=(.*)"
$HoudiniSetupValues = @{}
foreach ($Match in $HoudiniSetupMatches)
{
    $HoudiniSetupValues[$Match.Matches[0].Groups[1].Value] = $Match.Matches[0].Groups[2].Value.Trim('"')
}
$PythonVersionMatch = $HoudiniSetupValues["HHP"] | Select-String -Pattern "(python.*)libs"
$CompilerVersionMatch = $HoudiniSetupValues["HOUDINI_BUILD_COMPILER"] | Select-String -Pattern "^([0-9]*\.[0-9]*)\..*$"

if ($IsWindows)
{
    $HoudiniOS = "windows"
    $HoudiniPlatform = "cl"
}
elseif ($IsLinux)
{
    $HoudiniOS = "linux"
    $HoudiniPlatform = "gcc"
}
elseif ($IsMacOS)
{
    $HoudiniOS = "macos"
    $HoudiniPlatform = "clang"
}
else
{
    throw "Unexpected OS"
}

$HoudiniVersionFull = "$($HoudiniSetupValues["HOUDINI_MAJOR_RELEASE"]).$($HoudiniSetupValues["HOUDINI_MINOR_RELEASE"]).$($HoudiniSetupValues["HOUDINI_BUILD_VERSION"])"
$HoudiniPython = $PythonVersionMatch.Matches[0].Groups[1].Value
$HoudiniPlatform = "$($HoudiniPlatform).$($CompilerVersionMatch.Matches[0].Groups[1].Value)"

Write-Output "Houdini version: $($HoudiniVersionFull)"
Write-Output "Houdini OS: $($HoudiniOS)"
Write-Output "Houdini Python: $($HoudiniPython)"
Write-Output "Houdini platform: $($HoudiniPlatform)"

$PackageJSONPath = "$DestFolder/ZibraVDB.json"
$PackageFolder = "$DestFolder/ZibraVDB"
$PlatformSpecificFolder = "$PackageFolder/$($HoudiniOS)_$($HoudiniVersionFull)_$($HoudiniPython)_$($HoudiniPlatform)"
$DSOFolder = "$PlatformSpecificFolder/dso"
$AssetResolverFolder = "$PlatformSpecificFolder/usd"
$AssetResolverDestConfigFolder = "$PlatformSpecificFolder/usd_plugins/ZibraVDBResolver/resources"
$AssetResolverDestConfigPath = "$AssetResolverDestConfigFolder/plugInfo.json"
$OTLSFolder = "$PackageFolder/otls"

if (Test-Path $PackageJSONPath) {
    Remove-Item $PackageJSONPath
}
if (Test-Path $PackageFolder) {
    Remove-Item -Recurse -Force $PackageFolder
}
New-Item -ItemType Directory -Path $PackageFolder | Out-Null
New-Item -ItemType Directory -Path $PlatformSpecificFolder | Out-Null
New-Item -ItemType Directory -Path $DSOFolder | Out-Null
New-Item -ItemType Directory -Path $AssetResolverFolder | Out-Null
New-Item -ItemType Directory -Path $AssetResolverDestConfigFolder -Force | Out-Null
New-Item -ItemType Directory -Path $OTLSFolder | Out-Null

$HoudiniPackageConfig = @"
{
    "show" : true,
    "enable" : "houdini_os == '$($HoudiniOS)' and houdini_version == '$($HoudiniVersionFull)' and houdini_python == '$($HoudiniPython)' and houdini_platform_build == '$($HoudiniPlatform)'",
    "version": "$PluginVersion",
    "env": [
        {
            "ZIBRAVDB_PLUGIN_PATH": "`$HOUDINI_PACKAGE_PATH/ZibraVDB"
        },
        {
            "ZIBRAVDB_VERSION": "$PluginVersion"
        },
        {
            "HOUDINI_PATH": [
                {
                    "method": "append",
                    "value": "`$ZIBRAVDB_PLUGIN_PATH"
                },
                {
                    "method": "append",
                    "houdini_os == '$($HoudiniOS)' and houdini_version == '$($HoudiniVersionFull)' and houdini_python == '$($HoudiniPython)' and houdini_platform_build == '$($HoudiniPlatform)'": "`$ZIBRAVDB_PLUGIN_PATH/$($HoudiniOS)_$($HoudiniVersionFull)_$($HoudiniPython)_$($HoudiniPlatform)"
                }
            ]
        }
    ]
}
"@

$HoudiniPackageConfig | Out-File -FilePath $PackageJSONPath

function Copy-MergeFolders($SourceRoot, $DestRoot)
{
    $SourceRoot = (Resolve-Path $SourceRoot).Path
    $DestRoot = (Resolve-Path $DestRoot).Path
    if (-not (Test-Path $SourceRoot)) { throw "$SourceRoot does not exist" }
    if (-not (Test-Path $DestRoot)) { throw "$DestRoot does not exist" }

    Get-ChildItem -Path $SourceRoot -Recurse -File -Force | ForEach-Object {
        $RelativePath = $_.FullName.Substring($SourceRoot.Length).TrimStart('\','/')
        $DestPath = Join-Path $DestRoot $RelativePath
        $DestDir = Split-Path $DestPath -Parent
        if (-not (Test-Path $DestDir)) { New-Item -ItemType Directory -Path $DestDir -Force | Out-Null}
        Copy-Item -Path $_.FullName -Destination $DestPath -Force
    }
}

Copy-MergeFolders -SourceRoot "./assets" -DestRoot $PackageFolder
Copy-MergeFolders -SourceRoot "./assetsPlatformSpecific" -DestRoot $PlatformSpecificFolder

if ($IsWindows)
{
    $HOTLPath = "$($HFS)/bin/hotl.exe"
}
else
{
    $HOTLPath = "$($HFS)/bin/hotl"
}

Get-ChildItem ./HDA/ | ForEach-Object {
    if (-not $_.PSIsContainer)
    {
        throw "Unexpected file $($_.FullName) when iterating HDA folder. Only unpacked HDAs are expected in HDA folder."
    }
    $SourceHDA = $_.FullName
    $DestHDA = "$($OTLSFolder)/$($_.Name)"

    & $HOTLPath -l $SourceHDA $DestHDA
}

Write-Output "Successfully created package at $PackageJSONPath"
Copy-Item -Path $DSOLibrary -Destination $DSOFolder
Write-Output "Copied DSO library to $DSOFolder"

Copy-Item -Path $AssetResolverLibrary -Destination $AssetResolverFolder
Write-Output "Copied Asset Resolver library to $AssetResolverFolder"

Copy-Item -Path $AssetResolverConfig -Destination $AssetResolverDestConfigPath
Write-Output "Copied Asset Resolver config to $AssetResolverDestConfigPath"

Pop-Location
