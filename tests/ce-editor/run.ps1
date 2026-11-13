param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo 'build\ce-editor-pcm';New-Item -ItemType Directory -Force $out|Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'pcm-asan.exe'}else{'pcm.exe'}
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /wd4458 /wd4309 /O2 /MD '+$asan+' /I"'+$out+'" "'+$PSScriptRoot+'\pcm.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "PCM tests failed $LASTEXITCODE"}
}finally{Pop-Location}
