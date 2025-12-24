#!/usr/bin/env pwsh

param (
   [string]$AdditionalArgs, 
   [string]$TargetFolder = ""
)

$PlatformOptions = ""

if ($TargetFolder -eq "") {
    $TargetFolder = "build"

    if ($IsWindows)
    {
        $TargetFolder += "-windows-x64"
        $PlatformOptions = '-G "Visual Studio 17 2022"'
    } 
    elseif ($IsLinux) 
    {
        $TargetFolder += "-linux-x64"
        $PlatformOptions =  '-G "Ninja Multi-Config"'
    }
    elseif ($IsMacOS) 
    {
        $TargetFolder += "-macos-arm64"
        $PlatformOptions = '-G "Xcode" -DCMAKE_OSX_ARCHITECTURES=arm64'
    }
    else 
    {
        Write-Host "Unknown OS"
        exit 1
    }
}

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

Invoke-Expression "cmake -S . -B $TargetFolder $PlatformOptions $AdditionalArgs"

Pop-Location
