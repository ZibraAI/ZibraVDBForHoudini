$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

if (Test-Path build) {
    Remove-Item -Recurse -Force build
}
if (Test-Path package) {
    Remove-Item -Recurse -Force package
}

Pop-Location
