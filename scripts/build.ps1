#!/usr/bin/env pwsh

param (
   [string]$AdditionalArgs,
   [string]$TargetFolder = ""
)

if ($TargetFolder -eq "") {
    $TargetFolder = "build"

    if ($IsWindows)
    {
        $TargetFolder += "-windows-x64"
    } 
    elseif ($IsLinux) 
    {
        $TargetFolder += "-linux-x64"
    }
    elseif ($IsMacOS) 
    {
        $TargetFolder += "-macos-arm64"
    }
    else 
    {
        Write-Host "Unknown OS"
        exit 1
    }
}

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/configure.ps1 $AdditionalArgs $TargetFolder
cmake --build $TargetFolder --config Release

Pop-Location
