# windows-crash-dump-explainer 1.0.0

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


A Windows command-line tool for offline triage of existing standard Windows minidumps.
It explains recorded metadata without WinDbg or symbols. MIT licensed; C++17 and CMake.

```
windows-crash-dump-explainer crash.dmp
windows-crash-dump-explainer crash.dmp --summary
windows-crash-dump-explainer crash.dmp --modules
windows-crash-dump-explainer crash.dmp --threads
windows-crash-dump-explainer crash.dmp --exception
windows-crash-dump-explainer crash.dmp --json
windows-crash-dump-explainer --help
windows-crash-dump-explainer --version
```

Quote paths containing spaces. One text selector is allowed. `--json` always includes
all metadata, even with a selector. `--` ends option processing for filenames starting
with a dash. Output is UTF-8 on stdout, with no BOM. Embedded strings are quoted with
control characters escaped. A UTF-8 terminal is recommended. All diagnostics from
analysis are included in text or JSON; output-write failures also use stderr.

The executable never creates output files. Shell redirection is controlled by the
shell and may overwrite files; redirect only to a new report path, never the dump.

## Interpreting results

**Exception address falls within module X** means the recorded instruction address
is inside X's recorded image range. It does not establish that X caused the crash.
Earlier memory corruption, bad inputs, other components, dynamic code, or incomplete
metadata can affect the location. Module versions and embedded paths are writer
claims, not verified properties of files currently installed on the machine.

An exception stream does not prove an unhandled fatal crash: snapshot writers can
supply exception data. Missing exception data is reported without inventing a cause.
Full-memory flags describe writer intent and do not guarantee every page is present.
Process/thread IDs and OS version/build values refer to the dump, not the host.

## Features and supported files

Supports MDMP-format user-mode minidumps with low format-version word `0xa793`.
Decodes system info, exception info, thread and extended-thread lists, loaded and
unloaded module lists, base miscellaneous/process information, memory lists,
Memory64 lists, memory-region metadata totals, and ANSI/UTF-16 comments. The header's
flags, timestamp, checksum, format version, and all stream directory entries are
reported. Module file/product versions are read from VS_FIXEDFILEINFO when its
signature is valid. Memory bytes and register contexts are bounded but not decoded.

Common exception codes include access violation (operation and referenced address),
illegal instruction, stack overflow, integer divide by zero, breakpoint, single step,
heap corruption, MSVC C++ exceptions, in-page error, privileged instruction, and
fail-fast. `0xc0000409` can represent several fast-fail conditions: the name does
not prove a buffer overrun. Its first parameter is reported as a raw fast-fail subcode.

Other streams are bounded and reported as unhandled, producing partial triage.
Unknown architectures remain readable as metadata, with a warning. No stack
unwinding, symbols, executable loading, memory scanning, or root-cause diagnosis.
Kernel PAGE/DUMP-format complete/kernel/automatic/active-memory dumps are unsupported,
as are compressed/encrypted dumps, invalid format versions, and files over 256 MiB.
Even valid full-memory user dumps may exceed this resource limit. Empty lists are
allowed. Duplicate stream types (except unused type 0) and duplicate IDs within a
thread list are rejected rather than choosing an ambiguous record. Future list-entry
sizes are accepted if at least the known prefix fits; fields beyond it are ignored.
Miscellaneous information extensions beyond the base 24 bytes are not decoded.

## Offline and read-only operation

No intentional network access, telemetry, symbol downloading, or symbol resolution.
There is no symbol-path option in v1.0.0. Embedded paths are only displayed, never
opened. The executable contains no DbgHelp dependency. It never attaches to a process,
triggers a crash, configures WER, changes the registry, changes dump policies, executes
dump content, or modifies/deletes/moves/renames the analyzed file.

Use a regular file on a local drive. UNC paths, device prefixes, alternate data streams,
remote/unknown drives, and reparse points (including junctions and cloud placeholders)
are rejected. Copy a dump to a normal local directory yourself if necessary. Parent
directories are opened without write/delete sharing during inspection, and the input
is opened with GENERIC_READ and FILE_SHARE_READ only. Active writers or locked parent
directories may therefore cause exit 3. No elevation is required; normal ACLs apply.
See [security model](docs/SECURITY.md) for the trust boundary and resource limits.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Complete supported triage: system, exception, loaded modules, and thread list are present; no parser warnings. This is not complete forensic diagnosis. |
| 1 | Successful partial triage: a core stream is absent, architecture is unknown/uncommon, an unhandled stream exists, or metadata is inconsistent/ambiguous. |
| 2 | Malformed, unsupported, or resource-limited dump. No partially trusted metadata is emitted. |
| 3 | File/path/access/read error, or stdout write failure. |
| 4 | Invalid command-line usage. |

Optional absent process, unloaded-module, memory, or symbol data does not alone change
0 to 1; it remains explicitly listed in unavailable data. Help/version return 0.
JSON includes the same code. A stdout failure may prevent delivery of a complete JSON
report, so the actual process exit code is authoritative.

## JSON and builds

See [schema guide](docs/JSON.md) and [JSON Schema](docs/schema.json).
Key order, list order, hexadecimal formatting, and newline termination are deterministic
for the same bytes and tool version. No host metadata, input path, or current time is
inserted into successful reports.

Build with a Windows x64 MSVC developer environment, CMake 3.20+ and Ninja:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The provided `scripts/build.cmd` locates this validation machine's VS 18 BuildTools.
Adjust that path for another installation. `/W4 /WX`, `/sdl`, `/guard:cf`, ASLR, DEP,
and the static MSVC runtime are enabled. No external dependencies or package download
steps are needed. Validation scripts cover MSVC `/analyze`, clang-tidy, Cppcheck,
AddressSanitizer, CLI/schema checks, and packaging. Tests generate only synthetic
fixtures inside their designated build/test directory; ordinary tests cause no crash.

## Compatibility and qualification

The x64 executable targets Windows 10 and Windows 11 x64, and Windows Server x64
releases still supported by Microsoft where these filesystem APIs apply. ARM64 Windows
x64-emulation operation and native ARM64 builds are not qualified. It can inspect x86,
x64, and ARM64 dump metadata regardless of target architecture. SDK minimum is Windows 10.

Support targets are distinct from executed qualification. See
[validation record](docs/VALIDATION.md) for the actual host, tests, and remaining physical
Windows 10/Windows 11 and Server qualification. The release is unsigned. Extract into
a trusted local directory; do not place unrelated DLLs alongside the executable.
