param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$contactWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$contactZig=$ZigPath
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $contactWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $contactWork 'zig-local-cache'
$contactMh=Join-Path $projectRoot 'third_party\minhook'
$contactSrc=Join-Path $projectRoot 'src'
$contactBuild=Join-Path $contactWork 'contact-bin-25661166'
$contactProfile=Get-Content (Join-Path $projectRoot 'compatibility\steam-25661166-telemetry.json') -Raw | ConvertFrom-Json
foreach($entry in $contactProfile.files) {
    if((Get-FileHash -LiteralPath (Join-Path $contactProfile.install_path $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256){throw '14200 build/header changed'}
}
New-Item -ItemType Directory -Force -Path $contactBuild | Out-Null
$contactCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $contactMh 'include'),'-I',$contactSrc,'-I',(Join-Path $contactProfile.install_path 'Support\SharedMemoryInterface'))
$contactSources=@((Join-Path $contactSrc 'contact_native_25661166.cpp'),(Join-Path $contactMh 'src\buffer.c'),(Join-Path $contactMh 'src\hook.c'),(Join-Path $contactMh 'src\trampoline.c'),(Join-Path $contactMh 'src\hde\hde64.c'))
& $contactZig @contactCommon '-shared' '-DLMU_CONTACT_FIXTURE' @contactSources '-ladvapi32' '-luser32' '-o' (Join-Path $contactBuild 'contact-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Contact fixture DLL build failed'}
& $contactZig @contactCommon (Join-Path $contactSrc 'contact_fixture_25661166.cpp') '-luser32' '-o' (Join-Path $contactBuild 'contact-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Contact fixture executable build failed'}
& $contactZig @contactCommon '-shared' @contactSources '-ladvapi32' '-luser32' '-o' (Join-Path $contactBuild 'lmu-contact-native-25661166.dll')
if($LASTEXITCODE -ne 0){throw 'Contact DLL build failed'}
& (Join-Path $contactBuild 'contact-fixture.exe') (Join-Path $contactBuild 'contact-fixture.dll')
if($LASTEXITCODE -ne 0){throw "Contact fixture failed: $LASTEXITCODE"}
& (Join-Path $contactBuild 'contact-fixture.exe') (Join-Path $contactBuild 'contact-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw "Contact watchdog fixture failed: $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $contactBuild 'lmu-contact-native-25661166.dll') -Destination (Join-Path $projectRoot 'bin\lmu-contact-native-25661166.dll')
$contactHashes=[ordered]@{}
foreach($contactName in @('contact_native_25661166.cpp','contact_ipc_25661166.h','contact_fixture_25661166.cpp')) {
    $contactHashes[$contactName]=(Get-FileHash -LiteralPath (Join-Path $contactSrc $contactName) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$contactManifest=[ordered]@{
    utc=[DateTime]::UtcNow.ToString('o'); build=14200
    exe_sha256='66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf'
    dll_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\lmu-contact-native-25661166.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    target_rva='0xef8880'; prolog_hex='4c894c242048894c2408555356574154415541564157'
    abi='Win64 void(tire* RCX, double dt XMM1, double horizon XMM2, world* R9, double angularSpeed stack5)'
    contact_experiment_only=$true; full_active_reset_compatible=$false; default_mode='observation'
    write_scope='One-shot depth retirement at native refresh entry, stationary pits only; original performs removal/query/cache updates.'
    source_sha256=$contactHashes; fixture_stop_passed=$true; fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\steam-25661166-contact-experiment.json'),($contactManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output 'Contact module built; both fixture processes passed. No game access performed by this build.'
