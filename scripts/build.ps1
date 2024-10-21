Param($additionalArgs)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/configure.ps1 $additionalArgs
cmake --build build --config Release

Pop-Location
