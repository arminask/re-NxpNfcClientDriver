param(
    [ValidateSet('x64','ARM64')][string]$Platform = 'x64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)
. "$PSScriptRoot\common.ps1"
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$version = (Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')).Trim()
$dumpbin = Join-Path $vs "VC\Tools\MSVC\$version\bin\Hostx64\x64\dumpbin.exe"
$output = Join-Path $DriverRoot "out\$Platform\$Configuration"
$dll = Join-Path $output 'bin\NxpNfcClientDriver.dll'
$logs = Join-Path $output 'logs'
$bytes = [IO.File]::ReadAllBytes($dll)
$pe = [BitConverter]::ToInt32($bytes,0x3c)
$machine = [BitConverter]::ToUInt16($bytes,$pe+4)
$expectedMachine = if ($Platform -eq 'x64') { 0x8664 } else { 0xaa64 }
if ($machine -ne $expectedMachine -or [BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x20b -or
    !([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000) -or
    [BitConverter]::ToUInt16($bytes,$pe+24+68) -ne 2) { throw 'Expected native PE32+ Windows UMDF DLL for selected architecture' }
$dump = Invoke-CapturedTool $dumpbin @('/headers','/imports','/exports',$dll)
if ($dump.ExitCode) { throw 'Dumpbin failed' }
$dump.Output | Set-Content -LiteralPath (Join-Path $logs 'PE.txt')
$exports = [regex]::Matches($dump.Output,'(?m)^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)(?:\s+=\s+\S+)?\s*$')
if ($exports.Count -ne 1 -or $exports[0].Groups[1].Value -ne 'FxDriverEntryUm') { throw 'Unexpected export table' }
if ($dump.Output -match '(?i)(VCRUNTIME\d+|MSVCP\d+|ucrtbased)\.dll') { throw 'Unexpected external MSVC runtime dependency' }
$linkLog = Get-ChildItem -LiteralPath (Join-Path $output 'obj\NxpNfcClientDriver') -Recurse -Filter link.command.1.tlog
$link = Get-Content -LiteralPath $linkLog.FullName -Raw
foreach ($lib in @('NfcCxStub.lib','WdfDriverStubUm.lib','OneCoreUap.lib','NfcCoreVerified.lib')) {
    if ($link -notmatch [regex]::Escape($lib)) { throw "Missing required link dependency: $lib" }
}
if ($link -match '/FORCE|ReconstructionTests|HostLifecycleTests') { throw 'Invalid driver link input' }
$countC=0; $countCpp=0
foreach ($project in @('NxpNfcClientDriver','NfcCoreVerified')) {
    [xml]$xml = Get-Content -LiteralPath (Join-Path $DriverRoot "$project.vcxproj")
    $clLog = Get-ChildItem -LiteralPath (Join-Path $output "obj\$project") -Recurse -Filter CL.command.1.tlog
    $commands = Get-Content -LiteralPath $clLog.FullName -Raw
    foreach ($unit in $xml.Project.ItemGroup.ClCompile | Where-Object Include) {
        if ($unit.Include -like '*.c') {
            if ($project -ne 'NfcCoreVerified' -and $unit.CompileAs -ne 'CompileAsC') { throw "C language boundary changed: $($unit.Include)" }
            $countC++
        } else { $countCpp++ }
    }
    if ($commands -notmatch '/Zp8' -or $commands -notmatch '/TC') { throw 'Missing packing or C compilation setting' }
    if ($Platform -eq 'ARM64' -and $commands -notmatch '/volatile:ms') { throw 'Missing ARM64 volatile ordering option' }
    $crt = if ($Configuration -eq 'Debug') { '/MTd' } else { '/MT ' }
    if ($commands -notmatch [regex]::Escape($crt)) { throw 'Unexpected CRT selection' }
}
if ($countC -ne 21 -or $countCpp -ne 15) { throw 'Production compilation-unit set changed' }
$allowedRoots = @(($DriverRoot.TrimEnd('\')+'\'), ($KitRoot.TrimEnd('\')+'\'), ($vs.TrimEnd('\')+'\'), ($env:windir.TrimEnd('\')+'\'))
$dependencyCount=0
foreach ($readLog in Get-ChildItem -LiteralPath (Join-Path $output 'obj') -Recurse -Filter '*.read.*.tlog') {
    foreach ($path in Get-Content -LiteralPath $readLog.FullName | Where-Object { $_ -match '^[A-Za-z]:\\' }) {
        $permitted = $false
        foreach ($prefix in $allowedRoots) { if ($path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { $permitted=$true; break } }
        if (!$permitted) { throw "Build read an input outside the standalone tree or installed platform tools: $path" }
        $dependencyCount++
    }
}
if (!$dependencyCount) { throw 'No tracked build input records were found' }
$api = Join-Path $KitRoot "bin\$KitVersion\x64\apivalidator.exe"
$apiXml = Join-Path $KitRoot "build\$KitVersion\universalDDIs\$($Platform.ToLower())\UniversalDDIs.xml"
$apiRun = Invoke-CapturedTool $api @("-BinaryPath:$dll","-SupportedApiXmlFiles:$apiXml",'-StrictCompliance:true')
$apiRun.Output | Set-Content -LiteralPath (Join-Path $logs 'ApiValidator.txt')
if ($apiRun.ExitCode) { throw 'Strict WDK API validation failed' }
[ordered]@{ platform=$Platform; configuration=$Configuration; machine=('0x{0:x4}' -f $machine); pe32_plus=$true;
    subsystem='Windows'; exports=@('FxDriverEntryUm'); source_units_C=$countC; source_units_Cpp=$countCpp;
    umdf='2.15'; nfc_extension='NfcCx0102'; api_validator_exit=$apiRun.ExitCode;
    tracked_input_observations=$dependencyCount; standalone_inputs_only=$true;
    sha256=(Get-FileHash $dll -Algorithm SHA256).Hash; runtime_or_hardware_execution=$false } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logs 'binary-validation.json')
Write-Host "Verified $Platform $Configuration PE, production language boundaries, link contracts and strict API eligibility"
