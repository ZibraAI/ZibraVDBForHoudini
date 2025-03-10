#!/usr/bin/pwsh

param (
   [string]$AdditionalArgs, 
   [string]$TargetFolder = ""
)

if ($TargetFolder -eq "") {
    $TargetFolder = "build"
    if ($IsWindows) {
        $TargetFolder += "-windows"
    } else {
        $TargetFolder = "-linux"
    }
}

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if ($IsWindows)
{
   $AdditionalArgs += " -T v143,host=x64,version=14.35"
}

Invoke-Expression "cmake -S . -B $TargetFolder $AdditionalArgs"

Pop-Location
