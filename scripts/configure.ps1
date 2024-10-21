Param($additionalArgs)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

cmake -S . -B build -T "v143,host=x64,version=14.35" $additionalArgs

Pop-Location
