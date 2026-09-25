#!/usr/bin/env pwsh

param (
   [string]$AdditionalArgs,
   [string]$TargetFolder = "",
   [switch]$GenerateCompileCommands
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

    if ($GenerateCompileCommands)
    {
        $TargetFolder += "-ninja-config"
    }
}

if ($GenerateCompileCommands)
{
    # PCH is off because clang-tidy can't read PCH made by MSVC or GCC.
    $PlatformOptions = '-G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DZIBRAVDB_FETCH_CLANG_TOOLS=ON'

    if ($IsWindows -and -not (Get-Command cl.exe -ErrorAction SilentlyContinue))
    {
        $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"
        $VsPath = vswhere -version "[17.0,18.0)" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $VsPath)
        {
            Write-Host "Visual Studio 2022 with C++ tools not found"
            exit 1
        }
        & "$VsPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation
    }
}

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

Invoke-Expression "cmake -S . -B $TargetFolder $PlatformOptions $AdditionalArgs"

Pop-Location
