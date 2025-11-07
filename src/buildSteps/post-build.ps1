#!/usr/bin/env pwsh
param (
    [Parameter(Mandatory=$true)][string]$HFS,
    [Parameter(Mandatory=$true)][string]$pluginVersion,
    [Parameter(Mandatory=$true)][string]$dsoFile,
    [Parameter(Mandatory=$true)][string]$outputDir
)

$RepositeryRoot = "$PSScriptRoot/../.."
Push-Location $RepositeryRoot

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

$PackageJSON = "$outputDir/ZibraVDB.json"
$PackageFolder = "$outputDir/ZibraVDB"
$PlatformSpecificFolder = "$PackageFolder/$($HoudiniOS)_$($HoudiniVersionFull)_$($HoudiniPython)_$($HoudiniPlatform)"
$DSOFolder = "$PlatformSpecificFolder/dso"
$OTLSFolder = "$PackageFolder/otls"

if (Test-Path $PackageJSON) {
    Write-Output Removing old $PackageJSON
    Remove-Item $PackageJSON
}
if (Test-Path $PackageFolder) {
    Write-Output Removing old $PackageFolder
    Remove-Item -Recurse -Force $PackageFolder
}
New-Item -ItemType Directory -Path $PackageFolder
New-Item -ItemType Directory -Path $PlatformSpecificFolder
New-Item -ItemType Directory -Path $DSOFolder
New-Item -ItemType Directory -Path $OTLSFolder

$JSONContent = @"
{
    "show" : true,
    "enable" : "houdini_os == '$($HoudiniOS)' and houdini_version == '$($HoudiniVersionFull)' and houdini_python == '$($HoudiniPython)' and houdini_platform_build == '$($HoudiniPlatform)'",
    "version": "$pluginVersion",
    "env": [
        {
            "ZIBRAVDB_PLUGIN_PATH": "`$HOUDINI_PACKAGE_PATH/ZibraVDB"
        },
        {
            "ZIBRAVDB_VERSION": "$pluginVersion"
        },
        {
            "ZIBRAVDB_ENABLED": [
                {
                    "method": "replace",
                    "process_order": 1,
                    "value": "FALSE"
                },
                {
                    "method": "replace",
                    "process_order": 1,
                    "houdini_os == '$($HoudiniOS)' and houdini_version == '$($HoudiniVersionFull)' and houdini_python == '$($HoudiniPython)' and houdini_platform_build == '$($HoudiniPlatform)'": "TRUE"
                }
            ]
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

Write-Output "Writing package config to $PackageJSON"
Write-Output "Config: $JSONContent"
$JSONContent | Out-File -FilePath $PackageJSON

Copy-Item -Path ./assets/* -Destination $PackageFolder -Recurse
Copy-Item -Path ./assetsPlatformSpecific/* -Destination $PlatformSpecificFolder -Recurse

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

    Write-Output "Packing $($_.Name) into binary form and copying to $($OTLSFolder)"
    & $HOTLPath -l $SourceHDA $DestHDA
}

Copy-Item -Path $dsoFile -Destination $DSOFolder

Pop-Location
