# Reverse-engineered NXP NFC Client Driver

An **unofficial, decompiled and reverse-engineered reconstruction** of the NXP
Windows NFC client driver, based on the original x64 driver binary and its
associated PDB debugging symbols. The repository includes later source fixes
and an ARM64 source port.

This is reconstructed source, not an original NXP source-code release or a
clean-room implementation. It is not affiliated with, endorsed by, or supported
by NXP, Acer, Microsoft, or DriverGuide.

## Original binary and PDB source

The reference driver and PDB was obtained from this
[DriverGuide listing](https://www.driverguide.com/driver/detail.php?driverid=2122251).
The listing identifies the Acer TravelMate P214-51 package
`NFC_NXP_12.0.4.0_W10x64_A.zip`, version **12.0.4.0**. Its file list includes:

- `NFC_NXP_12.0.4.0_W10x64/NxpNfcClientDriver.dll`
- `NFC_NXP_12.0.4.0_W10x64/Driver_symbol/NxpNfcClientDriver.pdb`

The DLL was analyzed to recover behavior, while the PDB supplied symbols,
original compilation-unit names, types and layout information. Published
third-party source was reused where identified, rather than presenting all
recovered functions as newly authored code.

For reproducibility, the local reference inputs have these SHA-256 hashes:

| Reference input | SHA-256 |
|---|---|
| x64 driver DLL | `247ff64b48e59b31dae43d3ce5bd2523d8b007e72aa5ad3119664531d6f5fbfc` |
| PDB | `7eeba912edeeb9d42da1831360fd4c25ca8a2250d47bbbf53db69d058b3c767d` |

## Project scope and status

- Windows **UMDF 2 / NfcCx** client driver using Windows Resource Hub/SPB I2C.
- x64 and ARM64 project configurations, each with Debug and Release variants.
- Model-specific code paths for PN547, PN548, PN553 and PN557; this is not a claim
  of validated support for every controller revision or board.
- The ARM64 build is a source port, not a reconstruction of an original ARM64
  NXP binary. Hardware operation remain unverified.

The code differs intentionally from the reference binary because of hardening
and portability work.

## Building

Use Windows, Visual Studio with C++ x64/ARM64 tools, and Windows SDK/WDK
**10.0.26100.0** with the WDK integration installed. From a developer shell:

```powershell
msbuild NxpNfcClientDriver.vcxproj /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild NxpNfcClientDriver.vcxproj /t:Rebuild /p:Configuration=Debug /p:Platform=ARM64
```

Use `Configuration=Release` for Release builds. Outputs are written under
`out/`.
