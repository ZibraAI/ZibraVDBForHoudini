param (
   [string]$targetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (Test-Path $targetFolder) {
    Remove-Item -Recurse -Force $targetFolder
}

Pop-Location
