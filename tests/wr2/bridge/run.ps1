param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-bridge-tests'
New-Item -ItemType Directory -Path $out -Force | Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Native sync extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC tools missing'}
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''}
$name=if($AddressSanitizer){'bridge-asan.exe'}else{'bridge-tests.exe'}
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MD /DWR2_TRANSPORT_TEST_HOOK '+$asan+' /I"'+$out+'" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$PSScriptRoot+'\test_bridge.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "Bridge model failed: $LASTEXITCODE"}
}finally{Pop-Location}
