#!/usr/bin/pwsh

param (
   [string]$AdditionalArgs, 
   [string]$TargetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if ($IsWindows)
{
   $AdditionalArgs += " -T v143,host=x64,version=14.35"
}

if ($IsLinux)
{
   $AdditionalArgs += " -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11"
}

Invoke-Expression "cmake -S . -B $TargetFolder $AdditionalArgs"

Pop-Location
