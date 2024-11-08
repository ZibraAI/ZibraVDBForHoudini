param (
   [string]$additionalArgs,
   [string]$targetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

& ./scripts/configure.ps1 $additionalArgs $targetFolder
cmake --build $targetFolder --config Release

Pop-Location
