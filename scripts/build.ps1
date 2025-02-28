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

& ./scripts/configure.ps1 $AdditionalArgs $TargetFolder
cmake --build $TargetFolder --config Release

Pop-Location
