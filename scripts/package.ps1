$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (-not $Env:ZIBRA_HOUDINI_PATH) {
    Write-Host "ZIBRA_HOUDINI_PATH environment variable is not set. Please set it to the root of your Houdini installation."
    Exit 1
}

if (-not $IsWindows)
{
    Write-Host "This script is only supported on Windows."
    Exit 1
}

$HDAVersion = "0.2"

if (Test-Path package) {
    Remove-Item -Recurse -Force package
}

& ./scripts/clean-build.ps1 -DH_OUTPUT_INSTDIR="$($RepositeryRoot)/package/plugin" "build-package"

# Prepare the assets
Copy-Item -Path ./assets -Destination "$($RepositeryRoot)/package/archive" -Recurse
# Packs HDA back into binary format replacing the original
$HOTLPath = $Env:ZIBRA_HOUDINI_PATH + "/bin/hotl.exe"
$HDAPath = "$($RepositeryRoot)/package/archive/otls/zibravdb_filecache.$($HDAVersion).hda"
$TempHDAPath = $HDAPath + ".2"
& $HOTLPath -l $HDAPath $TempHDAPath
Remove-Item -Recurse -Force $HDAPath
Move-Item -Path $TempHDAPath -Destination $HDAPath
# Add the compiled plugin to the package
New-Item ./package/archive/dso -Type Directory
Copy-Item -Path "$($RepositeryRoot)/package/plugin/ZibraVDBForHoudini.dll" -Destination "$($RepositeryRoot)/package/archive/dso/ZibraVDBForHoudini.dll"
# Compress the archive, compression is disabled to prevent anti virus false positives
Compress-Archive -CompressionLevel NoCompression -Path "$($RepositeryRoot)/package/archive/*" -DestinationPath "$($RepositeryRoot)/package/ZibraVDBForHoudini.zip"

Pop-Location
