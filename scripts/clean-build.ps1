#!/usr/bin/pwsh

param (
   [string]$AdditionalArgs,
   [string]$TargetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/clean.ps1 $TargetFolder
& ./scripts/build.ps1 $AdditionalArgs $TargetFolder

Pop-Location
