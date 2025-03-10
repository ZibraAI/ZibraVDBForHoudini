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

& ./scripts/clean.ps1 $TargetFolder
& ./scripts/build.ps1 $AdditionalArgs $TargetFolder

Pop-Location
