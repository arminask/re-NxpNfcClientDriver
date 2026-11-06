"""Reproduce file provenance from pinned official upstream URLs (Python 3).

Writes documentation/patches only. Not a build prerequisite. Never rewrites
production source, firmware, or upstream metadata. Raw downloads stay ignored.
"""
import base64
import concurrent.futures
import csv
import difflib
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SHA = lambda data: hashlib.sha256(data).hexdigest()

def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "NxpNfcStandalone-ProvenanceAudit"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()

def normalize(data):
    return data.replace(b"\r\n", b"\n")

def main():
    jobs = []
    for component in ["microsoft-nfc", "aosp-nxp-download"]:
        manifest = json.loads((ROOT / "third_party" / component / "UPSTREAM.json").read_text())
        for entry in manifest["files"]:
            local = ROOT / "third_party" / component / entry["path"]
            if not local.exists():
                continue
            revision, upstream_path = entry["revision"], entry["upstream_path"]
            if component == "microsoft-nfc":
                url = f"https://raw.githubusercontent.com/microsoft/NFC-Class-Extension-Driver/{revision}/{upstream_path}"
            else:
                url = f"https://android.googlesource.com/platform/external/libnfc-nci/+/{revision}/{upstream_path}?format=TEXT"
            jobs.append((component, entry, local, url))
    # Fetch every distinct upstream file once; extracted fragments share an origin.
    urls = sorted({j[3] for j in jobs})
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        payloads = dict(zip(urls, pool.map(fetch, urls)))
    rows = []
    for component, entry, local, url in jobs:
        upstream = payloads[url]
        if component == "aosp-nxp-download":
            upstream = base64.b64decode(upstream)
            assert SHA(upstream) == entry["upstream_sha256"], f"Upstream hash mismatch: {url}"
        current = local.read_bytes()
        expected = entry.get("local_sha256", entry.get("sha256"))
        # Reconstruction manifests hash Windows CRLF materializations, while
        # authoritative Git HEAD blobs may use LF. Preserve HEAD bytes and
        # validate the documented newline representation without editing them.
        assert expected in {SHA(current), SHA(normalize(current).replace(b"\n", b"\r\n"))}, f"Included source differs from HEAD manifest: {local}"
        raw = ROOT / ".work/upstream" / component / entry["revision"] / entry["upstream_path"]
        raw.parent.mkdir(parents=True, exist_ok=True)
        raw.write_bytes(upstream)
        same = current == upstream
        normalized_same = normalize(current) == normalize(upstream)
        patch_name = ""
        if not normalized_same:
            patch = ROOT / "docs/third-party-patches" / component / (entry["path"] + ".patch")
            patch.parent.mkdir(parents=True, exist_ok=True)
            diff = difflib.unified_diff(
                normalize(upstream).decode("utf-8-sig").splitlines(keepends=True),
                normalize(current).decode("utf-8-sig").splitlines(keepends=True),
                fromfile="upstream/" + entry["upstream_path"],
                tofile="included/third_party/" + component + "/" + entry["path"],
            )
            patch.write_text("".join(diff), encoding="utf-8", newline="\n")
            patch_name = patch.relative_to(ROOT).as_posix()
        changes = entry.get("local_changes", [])
        if entry.get("local_hardening"):
            changes = [*changes, entry["local_hardening"]]
        rows.append({
            "file": local.relative_to(ROOT).as_posix(),
            "component": component,
            "upstream_revision": entry["revision"],
            "upstream_path": entry["upstream_path"],
            "upstream_url": url,
            "upstream_sha256": SHA(upstream),
            "included_sha256": SHA(current),
            "comparison": "EXACT_BYTES" if same else "LINE_ENDINGS_ONLY" if normalized_same else "ADAPTED_OR_EXTRACTED",
            "license": "MIT (published Microsoft repository; retain individual NXP notices)" if component == "microsoft-nfc" else "Apache-2.0 (NXP notices)",
            "patch": patch_name,
            "recorded_modifications": " ".join(changes),
        })
    # Reconstruction-authored diagnostics shim has no published upstream identity.
    shim = ROOT / "third_party/aosp-nxp-download/shim/phNxpLog.h"
    rows.append(dict.fromkeys(rows[0], ""))
    rows[-1].update(file=shim.relative_to(ROOT).as_posix(), component="reconstruction integration shim",
                    included_sha256=SHA(shim.read_bytes()), comparison="UNVERIFIED_UPSTREAM_ORIGIN",
                    license="No separate upstream grant; independently generated reconstruction bridge",
                    recorded_modifications="Three Android diagnostic macros are disabled; no transport or business logic. Present in reconstruction HEAD; no published source identity claimed.")
    for name in ["third_party/aosp-nxp-download/NOTICE", "third_party/aosp-nxp-download/UPSTREAM.json", "third_party/microsoft-nfc/UPSTREAM.json"]:
        metadata = ROOT / name
        row = dict.fromkeys(rows[0], "")
        row.update(file=name, component="reconstruction attribution/provenance metadata",
                   included_sha256=SHA(metadata.read_bytes()), comparison="RECONSTRUCTION_METADATA",
                   license="Attribution/metadata only; underlying licenses remain separately identified",
                   recorded_modifications="Reconstruction attribution/provenance metadata, including recorded local safety adaptations. Underlying source licenses remain separate.")
        rows.append(row)
    output = ROOT / "docs/THIRD_PARTY_FILES.csv"
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (ROOT / "docs/THIRD_PARTY_AUDIT.json").write_text(json.dumps({
        "method": "HTTPS download from official pinned file URLs, SHA256, normalized unified diffs",
        "files": len(rows), "unique_upstream_downloads": len(urls),
        "exact_bytes": sum(r["comparison"] == "EXACT_BYTES" for r in rows),
        "line_endings_only": sum(r["comparison"] == "LINE_ENDINGS_ONLY" for r in rows),
        "adapted_or_extracted": sum(r["comparison"] == "ADAPTED_OR_EXTRACTED" for r in rows),
        "unverified_shims": 1,
        "reconstruction_metadata_files": 3,
        "limits": "Verifies source origins and current modifications; does not independently re-establish binary equivalence. Original UPSTREAM.json evidence paths refer to reconstruction history."
    }, indent=2) + "\n")
    print(f"Verified {len(jobs)} included upstream files; documented one local shim and three attribution/metadata files")

if __name__ == "__main__":
    main()
