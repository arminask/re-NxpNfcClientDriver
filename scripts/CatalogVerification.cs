using System;
using System.IO;
using System.Runtime.InteropServices;

// Read-only Windows catalog membership verification. The native hash calculation
// applies Windows' file-type-specific hashing rules (including PE hashing).
public static class NxpCatalogVerification
{
    [DllImport("wintrust.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool CryptCATAdminAcquireContext2(out IntPtr context, IntPtr subsystem,
        string algorithm, IntPtr strongPolicy, uint flags);
    [DllImport("wintrust.dll", SetLastError=true)]
    static extern bool CryptCATAdminCalcHashFromFileHandle2(IntPtr context,
        IntPtr file, ref uint size, byte[] hash, uint flags);
    [DllImport("wintrust.dll")]
    static extern bool CryptCATAdminReleaseContext(IntPtr context, uint flags);
    [DllImport("wintrust.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CryptCATOpen(string path, uint flags, IntPtr provider,
        uint publicVersion, uint encodingType);
    [DllImport("wintrust.dll")]
    static extern IntPtr CryptCATEnumerateMember(IntPtr catalog, IntPtr previous);
    [DllImport("wintrust.dll")]
    static extern bool CryptCATClose(IntPtr catalog);
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct MemberPrefix { public uint size; public IntPtr referenceTag; public IntPtr fileName; }

    public static string VerifyMember(string catalog, string path)
    {
        IntPtr admin;
        if (!CryptCATAdminAcquireContext2(out admin, IntPtr.Zero, "SHA256", IntPtr.Zero, 0))
            throw new System.ComponentModel.Win32Exception();
        string tag;
        try {
            using (var file = File.OpenRead(path)) {
                uint size=0;
                if (!CryptCATAdminCalcHashFromFileHandle2(admin, file.SafeFileHandle.DangerousGetHandle(), ref size, null, 0))
                    throw new System.ComponentModel.Win32Exception();
                byte[] hash=new byte[size];
                if (!CryptCATAdminCalcHashFromFileHandle2(admin, file.SafeFileHandle.DangerousGetHandle(), ref size, hash, 0))
                    throw new System.ComponentModel.Win32Exception();
                tag=BitConverter.ToString(hash).Replace("-", "");
            }
        } finally { CryptCATAdminReleaseContext(admin, 0); }
        IntPtr cat=CryptCATOpen(catalog, 0, IntPtr.Zero, 0x100, 0);
        if (cat==IntPtr.Zero || cat==new IntPtr(-1)) throw new System.ComponentModel.Win32Exception();
        try {
            IntPtr previous=IntPtr.Zero;
            while ((previous=CryptCATEnumerateMember(cat, previous))!=IntPtr.Zero) {
                var member=(MemberPrefix)Marshal.PtrToStructure(previous, typeof(MemberPrefix));
                if (String.Equals(tag, Marshal.PtrToStringUni(member.referenceTag), StringComparison.OrdinalIgnoreCase)) return tag;
            }
        } finally { CryptCATClose(cat); }
        throw new InvalidDataException("Final file hash is absent from catalog: "+path);
    }
}
