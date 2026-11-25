"""Scan built binaries for Windows 8+/10-only API imports that would stop the plugin loading on Windows 7.
Prints a report and a GitHub warning if anything suspicious is found. Never fails the build."""
import pathlib, sys
SUSPECT = [b"WaitOnAddress", b"WakeByAddressSingle", b"WakeByAddressAll", b"api-ms-win-core-synch-l1-2-0",
           b"GetSystemTimePreciseAsFileTime", b"SetThreadDescription", b"GetCurrentPackageFamilyName",
           b"SetProcessMitigationPolicy", b"api-ms-win-core-realtime-l1-1-1"]
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
files = [p for p in root.rglob("*") if p.suffix.lower() in (".vst3", ".exe", ".dll") and p.is_file()]
hits = 0
print(f"Scanning {len(files)} binaries under {root}")
for f in files:
    data = f.read_bytes()
    found = [s.decode() for s in SUSPECT if s in data]
    print(f"  {f.name}: {'NEEDS Windows 8+/10 APIs -> ' + ', '.join(found) if found else 'no Win8+/10-only imports found'}")
    hits += bool(found)
if hits:
    print("::warning::Some binaries import Windows 8+/10 APIs and may not load on Windows 7. See README 'Windows 7/8'.")
else:
    print("OK: no known Windows 8+/10-only imports. (Not a guarantee - test on the real machine.)")
