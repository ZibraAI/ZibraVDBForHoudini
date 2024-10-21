Param($additionalArgs)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/clean.ps1
& ./scripts/build.ps1 $additionalArgs

Pop-Location
