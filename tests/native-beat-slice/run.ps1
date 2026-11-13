param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo 'build\native-beat-slice-tests'
New-Item -ItemType Directory $out -Force | Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$flags=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''}
$name=if($AddressSanitizer){'native-asan.exe'}else{'native-tests.exe'}
Push-Location $out
try {
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MD '+$flags+' /I"'+$out+'" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name)
 if($LASTEXITCODE){throw 'Native test compilation failed'}
 $run='call "'+$vcvars+'" >nul && "'+(Join-Path $out $name)+'"'
 & cmd.exe /d /c $run
 if($LASTEXITCODE){throw 'Native regression failed'}
 if(!$AddressSanitizer){foreach($case in @('baseline-fx-zero','baseline-end-zero')){
  & cmd.exe /d /c ($run+' '+$case)
  if($LASTEXITCODE -ne -1073741676){throw 'Expected baseline divide-by-zero was not observed'}
  Write-Output "BASELINE REPRO $case : expected 0xC0000094; corrected zero-count paths passed in-process"
 }}
}finally{Pop-Location}