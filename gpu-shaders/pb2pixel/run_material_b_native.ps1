param([string]$Packet='D:\nioh3-re\deliveries\pb2-material-b-native-20261012',
      [string]$Repo='C:\Users\Ahmad Hamza\nioh3-engine')
$ErrorActionPreference='Stop'
if(Get-Process Nioh3 -ErrorAction SilentlyContinue){throw 'Close game before this independent compiler gate'}
$binding=Join-Path $Packet 'binding.json'
if((Get-FileHash -LiteralPath $binding).Hash.ToLower() -ne '8bd0e44288a48d95568f8d8126a0c5e11e4f291e5d9639e88accdd6851da564e'){throw 'Wrong bound packet'}
$manifest=Get-Content -LiteralPath $binding -Raw | ConvertFrom-Json
function CheckInputs {
 foreach($p in $manifest.files.PSObject.Properties){
  if((Get-FileHash -LiteralPath (Join-Path $Packet $p.Name)).Hash.ToLower() -ne $p.Value){throw "Changed input: $($p.Name)"}
 }
}
CheckInputs
$native=Join-Path $Packet 'native'
$setup=Join-Path $Repo 'tools\msvc\setup_x64.bat'
$batch=Join-Path $native 'build-admit.cmd'
@"
@echo off
call "$setup" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /EHsc /std:c++17 /O2 pixel_admit.cpp /Fepixel_admit.exe /link d3d12.lib dxgi.lib
"@ | Set-Content -LiteralPath $batch -Encoding ASCII
Push-Location $native
try {
 & cmd.exe /d /c $batch
 if($LASTEXITCODE -ne 0){throw 'Compiler helper build failed'}
 CheckInputs
 $output=@(& .\pixel_admit.exe 2>&1)
 $exit=$LASTEXITCODE
 CheckInputs
 [ordered]@{scope='EXACT_PS05AD_VS70D_WINDOWS_PSO_NO_DRAW';utc=[DateTime]::UtcNow.ToString('o');
   candidate_sha256=$manifest.candidate_shader_sha256;binding_sha256=(Get-FileHash $binding).Hash.ToLower();
   helper_exe_sha256=(Get-FileHash .\pixel_admit.exe).Hash.ToLower();exit_code=$exit;
   output=($output -join "`n");draws_executed=$false;image_gate='UNRUN';performance_gate='UNRUN'} |
   ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Packet 'native-admission.json')
 $output
 if($exit -ne 0){throw 'Actual native PSO gate failed; see receipt'}
} finally {Pop-Location}
