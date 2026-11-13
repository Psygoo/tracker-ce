$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\preintegration\irq-test'
& python (Join-Path $PSScriptRoot 'extract_irq.py') $repo $out
if($LASTEXITCODE){throw 'IRQ extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /W4 /WX /O2 /EHsc irq_contract.cpp /Fe:irq-contract.exe && irq-contract.exe')
 if($LASTEXITCODE){throw 'IRQ contract test failed'}
}finally{Pop-Location}