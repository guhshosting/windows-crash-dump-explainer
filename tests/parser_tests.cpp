#include "parser.hpp"
#include <windows.h>
#include <dbghelp.h>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
// Verify wire-layout assumptions against the installed documented Windows SDK.
static_assert(sizeof(MINIDUMP_HEADER) == 32);
static_assert(sizeof(MINIDUMP_SYSTEM_INFO) == 56);
static_assert(sizeof(MINIDUMP_MODULE) == 108);
static_assert(sizeof(MINIDUMP_THREAD) == 48);
static_assert(sizeof(MINIDUMP_THREAD_EX) == 64);
static_assert(sizeof(MINIDUMP_EXCEPTION_STREAM) == 168);
static_assert(offsetof(MINIDUMP_MODULE, CvRecord) == 76);
static_assert(offsetof(MINIDUMP_EXCEPTION_STREAM, ThreadContext) == 160);
namespace {
using Bytes = std::vector<std::uint8_t>;
using U = std::uint64_t;
// Fixed wire-fixture positional writer; callers use explicit offset/value/width.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void put(Bytes& b, std::size_t p, U v, unsigned n = 4) { if (p + n > b.size()) throw std::runtime_error("Fixture bounds"); for (unsigned i = 0; i < n; ++i) b[p + i] = static_cast<std::uint8_t>(v >> (8U * i)); }
struct Fixture {
 Bytes b = Bytes(32 + 32 * 12); std::size_t count = 0;
 Fixture() { put(b, 0, 0x504d444d); put(b, 4, 0xa793); put(b, 12, 32); put(b, 20, 1700000000); }
 std::size_t stream(U type, std::size_t size) {
  const auto p = b.size(); b.resize(p + size); put(b, 32 + count * 12, type); put(b, 36 + count * 12, size); put(b, 40 + count * 12, p); ++count; put(b, 8, count); return p;
 }
 std::size_t string(const std::u16string& s) { const auto p = b.size(); b.resize(p + 4 + s.size() * 2); put(b, p, s.size() * 2); for (std::size_t i = 0; i < s.size(); ++i) put(b, p + 4 + i * 2, s[i], 2); return p; }
};
Fixture full() {
 Fixture f;
 auto p = f.stream(7, 56); put(f.b, p, 9, 2); put(f.b, p + 6, 4, 1); put(f.b, p + 7, 1, 1); put(f.b, p + 8, 10); put(f.b, p + 16, 22631); put(f.b, p + 20, 2);
 p = f.stream(6, 168); put(f.b, p, 42); put(f.b, p + 8, 0xc0000005); put(f.b, p + 24, 0x1010, 8); put(f.b, p + 32, 2); put(f.b, p + 40, 1, 8); put(f.b, p + 48, 0xdeadbeef, 8);
 p = f.stream(4, 4 + 2 * 108); put(f.b, p, 2); put(f.b, p + 4, 0x1000, 8); put(f.b, p + 12, 4096); put(f.b, p + 28, 0xfeef04bd); put(f.b, p + 36, 0x10002); put(f.b, p + 40, 0x30004);
 const auto n1 = f.string(u"C:\\test\\sample.dll"); put(f.b, p + 24, n1);
 put(f.b, p + 112, 0x2000, 8); put(f.b, p + 120, 4096); const auto n2 = f.string(u"second.dll"); put(f.b, p + 132, n2);
 p = f.stream(3, 52); put(f.b, p, 1); put(f.b, p + 4, 42);
 p = f.stream(14, 36); put(f.b, p, 12); put(f.b, p + 4, 24); put(f.b, p + 8, 1); put(f.b, p + 12, 0x8000, 8); put(f.b, p + 20, 4096); const auto n3 = f.string(u"unloaded.dll"); put(f.b, p + 32, n3);
 p = f.stream(15, 24); put(f.b, p, 24); put(f.b, p + 4, 3); put(f.b, p + 8, 123); put(f.b, p + 12, 1699990000);
 p = f.stream(5, 20); put(f.b, p, 1); put(f.b, p + 4, 0x9000, 8); put(f.b, p + 12, 4); const auto data = f.b.size(); f.b.resize(data + 4); put(f.b, p + 16, data);
 p = f.stream(11, 8); put(f.b, p, 'a', 2); put(f.b, p + 2, '"', 2); put(f.b, p + 4, 27, 2);
 return f;
}
std::size_t at(const Fixture& f, U type) {
 for (std::size_t i = 0; i < f.count; ++i) { const auto q = 32 + i * 12; U t = 0; for (unsigned j = 0; j < 4; ++j) t |= static_cast<U>(f.b[q + j]) << (8U * j); if (t == type) { U p = 0; for (unsigned j = 0; j < 4; ++j) p |= static_cast<U>(f.b[q + 8 + j]) << (8U * j); return static_cast<std::size_t>(p); } }
 throw std::runtime_error("Fixture stream absent");
}
int checks = 0;
void check(bool ok, const char* name) { ++checks; if (!ok) throw std::runtime_error(name); }
void malformed(const Fixture& f, const char* name) { check(wcde::parse(f.b).exit_code == 2, name); }
void write(const std::filesystem::path& dir, const char* name, const Bytes& b) { std::ofstream out(dir / name, std::ios::binary); out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); if (!out) throw std::runtime_error("Cannot write fixture"); }
}
int main(int argc, char* argv[]) {
 try {
  const auto good = full(); const auto report = wcde::parse(good.b); const auto j = wcde::json(report);
  check(report.exit_code == 0, "complete"); check(j == wcde::json(wcde::parse(good.b)), "deterministic");
  check(j.find("ACCESS_VIOLATION") != std::string::npos, "exception"); check(j.find("sample.dll") != std::string::npos, "mapping");
  check(j.find("1.2.3.4") != std::string::npos, "version"); check(j.find("unloaded.dll") != std::string::npos, "unloaded");
  check(j.find("second.dll") != std::string::npos, "multiple modules"); check(j.find("\\u001b") != std::string::npos, "terminal escaping");
  Fixture minimal; minimal.b.resize(32); check(wcde::parse(minimal.b).exit_code == 1, "minimal missing exception");
  { auto f = good; put(f.b, 32 + 12, 0); check(wcde::parse(f.b).exit_code == 1, "missing exception"); }
  { auto f = good; f.stream(0x12345678, 0); check(wcde::parse(f.b).exit_code == 1, "unknown stream"); }
  { auto f = good; put(f.b, at(f, 7), 65535, 2); check(wcde::parse(f.b).exit_code == 1, "unknown architecture"); }
  { auto f = good; put(f.b, at(f, 7), 0, 2); check(wcde::parse(f.b).exit_code == 0, "x86 metadata"); }
  { auto f = good; put(f.b, 4, 0); malformed(f, "version"); }
  { auto f = good; put(f.b, 0, 0x45474150); malformed(f, "kernel format"); }
  { auto f = good; put(f.b, 12, 0xffffffff); malformed(f, "directory RVA"); }
  { auto f = good; put(f.b, 8, 0xffffffff); malformed(f, "stream count"); }
  { auto f = good; put(f.b, 40, 0xffffffff); malformed(f, "stream RVA"); }
  { auto f = good; put(f.b, 36, 0xffffffff); malformed(f, "stream size"); }
  { auto f = good; put(f.b, at(f, 4), 0xffffffff); malformed(f, "module count"); }
  { auto f = good; put(f.b, at(f, 3), 0xffffffff); malformed(f, "thread count"); }
  { auto f = good; put(f.b, at(f, 6) + 32, 16); malformed(f, "exception parameter count"); }
  { auto f = good; put(f.b, at(f, 6) + 160, 32); put(f.b, at(f, 6) + 164, 0xffffffff); malformed(f, "context RVA"); }
  { auto f = good; put(f.b, at(f, 4) + 24, 0xffffffff); malformed(f, "name RVA"); }
  { auto f = good; const auto p = f.string(u"x"); put(f.b, p, 3); put(f.b, at(f, 4) + 24, p); malformed(f, "odd string"); }
  { auto f = good; const auto p = f.string(u"x"); put(f.b, p, 65538); put(f.b, at(f, 4) + 24, p); malformed(f, "long string"); }
  { auto f = good; const auto p = f.string(std::u16string(1, static_cast<char16_t>(0xd800))); put(f.b, at(f, 4) + 24, p); malformed(f, "surrogate"); }
  { auto f = good; const auto p = f.string(u"\U0001f600"); put(f.b, at(f, 4) + 24, p); check(wcde::parse(f.b).exit_code == 0, "surrogate pair"); }
  { auto f = good; put(f.b, at(f, 4) + 4, (std::numeric_limits<U>::max)(), 8); malformed(f, "module overflow"); }
  { auto f = good; put(f.b, at(f, 14) + 4, 0); malformed(f, "zero stride"); }
  { auto f = good; put(f.b, at(f, 15), 23); malformed(f, "misc size"); }
  { auto f = good; put(f.b, 32 + 12, 7); malformed(f, "duplicate"); }
  { auto f = good; put(f.b, at(f, 4) + 112, 0x1000, 8); check(wcde::parse(f.b).exit_code == 1, "ambiguous mapping"); }
  { auto f = good; put(f.b, at(f, 6) + 24, 0x3000, 8); check(wcde::json(wcde::parse(f.b)).find("\"module_paths\":[]") != std::string::npos, "exclusive range end"); }
  for (const U code : {0xc000001dULL,0xc00000fdULL,0xc0000094ULL,0x80000003ULL,0xc0000409ULL,0xc0000602ULL,0xf0000000ULL}) { auto f = good; put(f.b, at(f, 6) + 8, code); check(wcde::parse(f.b).exit_code == 0, "exception decode"); }
  { Fixture f; auto p = f.stream(9, 32); put(f.b, p, 1, 8); put(f.b, p + 8, f.b.size(), 8); put(f.b, p + 16, 0x1000, 8); put(f.b, p + 24, 4, 8); f.b.resize(f.b.size() + 4); check(wcde::parse(f.b).exit_code == 1, "memory64"); put(f.b, p + 24, (std::numeric_limits<U>::max)(), 8); malformed(f, "memory overflow"); }
  { Fixture f; const auto p = f.stream(16, 64); put(f.b, p, 16); put(f.b, p + 4, 48); put(f.b, p + 8, 1, 8); put(f.b, p + 16, 0x1000, 8); put(f.b, p + 40, 4096, 8); check(wcde::parse(f.b).exit_code == 1, "memory info"); put(f.b, p + 8, (std::numeric_limits<U>::max)(), 8); malformed(f, "64bit count"); }
  { auto f = good; put(f.b, 36, 55); malformed(f, "short system stream"); }
  { auto f = good; put(f.b, at(f, 3) + 36, 4); put(f.b, at(f, 3) + 40, 0xffffffff); malformed(f, "stack RVA"); }
  { auto f = good; put(f.b, at(f, 4) + 80, 4); put(f.b, at(f, 4) + 84, 0xffffffff); malformed(f, "debug-record RVA"); }
  { auto f = good; put(f.b, at(f, 5) + 16, 0xffffffff); malformed(f, "memory-list RVA"); }
  { Fixture f; const auto p = f.stream(8, 68); put(f.b, p, 1); put(f.b, p + 4, 100); check(wcde::parse(f.b).exit_code == 1, "extended thread"); put(f.b, p + 60, 4); put(f.b, p + 64, 0xffffffff); malformed(f, "backing store RVA"); }
  { Fixture f; const auto p = f.stream(10, 3); put(f.b, p, 0xe9, 1); put(f.b, p + 1, 27, 1); check(wcde::json(wcde::parse(f.b)).find("\\u001b") != std::string::npos, "ANSI comment escaping"); }
  { auto f = good; put(f.b, at(f, 15) + 4, 0); const auto result = wcde::json(wcde::parse(f.b)); check(result.find("\"process_id\":null") != std::string::npos, "misc validity bits"); }
  { Fixture f; const auto p = f.stream(4, 4 + 130 * 108); put(f.b, p, 130); const auto name = f.string(std::u16string(32768, u'x')); for (std::size_t i = 0; i < 130; ++i) put(f.b, p + 24 + i * 108, name); malformed(f, "aggregate string budget"); }
  { auto f = good; put(f.b, at(f, 6), 99); check(wcde::parse(f.b).exit_code == 1, "thread mismatch"); }
  { Fixture f; const auto p = f.stream(9, 16); put(f.b, p, (std::numeric_limits<U>::max)(), 8); malformed(f, "memory64 oversized count"); }
  for (std::size_t n = 0; n < good.b.size(); ++n) { auto b = good.b; b.resize(n); const auto r = wcde::parse(b); check(r.exit_code >= 1 && r.exit_code <= 2, "every truncation"); }
  // Deterministic mutation corpus is intentional; not a security RNG.
  // NOLINTNEXTLINE(bugprone-random-generator-seed)
  std::mt19937 random(1234567);
  for (unsigned n = 0; n < 20000; ++n) { auto b = good.b; for (unsigned k = 0; k < 1 + n % 8; ++k) b[random() % b.size()] = static_cast<std::uint8_t>(random()); const auto r = wcde::parse(b); check(r.exit_code >= 0 && r.exit_code <= 2, "mutation corpus"); }
  check(wcde::text(report, "summary").find("modules:\n") == std::string::npos, "summary selector");
  if (argc == 3 && std::string(argv[1]) == "--write-fixtures") { const std::filesystem::path dir(argv[2]); std::filesystem::create_directories(dir); write(dir, "complete.dmp", good.b); write(dir, "minimal.dmp", minimal.b); write(dir, "malformed.dmp", Bytes(7)); }
  else if (argc != 1) throw std::runtime_error("Invalid test usage");
  std::cout << checks << " checks passed (including 20000 deterministic mutations)\n"; return 0;
 } catch (const std::exception& e) { std::fprintf(stderr, "TEST FAILURE: %s\n", e.what()); return 1; } catch (...) { std::fputs("Unknown test failure\n", stderr); return 1; }
}


