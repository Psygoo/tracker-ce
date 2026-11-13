param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-ui-tests';New-Item -ItemType Directory -Force $out | Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'ui-asan.exe'}else{'ui-tests.exe'}
Push-Location $out
try {
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /wd4458 /O2 /MD '+$asan+' /DTRACKER_CE_ENABLE_WR2=1 /I"'+$out+'" /I"'+$repo+'\core\source" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$repo+'\modules\wr2\UiModel.cpp" "'+$repo+'\core\source\mtPadsBacklight.cpp" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "UI tests failed: $LASTEXITCODE"}
}finally{Pop-Location}
