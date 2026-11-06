param(
    [string]$File,
    [string]$CertificateThumbprint,
    [switch]$CreateCertificate,
    [string]$Report,
    [switch]$RequireTrustedSignature
)
. "$PSScriptRoot\common.ps1"
$cert = Get-DevelopmentCertificate $CertificateThumbprint -CreateIfMissing:$CreateCertificate
if (!$File) {
    if (!$CreateCertificate) { throw 'Provide -File or -CreateCertificate' }
    Write-Output "Certificate: $($cert.Subject); thumbprint: $($cert.Thumbprint)"
    return
}
Invoke-CheckedTool $SignTool @('sign','/v','/fd','SHA256','/s','My','/sha1',$cert.Thumbprint,$File)
$run = Invoke-CapturedTool $SignTool @('verify','/v','/pa','/sha1',$cert.Thumbprint,$File)
$verification = $run.Output
$verifyExit = $run.ExitCode
if ($verifyExit -eq 0) { Write-Host $verification }
$signatureValid = $false
if ([IO.Path]::GetExtension($File) -eq '.cat') {
    Add-Type -AssemblyName System.Security
    $cms = New-Object System.Security.Cryptography.Pkcs.SignedCms
    $cms.Decode([IO.File]::ReadAllBytes($File))
    $cms.CheckSignature($true)
    if ($cms.SignerInfos.Count -ne 1 -or $cms.SignerInfos[0].Certificate.Thumbprint -ne $cert.Thumbprint) {
        throw 'Catalog signer does not match the selected certificate'
    }
    $signatureValid = $true
}
$result = [ordered]@{
    certificate_subject=$cert.Subject; certificate_thumbprint=$cert.Thumbprint
    certificate_expires=$cert.NotAfter.ToUniversalTime().ToString('o')
    file=[IO.Path]::GetFileName($File); sha256=(Get-FileHash $File -Algorithm SHA256).Hash
    signature_digest='SHA256'; cms_signature_valid=$signatureValid
    signtool_verify_exit=$verifyExit; signtool_verify_output=$verification
    trusted_authenticode=($verifyExit -eq 0); timestamped=$false
}
if ($Report) { $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report }
if ($verifyExit) {
    # An untrusted self-signed certificate is expected without installing trust.
    # All other signature failures remain fatal. Cryptographic verification of
    # the catalog and explicit signer matching above must still have succeeded.
    if (!$signatureValid -or $verification -notmatch '0x800B0109|not trusted by the trust provider') {
        throw "SignTool signature verification failed ($verifyExit)"
    }
    Write-Warning 'Cryptographic signature verified; SignTool trust-policy verification FAILED: untrusted root. No trust store was changed.'
    if ($RequireTrustedSignature) { throw 'A trusted signature was required' }
}
