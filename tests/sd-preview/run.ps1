param([switch]$AddressSanitizer,[ValidateSet(0,1)][int]$Wr2=1)
$SdEdits=1
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo ('build\sd-preview-tests-wr2-'+$Wr2+'-edits-'+$SdEdits);New-Item -ItemType Directory -Force $out|Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'preview-asan.exe'}else{'preview-tests.exe'}
$wr2Sources=if($Wr2){' /I"'+$repo+'\modules\wr2" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$repo+'\modules\wr2\Persistence.cpp" "'+$repo+'\modules\wr2\sha256.cpp"'}else{''}
Push-Location $out
try{
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DPLATFORM_IMX /DTRACKER_CE_ENABLE_WR2='+$Wr2+' /DTRACKER_CE_ENABLE_SD_EDITS='+$SdEdits+' /EHsc /W4 /WX /O2 /MD '+$asan+' /I"'+$out+'" /I"'+$repo+'\core\effector" /I"'+$repo+'\core\FileManager" '+$wr2Sources+' "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "Preview tests failed $LASTEXITCODE"}
}finally{Pop-Location}
