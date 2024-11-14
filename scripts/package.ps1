$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (Test-Path package) {
    Remove-Item -Recurse -Force package
}

& ./scripts/clean-build.ps1 -DH_OUTPUT_INSTDIR="$($RepositeryRoot)/package/plugin" "build-package"

Copy-Item -Path ./assets -Destination "$($RepositeryRoot)/package/archive" -Recurse
New-Item ./package/archive/dso -Type Directory
Copy-Item -Path "$($RepositeryRoot)/package/plugin/ZibraVDBForHoudini.dll" -Destination "$($RepositeryRoot)/package/archive/dso/ZibraVDBForHoudini.dll"
Compress-Archive -Path "$($RepositeryRoot)/package/archive/*" -DestinationPath "$($RepositeryRoot)/package/ZibraVDBForHoudini.zip"

Pop-Location
