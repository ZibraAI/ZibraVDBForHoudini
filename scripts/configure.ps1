param (
   [string]$additionalArgs, 
   [string]$targetFolder = "build"
)

$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

cmake -S . -B $targetFolder -T "v143,host=x64,version=14.35" -DZCE_NO_STATIC_API_DECL=1 $additionalArgs

Pop-Location
