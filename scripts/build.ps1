param(
    [ValidateSet('x64','ARM64')][string]$Platform = 'x64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$CertificateThumbprint,
    [switch]$Rebuild,
    [switch]$DriverOnly,
    [switch]$Unsigned,
    [switch]$RequireTrustedSignature,
    [switch]$RequireIsolation
)
. "$PSScriptRoot\common.ps1"
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (!$vs) { throw 'Visual Studio with MSBuild and C++ x64/ARM64 driver tools is required' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
$target = if ($DriverOnly) { 'NxpNfcClientDriver.vcxproj' } else { 'NxpNfcClientDriver.sln' }
$logDir = Join-Path $DriverRoot "out\$Platform\$Configuration\logs"
New-Item -ItemType Directory -Force $logDir | Out-Null
$argsList = @((Join-Path $DriverRoot $target), "/p:Platform=$Platform", "/p:Configuration=$Configuration", '/nologo', '/v:minimal', '/m')
if ($Rebuild) { $argsList += '/t:Rebuild' }
if ($Unsigned) { $argsList += '/p:NxpTestSign=false' }
if ($RequireTrustedSignature) { $argsList += '/p:NxpRequireTrustedSignature=true' }
if ($RequireIsolation) { $argsList += '/p:NxpRequireIsolation=true' }
if ($CertificateThumbprint) { $argsList += "/p:NxpSigningCertificate=$CertificateThumbprint" }
$log = Join-Path $logDir ('build-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '.log')
& $msbuild @argsList 2>&1 | Tee-Object -FilePath $log
$code = $LASTEXITCODE
[ordered]@{ utc=[DateTime]::UtcNow.ToString('o'); executable=$msbuild; arguments=$argsList; exit_code=$code; log=$log } |
    ConvertTo-Json -Depth 4 | Set-Content (Join-Path $logDir 'last-build.json')
if ($code) { throw "MSBuild failed with exit $code; see $log" }
