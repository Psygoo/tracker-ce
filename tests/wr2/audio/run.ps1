param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-audio-tests'
New-Item -ItemType Directory $out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC tools not found'}
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''}
$name=if($AddressSanitizer){'audio-asan.exe'}else{'audio-tests.exe'}
Push-Location $out
try {
    & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MD /DWR2_AUDIO_TEST_HOOKS '+$asan+' "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$PSScriptRoot+'\test_audio.cpp" /Fe:'+ $name +' && '+$name)
    if($LASTEXITCODE){throw "Audio harness failed: $LASTEXITCODE"}
}finally{Pop-Location}
