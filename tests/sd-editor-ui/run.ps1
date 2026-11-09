param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo 'build\sd-editor-ui';New-Item -ItemType Directory -Force $out|Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'ui-asan.exe'}else{'ui.exe'}
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /wd4458 /wd4244 /wd4309 /O2 /MD '+$asan+' /I"'+$out+'" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "SD UI tests failed $LASTEXITCODE"}
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /wd4458 /wd4244 /wd4309 /wd4189 /wd4267 /wd5208 /O2 /MD '+$asan+' /I"'+$out+'" "'+$PSScriptRoot+'\graphics.cpp" /Fe:graphics-'+$name+' && graphics-'+$name)
 if($LASTEXITCODE){throw "Graphics tests failed $LASTEXITCODE"}
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /wd4458 /wd4244 /wd4309 /wd4189 /wd4267 /wd5208 /O2 /MD '+$asan+' /I"'+$out+'" "'+$PSScriptRoot+'\transitions.cpp" /Fe:transitions-'+$name+' && transitions-'+$name)
 if($LASTEXITCODE){throw "Transition tests failed $LASTEXITCODE"}
}finally{Pop-Location}
