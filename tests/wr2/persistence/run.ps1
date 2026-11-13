param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-persistence-tests';New-Item -ItemType Directory -Force $out | Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'persist-asan.exe'}else{'persist-tests.exe'}
Push-Location $out
try {
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DTRACKER_CE_ENABLE_WR2=1 /EHsc /W4 /WX /O2 /MD '+$asan+' /I"'+$repo+'\modules\wr2" /I"'+$out+'" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$repo+'\modules\wr2\sha256.cpp" "'+$repo+'\modules\wr2\Companion.cpp" "'+$repo+'\modules\wr2\Persistence.cpp" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "Persistence tests failed: $LASTEXITCODE"}
}finally{Pop-Location}
