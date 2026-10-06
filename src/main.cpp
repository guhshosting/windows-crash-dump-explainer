#include "parser.hpp"
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace {
const char* help =
"windows-crash-dump-explainer 1.0.0\n"
"Usage: windows-crash-dump-explainer <dump.dmp> [--summary|--modules|--threads|--exception] [--json]\n"
"       windows-crash-dump-explainer --help | --version\n"
"Offline, read-only minidump metadata triage. No symbols, stack unwinding, or root-cause diagnosis.\n"
"JSON contains all parsed metadata; text selectors affect display only. Output goes to stdout.\n"
"Exit codes: 0 complete triage, 1 partial triage, 2 malformed/unsupported/resource limit,\n"
"            3 file/access/output error, 4 invalid usage. Max file size: 256 MiB.\n"
"Local filesystem paths only. UNC/device/reparse paths and alternate data streams are rejected.\n";
struct HandleCloser { void operator()(void* h) const noexcept { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
using Handle = std::unique_ptr<void, HandleCloser>;
bool local_path(const std::wstring& path) {
 if (path.empty()) return false;
 std::wstring p = path; std::replace(p.begin(), p.end(), L'/', L'\\');
 if (p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\') return false;
 for (std::size_t i = 0; i < p.size(); ++i) if (p[i] == L':' && !(i == 1 && ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')))) return false;
 return true;
}
wcde::Report failure(int code, const std::string& message) { wcde::Report r; r.exit_code = code; r.error = message; return r; }
wcde::Report read_dump(const std::wstring& path) {
 if (!local_path(path)) return failure(3, "Only local filesystem paths without device prefixes or alternate streams are accepted");
 const DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
 if (!required || required > 32768) return failure(3, "Cannot resolve local path");
 std::vector<wchar_t> absolute(static_cast<std::size_t>(required) + 1);
 const DWORD length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(absolute.size()), absolute.data(), nullptr);
 if (!length || length >= absolute.size()) return failure(3, "Cannot resolve local path");
 const std::wstring full(absolute.data());
 if (full.size() < 3 || full[1] != L':' || full[2] != L'\\') return failure(3, "Input must resolve to a local drive path");
 const auto drive = GetDriveTypeW(full.substr(0, 3).c_str());
 if (drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_CDROM && drive != DRIVE_RAMDISK) return failure(3, "Remote or unknown volumes are not supported");
 // Pin all parent directories while checking them. Deny write/delete sharing to
 // prevent namespace replacement or reparse changes between checking and reading.
 std::vector<Handle> parents;
 for (std::size_t i = 3; i < full.size(); ++i) {
  if (i != 3 && full[i] != L'\\') continue;
  const auto component = full.substr(0, i);
  Handle parent(CreateFileW(component.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  BY_HANDLE_FILE_INFORMATION pi{};
  if (parent.get() == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(parent.get(), &pi)) return failure(3, "Cannot inspect or lock local directory component");
  if (!(pi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (pi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return failure(3, "Reparse or non-directory path component is rejected");
  parents.push_back(std::move(parent));
 }
 Handle handle(CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
 if (handle.get() == INVALID_HANDLE_VALUE) return failure(3, "Cannot open dump for read (Win32 error " + std::to_string(GetLastError()) + ')');
 BY_HANDLE_FILE_INFORMATION info{};
 if (!GetFileInformationByHandle(handle.get(), &info) || (info.dwFileAttributes & (static_cast<DWORD>(FILE_ATTRIBUTE_REPARSE_POINT) | static_cast<DWORD>(FILE_ATTRIBUTE_DIRECTORY))) || GetFileType(handle.get()) != FILE_TYPE_DISK) return failure(3, "Input is not a regular local file");
 LARGE_INTEGER size{};
 if (!GetFileSizeEx(handle.get(), &size) || size.QuadPart < 0) return failure(3, "Cannot obtain dump size");
 if (static_cast<std::uint64_t>(size.QuadPart) > wcde::max_file_size) return failure(2, "Dump exceeds 256 MiB resource limit");
 std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart)); std::size_t cursor = 0;
 while (cursor < bytes.size()) {
  const auto request = static_cast<DWORD>((std::min)(bytes.size() - cursor, static_cast<std::size_t>(1024 * 1024))); DWORD received = 0;
  if (!ReadFile(handle.get(), bytes.data() + cursor, request, &received, nullptr) || received == 0) return failure(3, "Read failed or dump changed while being read");
  cursor += received;
 }
 return wcde::parse(bytes);
}
}
int wmain(int argc, wchar_t* argv[]) {
 bool as_json = false; std::string selection; std::wstring path;
 if (argc == 2 && std::wstring(argv[1]) == L"--help") { std::cout << help; return 0; }
 if (argc == 2 && std::wstring(argv[1]) == L"--version") { std::cout << "windows-crash-dump-explainer 1.0.0\n"; return 0; }
 bool usage_error = false, literal = false;
 for (int i = 1; i < argc; ++i) {
  const std::wstring a(argv[i]);
  if (!literal && a == L"--") { literal = true; continue; }
  if (!literal && a == L"--json") { if (as_json) usage_error = true; as_json = true; }
  else if (!literal && (a == L"--summary" || a == L"--modules" || a == L"--threads" || a == L"--exception")) {
   if (!selection.empty()) usage_error = true;
   selection = a == L"--summary" ? "summary" : a == L"--modules" ? "modules" : a == L"--threads" ? "threads" : "exception";
  } else if (!literal && !a.empty() && a[0] == L'-') usage_error = true;
  else { if (!path.empty()) usage_error = true; path = a; }
 }
 if (path.empty()) usage_error = true;
 wcde::Report report;
 try { report = usage_error ? failure(4, "Invalid usage; use --help") : read_dump(path); }
 catch (const std::bad_alloc&) { report = failure(2, "Resource limit: memory allocation failed"); }
 catch (const std::exception&) { report = failure(2, "Analysis could not be completed"); }
 std::cout << (as_json ? wcde::json(report) : wcde::text(report, selection));
 if (!std::cout) { std::cerr << "Output write failed\n"; return 3; }
 return report.exit_code;
}


