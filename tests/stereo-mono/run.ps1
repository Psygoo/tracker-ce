param([switch]$AddressSanitizer,[switch]$UndoOnly,[switch]$UIOnly,[switch]$WrPersistenceOnly,[ValidateSet(0,1)][int]$Wr2=1,[ValidateSet(0,1)][int]$SdEdits=0)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out=Join-Path $repo ('build\stereo-mono-tests-wr2-'+$Wr2+'-edits-'+$SdEdits);New-Item -ItemType Directory -Force $out|Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'mono-asan.exe'}else{'mono-tests.exe'}
$wr2Sources=if($Wr2){' /I"'+$repo+'\modules\wr2" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$repo+'\modules\wr2\Persistence.cpp" "'+$repo+'\modules\wr2\sha256.cpp"'}else{''}
Push-Location $out
try{
 if($WrPersistenceOnly){if(!$Wr2){throw "WrPersistenceOnly requires WR2"};$env:TRACKER_SD_WR_PERSIST_ONLY="1"}
 if($UndoOnly){$env:TRACKER_SD_UNDO_ONLY="1"}
 if($UIOnly){$env:TRACKER_SD_UI_ONLY="1"}
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DPLATFORM_IMX /DTRACKER_CE_ENABLE_WR2='+$Wr2+' /DTRACKER_CE_ENABLE_SD_EDITS='+$SdEdits+' /EHsc /W4 /WX /O2 /MD '+$asan+' /I"'+$out+'" /I"'+$repo+'\core\effector" /I"'+$repo+'\core\FileManager" '+$wr2Sources+' "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "Mono tests failed $LASTEXITCODE"}
}finally{Remove-Item Env:TRACKER_SD_WR_PERSIST_ONLY -ErrorAction SilentlyContinue;Remove-Item Env:TRACKER_SD_UNDO_ONLY -ErrorAction SilentlyContinue;Remove-Item Env:TRACKER_SD_UI_ONLY -ErrorAction SilentlyContinue;Pop-Location}
