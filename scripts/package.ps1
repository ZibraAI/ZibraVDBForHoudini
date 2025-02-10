#!/usr/bin/pwsh

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (-not $Env:ZIBRA_HOUDINI_PATH -and -not $Env:HFS) {
    Write-Host "ZIBRA_HOUDINI_PATH and HFS environment variables are not set. Please set one of those to the root of your Houdini installation."
    Exit 1
}

if (-not $Env:ZIBRA_HOUDINI_PATH) {
    $HoudiniPath = $Env:HFS
}
else {
    $HoudiniPath = $Env:ZIBRA_HOUDINI_PATH
}

if (-not $IsWindows -and -not $IsLinux)
{
    Write-Host "This script is only supported on Windows."
    Exit 1
}

$HDAVersion = "0.2"

if (Test-Path package) {
    Remove-Item -Recurse -Force package
}

$RepositoryRootFullPath = (Get-Item -Path $RepositeryRoot).FullName

& ./scripts/clean-build.ps1 -DZIBRAVDB_OUTPUT_PATH="$($RepositoryRootFullPath)/package/archive" "build-package"

# Prepare the assets
Copy-Item -Path ./assets -Destination "$($RepositeryRoot)/package/archive" -Recurse
# Packs HDA back into binary format replacing the original
$HOTLPath = $HoudiniPath
if ($IsWindows)
{
    $HOTLPath = "$($HOTLPath)/bin/hotl.exe"
}
else
{
    $HOTLPath = "$($HOTLPath)/bin/hotl"
}
$HDAPath = "$($RepositeryRoot)/package/archive/otls/zibravdb_filecache.$($HDAVersion).hda"
$TempHDAPath = $HDAPath + ".2"
& $HOTLPath -l $HDAPath $TempHDAPath
Remove-Item -Recurse -Force $HDAPath
Move-Item -Path $TempHDAPath -Destination $HDAPath
# Compress the archive, compression is disabled to prevent anti virus false positives
Compress-Archive -CompressionLevel NoCompression -Path "$($RepositeryRoot)/package/archive/*" -DestinationPath "$($RepositeryRoot)/package/ZibraVDBForHoudini.zip"

Pop-Location
