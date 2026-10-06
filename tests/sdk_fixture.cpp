// Optional validation helper: creates a normal self-snapshot, not a live crash.
// Writes only a new file named explicitly on its command line. Never shipped.
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
int wmain(int argc, wchar_t* argv[]) {
 if (argc != 2) { std::cerr << "Usage: sdk_fixture <new local output.dmp>\n"; return 4; }
 HANDLE file = CreateFileW(argv[1], GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
 if (file == INVALID_HANDLE_VALUE) { std::cerr << "Cannot create NEW fixture: " << GetLastError() << '\n'; return 3; }
 CONTEXT context{}; RtlCaptureContext(&context);
 EXCEPTION_RECORD record{}; record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
 record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
 record.NumberParameters = 2; record.ExceptionInformation[0] = 0; record.ExceptionInformation[1] = 0;
 EXCEPTION_POINTERS pointers{&record, &context};
 MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), &pointers, FALSE};
 const auto flags = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo);
 const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, flags, &info, nullptr, nullptr);
 const DWORD error = ok ? ERROR_SUCCESS : GetLastError(); CloseHandle(file);
 if (!ok) { std::cerr << "SDK writer failed: " << error << '\n'; return 1; }
 std::cout << "Created self-snapshot with synthetic exception metadata; no crash was triggered.\n"; return 0;
}
