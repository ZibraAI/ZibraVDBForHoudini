param (
   [string]$additionalArgs,
   [string]$targetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/clean.ps1 $targetFolder
& ./scripts/build.ps1 $additionalArgs $targetFolder

Pop-Location
