$ErrorActionPreference = 'Stop'
$DriverRoot = Split-Path -Parent $PSScriptRoot
$KitVersion = '10.0.26100.0'
$kits = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots'
$KitRoot = $kits.KitsRoot10
if (!$KitRoot) { throw 'Windows SDK/WDK 26100 is required' }
$SignTool = Join-Path $KitRoot "bin\$KitVersion\x64\signtool.exe"
$Inf2Cat = Join-Path $KitRoot "bin\$KitVersion\x86\Inf2Cat.exe"
$InfVerif = Join-Path $KitRoot "Tools\$KitVersion\x64\InfVerif.exe"
function Invoke-CheckedTool([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE) { throw "$Tool exited $LASTEXITCODE" }
}
function Invoke-CapturedTool([string]$Tool, [string[]]$Arguments) {
    # Windows PowerShell turns native stderr into ErrorRecord objects. Capture
    # diagnostics and exit status before deciding whether a failure is expected.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $Tool @Arguments 2>&1 | Out-String
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    return [pscustomobject]@{ Output=$output; ExitCode=$code }
}
function Get-DevelopmentCertificate([string]$Thumbprint, [switch]$CreateIfMissing) {
    if (!$Thumbprint) { $Thumbprint = $env:NXP_SIGNING_CERTIFICATE }
    if ($Thumbprint) {
        $cert = Get-Item -LiteralPath "Cert:\CurrentUser\My\$($Thumbprint.Replace(' ',''))"
    } else {
        $candidates = @(Get-ChildItem Cert:\CurrentUser\My | Where-Object {
            $_.Subject -match 'WDKTestCert|NxpNfcClientDriver Development' -and
            $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) -and
            ($_.EnhancedKeyUsageList.ObjectId -contains '1.3.6.1.5.5.7.3.3')
        } | Sort-Object NotAfter -Descending)
        if ($candidates.Count) { $cert = $candidates[0] }
        elseif ($CreateIfMissing) {
            $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=NxpNfcClientDriver Development' `
                -CertStoreLocation Cert:\CurrentUser\My -KeyExportPolicy NonExportable `
                -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(2)
        } else { throw 'No development certificate found. Pass -CertificateThumbprint or use sign.ps1 -CreateCertificate.' }
    }
    if (!$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date) -or $cert.NotBefore -gt (Get-Date) -or
        !($cert.EnhancedKeyUsageList.ObjectId -contains '1.3.6.1.5.5.7.3.3')) {
        throw 'Select a currently valid code-signing certificate with a private key in CurrentUser\My'
    }
    return $cert
}
