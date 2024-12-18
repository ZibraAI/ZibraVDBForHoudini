#!/usr/bin/pwsh

param (
   [string]$AdditionalArgs,
   [string]$TargetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/configure.ps1 $AdditionalArgs $TargetFolder
cmake --build $TargetFolder --config Release

Pop-Location
