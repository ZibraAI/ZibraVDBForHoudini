#!/usr/bin/env pwsh

param (
   [switch]$Fix,
   [string]$TargetFolder = "build-compile-commands"
)

$RepositeryRoot = "$PSScriptRoot/../.."
Push-Location $RepositeryRoot

if (-not $IsWindows)
{
    Write-Host "Linting is only supported on Windows"
    Pop-Location
    exit 1
}

& "$PSScriptRoot/generate-compile-commands.ps1" -TargetFolder $TargetFolder
if ($LASTEXITCODE -ne 0)
{
    Pop-Location
    exit $LASTEXITCODE
}

$ClangTidy = "$TargetFolder/_deps/clangtools-build/windows-x64/bin/clang-tidy.exe"
$Files = (Get-Content "$TargetFolder/compile_commands.json" -Raw | ConvertFrom-Json).file

$ClangTidyArgs = @("-p", $TargetFolder, "--quiet")
if ($Fix)
{
    $ClangTidyArgs += "--fix"
}

& $ClangTidy @ClangTidyArgs $Files

Pop-Location

exit $LASTEXITCODE
