# Security model

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](../RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


The application treats every dump byte as attacker controlled. Parsing is a pure
function over a bounded byte vector, separate from the Windows reader and CLI.
No binary data is reinterpreted as C++ structs; little-endian integer reads check
their full extent. File and stream offsets use unsigned 64-bit arithmetic. Counted
lists use division against the stream extent before multiplying, preventing count
multiplication overflow. Nested locations are validated against the file, even when
their context/debug/memory contents are otherwise ignored. Addition checks cover
virtual address endpoints, cumulative memory sizes, and string budgets. Module
membership uses subtraction after a lower-bound comparison; the upper bound is exclusive.

Limits: 256 MiB file; 4,096 streams; 16,384 entries per module/thread list; 100,000
memory descriptors per list; 65,536 bytes per string/comment; 8 MiB aggregate embedded
string bytes. List stride and header size must fit the stream; fixed streams must
contain the documented minimum structure. Odd UTF-16 lengths and unpaired surrogates
are rejected. ANSI comments map bytes to U+0000..U+00FF deterministically, because the
dump does not reliably identify its codepage; non-ASCII text may look different from
the creator's locale. UTF-16 comments are decoded as UTF-8. Output escapes C0 controls,
DEL, quotes, and backslashes; embedded terminal escape sequences cannot issue commands.

Unknown stream payloads and unparsed extension bytes are not recursively validated:
only their directory bounds are checked. The checksum is reported, not verified (many
writers leave it zero). Header/list reserved fields and stream alignment/overlaps are
not taken as proof of validity. Memory totals may double-count overlapping regions.
This is a defensive metadata reader, not a forensic integrity validator. A malformed
known stream aborts the report and clears metadata; one corrupt stream cannot silently
lend confidence to another. Allocation failure becomes a resource-limit error where
possible; catastrophic exhaustion during error reporting is not guaranteed recoverable.
Mutation tests and ASan improve confidence but cannot prove absence of bugs.

The executable opens only the requested local regular input file and its local parent
directories. Parent handles deny write/delete sharing and are kept alive through the
read. Reparse points and remote drives are rejected. The final component is opened
with OPEN_REPARSE_POINT and checked by handle. The input denies write/delete sharing;
reading occurs in 1 MiB chunks into one vector. This does not prevent writes through a
preexisting writable memory mapping; use a stable completed dump for reproducible analysis.
No dump path is sent to a shell. Embedded names never influence filesystem access.

The guarantee is no intentional network or system-configuration mutation by the tool.
It is not a sandbox against a malicious administrator, filesystem/filter driver,
hostile DOS-device remapping, a compromised OS loader, or OS activity such as last-access
timestamps, antivirus scans, paging, and execution logging. Mount points, symlinks and
cloud placeholders are deliberately excluded. A trusted local installation directory
is part of the executable/Windows DLL-loader trust boundary. The static C++ runtime
avoids dependence on application-local C++ runtime DLLs. System DLLs are OS dependencies;
no LoadLibrary calls or DbgHelp/symbol engine exist in production sources.

Developer tests/scripts are a separate trust domain. They may write fixtures, builds,
logs, ZIPs, and extracted packages only in dedicated workspace locations. The optional
SDK writer fixture is a test-only self-snapshot generator; it is not linked into the CLI.
Never distribute test helper binaries as the release utility.
