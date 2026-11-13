param([switch]$AddressSanitizer)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Visual Studio C++ tools not found' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$buildDir = Join-Path $repoRoot 'build\wr2-tests'
New-Item -ItemType Directory -Force $buildDir | Out-Null
$asan = if ($AddressSanitizer) { '/fsanitize=address /Zi' } else { '' }
$exeName = if ($AddressSanitizer) { 'wr2-asan.exe' } else { 'wr2-tests.exe' }
Push-Location $buildDir
try {
    $command = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MD ' + $asan +
        ' "' + $repoRoot + '\modules\wr2\WeightedRandom.cpp" "' + $PSScriptRoot + '\test_wr2.cpp" /Fe:' + $exeName + ' && ' + $exeName
    & cmd.exe /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "WR2 build/tests failed: $LASTEXITCODE" }
} finally { Pop-Location }
