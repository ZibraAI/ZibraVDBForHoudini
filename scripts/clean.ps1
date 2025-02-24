#!/usr/bin/pwsh

param (
   [string]$TargetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (Test-Path $TargetFolder) {
    Remove-Item -Recurse -Force $TargetFolder
}

Pop-Location
