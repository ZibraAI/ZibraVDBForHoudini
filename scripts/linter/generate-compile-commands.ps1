#!/usr/bin/env pwsh

param (
   [string]$TargetFolder = "build-compile-commands"
)

# Ninja, doesn't find MSVC by itself
if ($IsWindows -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"
    $VsPath = vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $VsPath)
    {
        Write-Host "Visual Studio with C++ tools not found"
        exit 1
    }
    & "$VsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation
}

$RepositeryRoot = "$PSScriptRoot/../.."
Push-Location $RepositeryRoot

cmake -S scripts/linter -B $TargetFolder -G "Ninja Multi-Config"

Pop-Location

exit $LASTEXITCODE
