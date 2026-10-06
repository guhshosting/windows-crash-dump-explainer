# JSON schema 1.0.0

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](../RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


`schema.json` is an offline Draft 7 JSON Schema. All references are internal.
The executable emits one UTF-8 object and a final newline, with no BOM.

Top-level keys always exist: schema_version, tool_version, exit_code, status, error,
dump, system, process, exception, exception_location, threads, extended_threads,
modules, unloaded_modules, memory, comments, findings, warnings, unavailable_data.
Absent scalar/object sections use null; absent collections use []. An empty collection
can also be a recorded empty list: inspect dump.streams and unavailable_data to distinguish.
Errors contain no parsed sections. `status` is complete/partial/error and matches the
exit code group 0/1/2..4. Analysis never includes the input filename or host state.

Addresses, exception codes/flags, dump flags, and other bitmasks use fixed-width
`0x` plus 16 lowercase hexadecimal digits. 64-bit memory byte totals use decimal
strings to preserve precision in JavaScript. Other integers (IDs, RVAs, version parts,
counts, captured context lengths, timestamps) are exact JSON integers within safe
IEEE-754 integer range given the documented field widths and resource limits.
Unix timestamps are raw seconds from the writer; no timezone conversion is performed.

The system section records architecture ID/name, processor count, product type,
version/build, platform ID, suite mask, and service pack. `process` base miscellaneous
fields are null unless their validity bits are set. Module paths are untrusted stored
text. Version fields are null when the fixed version signature is absent. Unloaded
modules have no version field because the documented structure provides none.

Exception parameters are raw hex strings in recorded order. memory_operation and
referenced_address are present only for access violation/in-page exceptions with
at least two parameters. fast_fail_subcode appears only for 0xc0000409 with at least
one parameter. exception_location contains all loaded module paths whose ranges include
the address; multiple matches are marked ambiguous. Unloaded ranges are not treated
as current loaded location. No exception yields null exception_location.

`memory` contains totals per decoded stream (5, 9, 16), not captured memory contents.
MemoryInfoList type 16 contains described_bytes; types 5/9 contain captured_bytes.
`comments` preserves stream order, removes one terminal NUL where present, and uses
the deterministic encoding policy in SECURITY.md. Findings are conservative human
explanations; warnings affect completeness. unavailable_data describes absent or
unimplemented capabilities even for a complete supported triage report.

Array order follows directory/record order. Consumers should key on field names,
not serialized key order or English prose. Version 1.0.0 uses no external schema
resolver or runtime JSON dependency. Validation uses PowerShell Test-Json independently.
