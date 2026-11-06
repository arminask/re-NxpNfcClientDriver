param(
    [ValidateSet('x64','ARM64')][string]$Platform = 'x64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('true','false')][string]$TestSign = 'true',
    [string]$CertificateThumbprint,
    [switch]$EmbeddedSignature,
    [switch]$RequireTrustedSignature,
    [switch]$RequireIsolation
)
. "$PSScriptRoot\common.ps1"
if ($EmbeddedSignature -and $TestSign -ne 'true') { throw 'EmbeddedSignature requires TestSign=true' }
$output = Join-Path $DriverRoot "out\$Platform\$Configuration"
$package = Join-Path $output 'package'
$logs = Join-Path $output 'logs'
New-Item -ItemType Directory -Force $package,$logs | Out-Null
foreach ($reportName in @('package-integrity.json','signing.json','inf-validation.json')) {
    $oldReport = Join-Path $logs $reportName
    if (Test-Path -LiteralPath $oldReport) { Remove-Item -LiteralPath $oldReport }
}
$dll = Join-Path $output 'bin\NxpNfcClientDriver.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build the driver first with scripts\build.ps1' }
$firmware = Get-Content -Raw (Join-Path $DriverRoot 'firmware\SHA256.json') | ConvertFrom-Json
foreach ($f in $firmware) {
    $path = Join-Path $DriverRoot "firmware\$($f.filename)"
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $f.sha256) { throw "Firmware hash mismatch: $path" }
}
$expected = @('NxpNfcClientDriver.dll','NxpNfcClientDriver.inf','NXPPN547FW.DAT','NXPPN548FW.DAT',"NxpNfcClientDriver-$Platform.cat")
# Refuse unknown files; never clean arbitrary user files out of an output folder.
foreach ($existing in Get-ChildItem -LiteralPath $package -Force) {
    if ($existing.PSIsContainer -or $existing.Name -notin $expected) { throw "Unexpected package content: $($existing.FullName)" }
}
# Invalidate an earlier catalog before modifying payload contents.
$cat = Join-Path $package "NxpNfcClientDriver-$Platform.cat"
if (Test-Path -LiteralPath $cat) { Remove-Item -LiteralPath $cat }
Copy-Item -LiteralPath $dll -Destination $package
Copy-Item -LiteralPath (Join-Path $DriverRoot 'package\NxpNfcClientDriver.inf') -Destination $package
foreach ($f in $firmware) { Copy-Item -LiteralPath (Join-Path $DriverRoot "firmware\$($f.filename)") -Destination $package }
if ($EmbeddedSignature) {
    # UMDF DLLs can use the signed package catalog. Embedded signing is optional.
    $cert = Get-DevelopmentCertificate $CertificateThumbprint -CreateIfMissing
    Invoke-CheckedTool $SignTool @('sign','/v','/fd','SHA256','/s','My','/sha1',$cert.Thumbprint,(Join-Path $package 'NxpNfcClientDriver.dll'))
}
$inf = Join-Path $package 'NxpNfcClientDriver.inf'
$infResults = @()
foreach ($mode in @('base','u','w','h')) {
    $toolArguments = @('/v')
    if ($mode -ne 'base') { $toolArguments += "/$mode" }
    $toolArguments += $inf
    $run = Invoke-CapturedTool $InfVerif $toolArguments
    $run.Output | Set-Content -LiteralPath (Join-Path $logs "InfVerif-$mode.log")
    $codes = @([regex]::Matches($run.Output,'ERROR\((\d+)\)') | ForEach-Object { [int]$_.Groups[1].Value })
    $infResults += [ordered]@{mode=$mode; exit_code=$run.ExitCode; error_codes=$codes}
    if ($run.ExitCode -eq 0) { Write-Host $run.Output }
    else { throw "InfVerif $mode failed: $($run.ExitCode); see retained log" }
}
$infResults | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logs 'inf-validation.json')
if ($RequireIsolation -and @($infResults | Where-Object exit_code -ne 0).Count) {
    throw 'Modern INF isolation validation failed; see retained logs'
}
$os = if ($Platform -eq 'x64') { '10_GE_X64' } else { '10_GE_ARM64' }
& $Inf2Cat "/driver:$package" "/os:$os" /verbose 2>&1 | Tee-Object -FilePath (Join-Path $logs 'Inf2Cat.log')
if ($LASTEXITCODE) { throw "Inf2Cat failed: $LASTEXITCODE" }
if (!(Test-Path -LiteralPath $cat)) { throw 'Architecture-specific catalog was not produced' }
if ($TestSign -eq 'true') {
    $cert = Get-DevelopmentCertificate $CertificateThumbprint -CreateIfMissing
    & "$PSScriptRoot\sign.ps1" -File $cat -CertificateThumbprint $cert.Thumbprint `
        -Report (Join-Path $logs 'signing.json') -RequireTrustedSignature:$RequireTrustedSignature
}
if (!('NxpCatalogVerification' -as [type])) {
    # Host .NET compilation does not use the target C/C++ LIB search path.
    # WDK ARM64 environments can contain a nonexistent NETFXSDK arm64 folder.
    $savedLib = $env:LIB
    try {
        $env:LIB = $null
        Add-Type -Path "$PSScriptRoot\CatalogVerification.cs"
    } finally { $env:LIB = $savedLib }
}
$files = @()
foreach ($name in $expected | Where-Object { $_ -notlike '*.cat' }) {
    $path = Join-Path $package $name
    $member = [NxpCatalogVerification]::VerifyMember($cat,$path)
    $run = Invoke-CapturedTool $SignTool @('verify','/v','/pa','/c',$cat,$path)
    $verify = $run.Output
    $verifyExit = $run.ExitCode
    $verify | Set-Content -LiteralPath (Join-Path $logs "coverage-$name.log")
    if ($TestSign -eq 'true' -and $verifyExit -and $verify -notmatch '0x800B0109|not trusted by the trust provider') {
        throw "SignTool catalog coverage verification failed for $name"
    }
    $files += [ordered]@{file=$name; size=(Get-Item $path).Length; sha256=(Get-FileHash $path -Algorithm SHA256).Hash; catalog_member_sha256=$member; signtool_verify_exit=$verifyExit}
}
if (@(Get-ChildItem -LiteralPath $package -File).Count -ne 5) { throw 'Package must contain exactly five installation files' }
[ordered]@{platform=$Platform; configuration=$Configuration; files=$files; catalog=[IO.Path]::GetFileName($cat); catalog_sha256=(Get-FileHash $cat -Algorithm SHA256).Hash; test_signed=($TestSign -eq 'true'); inf_base_and_universal_valid=$true; inf_windows_isolation_eligible=(@($infResults | Where-Object exit_code -ne 0).Count -eq 0); hardware_tested=$false} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $logs 'package-integrity.json')
Write-Host "Package assembled and file membership verified: $package"
