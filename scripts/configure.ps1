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

Invoke-Expression "cmake -S . -B $TargetFolder $AdditionalArgs"

Pop-Location
