param([switch]$AddressSanitizer,[ValidateSet(0,1)][int]$SdEdits=0)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo ('build\sample-file-tests-edits-'+$SdEdits);New-Item -ItemType Directory -Force $out|Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'mono-asan.exe'}else{'mono-tests.exe'}
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DPLATFORM_IMX /DTRACKER_CE_ENABLE_SD_EDITS='+$SdEdits+' /EHsc /W4 /WX /O2 /MD '+$asan+' /I"'+$out+'" /I"'+$repo+'\core\effector" /I"'+$repo+'\core\FileManager" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "Mono tests failed $LASTEXITCODE"}
}finally{Pop-Location}
