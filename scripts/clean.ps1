#!/usr/bin/pwsh

param (
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

if (Test-Path $TargetFolder) {
    Remove-Item -Recurse -Force $TargetFolder
}

Pop-Location
