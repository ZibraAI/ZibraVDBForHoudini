#!/usr/bin/pwsh

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (Test-Path package) {
    Remove-Item -Recurse -Force package
}

$RepositoryRootFullPath = (Get-Item -Path $RepositeryRoot).FullName

& ./scripts/clean-build.ps1 -DZIBRAVDB_OUTPUT_PATH="$($RepositoryRootFullPath)/package/archive" "build-package"

Compress-Archive -Path "$($RepositeryRoot)/package/archive/*" -DestinationPath "$($RepositeryRoot)/package/ZibraVDBForHoudini.zip"

Pop-Location
