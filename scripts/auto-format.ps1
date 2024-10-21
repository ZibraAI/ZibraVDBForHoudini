$RepositeryRoot = "$PSScriptRoot/.."
Push-Location $RepositeryRoot

$Path = 'src'
$Include = @('*.h', '*.hpp', '*.cpp')
$FilesToFormat = Get-ChildItem -Recurse -Path $Path -Include $Include
clang-format -i $FilesToFormat.FullName

Pop-Location
