#!/usr/bin/env pwsh

param (
   [switch]$Fix,
   [string]$TargetFolder = "build-windows-x64-ninja-config"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

$ClangTidy = (Get-ChildItem $TargetFolder -Recurse -File -Filter "clang-tidy*").FullName
$Files = (Get-Content "$TargetFolder/compile_commands.json" -Raw | ConvertFrom-Json).file

$ClangTidyArgs = @("-p", $TargetFolder, "--quiet")
if ($Fix)
{
    $ClangTidyArgs += "--fix"
}

& $ClangTidy @ClangTidyArgs $Files

Pop-Location

exit $LASTEXITCODE
