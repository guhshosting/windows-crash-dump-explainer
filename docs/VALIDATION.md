# Validation record: v1.0.0

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](../RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


Executed on 2026-10-02 in a Windows Server 2025 Standard Evaluation x64 environment,
Windows version 10.0, build 26100, revision 32230 (registry-reported host values).
This is a virtual/hosted environment, not physical Windows 10 or Windows 11 qualification.
MSVC 19.51.36260 / toolset 14.51.36231, Windows SDK 10.0.26100.0;
LLVM clang-tidy 23.1.2 and Cppcheck 2.22.0.

## Executed checks

- Release x64 C++17 with `/W4 /WX /permissive- /sdl /guard:cf`: passed.
- Release CTest: 4/4 passed. Parser test: 21,153 checks, including every byte-prefix
  truncation of the complete fixture and 20,000 reproducibly seeded binary mutations.
- CLI/schema test: 94 checks passed using independent PowerShell ConvertFrom-Json and
  Test-Json Draft 7 schema validation, including a deliberately invalid schema control.
- MSVC `/analyze`, all production code and primary parser tests: no diagnostics;
  separate analyzed build's CTest 4/4 passed.
- clang-tidy: clang-analyzer-*, bugprone-*, performance-* with diagnostics treated as
  errors, production sources and primary parser test: no unsuppressed user-code findings.
  SDK/STL header warnings remain excluded by the normal system-header filter. Two
  narrow, documented test-only NOLINTs cover intentional fixed seeding and a positional
  binary-fixture writer. No production NOLINTs. Exception reporting in test catch blocks
  uses C stdio to prevent allocation/locale exceptions escaping the error handler.
- Cppcheck warning/performance/portability checks with C++17 and win64 platform:
  no findings in production, tests, optional SDK fixture, or optional fuzz harness.
- MSVC AddressSanitizer RelWithDebInfo: CTest 4/4 passed; parser 36.33 s, CLI/schema
  42.21 s, total 78.95 s. No sanitizer findings. ASan binaries are validation-only and
  are not packaged; they require the toolchain's runtime DLL on PATH. MSVC ASan does
  not provide UBSan, MemorySanitizer, or a proof against every integer/logical error.
  No long-running coverage-guided fuzz campaign was performed. A libFuzzer entry point
  is supplied separately; its compilation/long-running qualification remains optional.

Logs are retained in artifacts/release-build.txt, msvc-analyze.txt, clang-tidy.txt,
cppcheck.txt, asan.txt and build*/Testing/Temporary/LastTest.log.

## Fixture and safety validation

Synthetic fixtures exercise minimal dumps, exceptions and common code descriptions,
loaded/unloaded modules, versions, exclusive-end mapping, overlaps, extended threads,
process validity bits, memory lists/Memory64/MemoryInfo, comments, valid/nonvalid UTF-16,
unknown streams and architectures, unsupported signatures/format versions, short known
streams, corrupt directory/stream/name/context/stack/debug/backing-store/memory RVAs,
oversized 32/64-bit counts, zero stride, address overflow, and aggregate string budgets.

CLI tests cover all exit-code classes, repeatable JSON, selectors, Unicode/space paths,
invalid/duplicate arguments, locked input, junction rejection, UNC/device/alternate-stream
rejection before file reads, and SHA-256 equality of the analyzed input before/after.
No persistent machine policy, WER setting, or registry value was changed.

An optional test-only MiniDumpWriteDump helper created a normal snapshot of itself with
explicit synthetic exception metadata: **no crash was triggered**, and no other process
was attached. The helper uses CREATE_NEW, so it cannot overwrite its output. The SDK
fixture was stored only in a GUID-named artifacts/sdk-fixture-* directory. The released
reader decoded 18 modules and 4 threads, system x64/build 26100, access violation data,
and one loaded-module match for its exception address. Independent JSON/schema and
unchanged-fixture SHA-256 checks passed. Exit 1 correctly reported unhandled streams
13, 21 and 22. This tests compatibility with the Windows SDK writer; it is not a
captured real-world crash or evidence of debugger-quality root-cause analysis.
Evidence: artifacts/sdk-validation.txt and its referenced report.json.

## Parser robustness and source/import audit

Reviewed all byte-reader call sites, list prefix/stride/count validation, nested location
RVA validation, offset/address additions, memory totals, string decoding and aggregate
budgets. The parser avoids untrusted struct casts and unchecked count multiplications.
Unknown payloads are not decoded. Malformed known metadata clears the entire report.
Exception-address matching explicitly avoids attribution of blame.

Production-source scan found only CreateFileW with OPEN_EXISTING and read/attribute
access, ReadFile, handle metadata queries, and stdout/stderr output. No networking,
registry, shell/process launch, process attachment, crash generation, dump writers,
file-delete/move, symbol APIs, or embedded-path opens are present in src/.

PE inspection: x64 image; high-entropy VA, dynamic base (ASLR), NX (DEP), and Control
Flow Guard flags. The only direct DLL import is KERNEL32.dll; no DbgHelp, WinHTTP,
WinINet, Winsock, Advapi32, or external C++ runtime DLL dependency. Static CRT imports
include WriteFile/WriteConsoleW for output and LoadLibraryExW/GetProcAddress for runtime
support, plus exception/termination/environment functions. Their presence alone is
not evidence of dump mutation or dump-directory DLL loading. Installed UCRT source
internal/winapi_thunks.cpp uses LOAD_LIBRARY_SEARCH_SYSTEM32 for runtime libraries on
supported Windows; no application LoadLibrary calls exist. The executable runs with
normal user ACLs; no privileged helper or service is installed.
Evidence: artifacts/binary-inspection.txt and artifacts/source-audit.txt.

## Packaging and remaining qualification

scripts/package.ps1 installs only the release executable, README, LICENSE, and docs,
creates the Windows x64 v1.0.0 ZIP without overwriting an existing ZIP, writes SHA-256,
extracts the exact archive to a fresh GUID-named directory, verifies extracted/source
file hashes, runs all 94 CLI/schema checks using the extracted executable and packaged
schema, and analyzes the existing SDK fixture using the extracted binary. Its final
result is recorded in artifacts/package-validation.txt. No test helpers, fixtures,
ASan runtime, development dependencies, or source binaries are included in the ZIP.

Remaining physical qualification: Windows 11 x64 and Windows 10 x64 local-disk runs,
standard non-admin account, localized/UTF-8 terminals, actual available application
crash dumps, endpoint-security interactions, and NTFS/ReFS/removable-drive scenarios.
Qualify supported Windows Server releases beyond this Server 2025 environment as
applicable; native ARM64/emulation is not qualified. Extended live/network traces,
Authenticode signing, and large/full/kernel dump handling are not claimed. A before/after
hash plus source/import inspection is not a complete kernel-level activity trace.

Known limitations and filesystem trust boundaries are detailed in README.md,
SECURITY.md and JSON.md. Runtime has no symbol support, stacks, CPU-context interpretation,
memory-content analysis, or root-cause diagnosis. Standard MDMP files only; 256 MiB cap.
