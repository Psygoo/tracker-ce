param([switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$out=Join-Path $repo 'build\wr2-audio-tests'
New-Item -ItemType Directory $out -Force | Out-Null
& python (Join-Path $PSScriptRoot 'extract_native.py') $repo $out
if($LASTEXITCODE){throw 'Native extraction failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$flags=if($AddressSanitizer){'/fsanitize=address /Zi'}else{''}
$name=if($AddressSanitizer){'native-probe-asan.exe'}else{'native-probe.exe'}
Push-Location $out
try {
    & cmd.exe /d /c ('call "'+$vcvars+'" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Od /MD '+$flags+' /I"'+$out+'" "'+$PSScriptRoot+'\native_probe.cpp" "'+$repo+'\modules\wr2\WeightedRandom.cpp" "'+$repo+'\modules\wr2\AudioAdapter.cpp" /Fe:'+ $name)
    if($LASTEXITCODE){throw 'Native fixture compilation failed'}
    $runCommand='call "'+$vcvars+'" >nul && "'+(Join-Path $out $name)+'"'
    & cmd.exe /d /c $runCommand
    if($LASTEXITCODE){throw 'Native probe checks failed'}
    foreach($case in $(if($AddressSanitizer){@()}else{@('fx-zero','end-fx-zero')})) {
        & cmd.exe /d /c ($runCommand+' '+$case)
        $result=$LASTEXITCODE
        if($result -ne -1073741676){throw "Expected integer divide-by-zero 0xC0000094 for $case; got $result"}
        Write-Output "REPRODUCED $case : Windows exception 0xC0000094 (integer divide by zero), expected"
    }
} finally {Pop-Location}