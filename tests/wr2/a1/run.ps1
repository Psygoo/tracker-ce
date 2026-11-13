param([switch]$AddressSanitizer,[switch]$NativeBaseline,[switch]$FirmwareBridge,[switch]$RealAmp,[switch]$IO,[switch]$UiLifecycle,[switch]$Imports)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-a1-tests'; New-Item -ItemType Directory -Path $out -Force | Out-Null
& python (Join-Path $PSScriptRoot 'extract.py') $repo $out
if($LASTEXITCODE){throw 'Extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC missing'}
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$asan=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''};$name=if($AddressSanitizer){'a1-asan.exe'}else{'a1-tests.exe'}
$defines=if($NativeBaseline){'/DA1_NATIVE_BASELINE'}else{''}
if($NativeBaseline){$name='baseline-'+$name}
if($FirmwareBridge){$defines='/DA1_FIRMWARE_BRIDGE /DWR2_BRIDGE_HOST /DTRACKER_CE_ENABLE_WR2=1';$name='firmware-'+$name}
if($UiLifecycle){$defines='/DA1_FIRMWARE_BRIDGE /DWR2_BRIDGE_HOST /DTRACKER_CE_ENABLE_WR2=1 /DA1_UI_LIFECYCLE';$name='lifecycle-'+$name}
if($Imports){$IO=$true;$defines='/DA1_FIRMWARE_BRIDGE /DWR2_BRIDGE_HOST /DTRACKER_CE_ENABLE_WR2=1 /DA1_IMPORTS';$name='imports-'+$name}
if($IO){
 & python (Join-Path $PSScriptRoot 'extract_io.py') $repo $out
 if($LASTEXITCODE){throw 'IO extraction failed'}
 $defines+=' /DA1_IO /DPLATFORM_IMX';$name='io-'+$name
}
if($RealAmp){$defines+=' /DA1_REAL_AMP';$name='real-'+$name}
Push-Location $out
try {
 & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /O2 /MD '+$asan+' '+$defines+' /I"'+$out+'" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" "'+$repo+'\modules\wr2\UiModel.cpp" "'+$PSScriptRoot+'\test.cpp" /Fe:'+ $name +' && '+$name)
 if($LASTEXITCODE){throw "A1 fixture failed: $LASTEXITCODE"}
}finally{Pop-Location}
