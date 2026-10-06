#include "parser.hpp"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
namespace wcde {
namespace {
struct Invalid : std::runtime_error { using std::runtime_error::runtime_error; };
using U = std::uint64_t;
U add(U a, U b) { if (b > (std::numeric_limits<U>::max)() - a) throw Invalid("Integer addition overflow"); return a + b; }
class Reader {
public:
 explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}
 void range(U p, U n) const { if (p > b_.size() || n > b_.size() - p) throw Invalid("Offset or size outside file (truncated or corrupt dump)"); }
 U get(U p, unsigned n) const {
  range(p, n); U v = 0;
  for (unsigned i = 0; i < n; ++i) v |= static_cast<U>(b_[static_cast<std::size_t>(p + i)]) << (8U * i);
  return v;
 }
 void location(U p) const { range(get(p + 4, 4), get(p, 4)); }
 std::string unicode(U p, U bytes) {
  if (bytes % 2 != 0 || bytes > 65536) throw Invalid("Invalid UTF-16 length or string limit exceeded");
  range(p, bytes); string_bytes_ = add(string_bytes_, bytes);
  if (string_bytes_ > 8ULL * 1024 * 1024) throw Invalid("Aggregate string limit exceeded");
  std::string out;
  for (U i = 0; i < bytes; i += 2) {
   auto c = get(p + i, 2);
   if (c >= 0xd800 && c <= 0xdbff) {
    if (i + 2 >= bytes) throw Invalid("Unpaired UTF-16 surrogate");
    const auto lo = get(p + i + 2, 2);
    if (lo < 0xdc00ULL || lo > 0xdfff) throw Invalid("Unpaired UTF-16 surrogate");
    c = 0x10000 + ((c - 0xd800) << 10ULL) + lo - 0xdc00; i += 2;
   } else if (c >= 0xdc00 && c <= 0xdfff) throw Invalid("Unpaired UTF-16 surrogate");
   if (c < 0x80) out.push_back(static_cast<char>(c));
   else if (c < 0x800) { out.push_back(static_cast<char>(0xc0ULL | (c >> 6ULL))); out.push_back(static_cast<char>(0x80ULL | (c & 63ULL))); }
   else if (c < 0x10000) { out.push_back(static_cast<char>(0xe0ULL | (c >> 12ULL))); out.push_back(static_cast<char>(0x80ULL | ((c >> 6ULL) & 63ULL))); out.push_back(static_cast<char>(0x80ULL | (c & 63ULL))); }
   else { out.push_back(static_cast<char>(0xf0ULL | (c >> 18ULL))); out.push_back(static_cast<char>(0x80ULL | ((c >> 12ULL) & 63ULL))); out.push_back(static_cast<char>(0x80ULL | ((c >> 6ULL) & 63ULL))); out.push_back(static_cast<char>(0x80ULL | (c & 63ULL))); }
  }
  return out;
 }
 std::string string(U p) { return p == 0 ? std::string{} : unicode(add(p, 4), get(p, 4)); }
 std::string ansi(U p, U n) {
  if (n > 65536) throw Invalid("Comment limit exceeded");
  range(p, n); string_bytes_ = add(string_bytes_, n);
  if (string_bytes_ > 8ULL * 1024 * 1024) throw Invalid("Aggregate string limit exceeded");
  std::string result;
  for (U i = 0; i < n; ++i) { const auto c = get(p + i, 1); if (c == 0 && i + 1 == n) break;
   if (c < 128) result.push_back(static_cast<char>(c));
   else { result.push_back(static_cast<char>(0xc0ULL | (c >> 6ULL))); result.push_back(static_cast<char>(0x80ULL | (c & 63ULL))); }
  }
  return result;
 }
private:
 const std::vector<std::uint8_t>& b_; U string_bytes_ = 0;
};
struct Object {
 std::string value = "{"; bool first = true;
 void field(const std::string& key, const std::string& raw) { if (!first) value += ','; first = false; value += quote(key) + ':' + raw; }
 void str(const std::string& key, const std::string& v) { field(key, quote(v)); }
 void num(const std::string& key, U v) { field(key, std::to_string(v)); }
 void addr(const std::string& key, U v) { str(key, hex(v)); }
 std::string done() const { return value + '}'; }
};
std::string array(const std::vector<std::string>& v) { std::string out = "["; for (std::size_t i = 0; i < v.size(); ++i) { if (i) out += ','; out += v[i]; } return out + ']'; }
std::string strings(const std::vector<std::string>& v) { std::vector<std::string> q; q.reserve(v.size()); for (const auto& s : v) q.push_back(quote(s)); return array(q); }
std::string arch(U a) { switch (a) { case 0: return "x86"; case 5: return "ARM"; case 6: return "IA64"; case 9: return "x64"; case 12: return "ARM64"; default: return "unknown (" + std::to_string(a) + ')'; } }
std::pair<std::string, std::string> exception(U c) {
 switch (c) {
 case 0xc0000005: return {"ACCESS_VIOLATION", "An instruction attempted to read, write, or execute inaccessible memory."};
 case 0xc000001d: return {"ILLEGAL_INSTRUCTION", "The processor encountered an invalid or unsupported instruction."};
 case 0xc00000fd: return {"STACK_OVERFLOW", "The thread exhausted its stack space."};
 case 0xc0000094: return {"INTEGER_DIVIDE_BY_ZERO", "An integer division used a zero divisor."};
 case 0x80000003: return {"BREAKPOINT", "A breakpoint exception was recorded; this alone does not establish a defect."};
 case 0x80000004: return {"SINGLE_STEP", "A single-step debug exception was recorded."};
 case 0xc0000409: return {"STACK_BUFFER_OVERRUN_OR_FAST_FAIL", "A security check or fast-fail terminated the process. This code alone does not prove a stack buffer overrun."};
 case 0xc0000602: return {"FAIL_FAST_EXCEPTION", "The process requested immediate fail-fast termination."};
 case 0xc0000374: return {"HEAP_CORRUPTION", "Heap corruption was detected; the corrupting operation may have occurred earlier."};
 case 0xe06d7363: return {"MSVC_CPP_EXCEPTION", "A Microsoft C++ exception was recorded; its type is not decoded."};
 case 0xc0000006: return {"IN_PAGE_ERROR", "Required memory could not be paged in; underlying status may appear in exception parameters."};
 case 0xc0000096: return {"PRIVILEGED_INSTRUCTION", "An instruction required privileges unavailable to this process."};
 default: return {"UNKNOWN_EXCEPTION", "This exception code is not decoded by version 1.0.0."};
 }
}
struct Stream { U type, size, rva; };
struct Module { U base = 0, size = 0; std::string name; };
void table(U count, U header, U stride, U size, U min_header, U min_stride, U cap) {
 if (header < min_header || header > size || stride < min_stride || count > cap || count > (size - header) / stride) throw Invalid("Invalid list header, entry size, count, or count limit exceeded");
}
void need(const Stream& s, U bytes) { if (s.size < bytes) throw Invalid("Known stream is shorter than its required structure"); }
}
std::string hex(U value) { std::ostringstream s; s << "0x" << std::hex << std::setfill('0') << std::setw(16) << value; return s.str(); }
std::string quote(const std::string& v) {
 static constexpr char digits[] = "0123456789abcdef"; std::string out = "\"";
 for (const char ch : v) { const auto c = static_cast<unsigned int>(static_cast<unsigned char>(ch));
  if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
  else if (c < 32 || c == 127) { out += "\\u00"; out += digits[c >> 4ULL]; out += digits[c & 15ULL]; }
  else out += static_cast<char>(c);
 }
 return out + '"';
}
Report parse(const std::vector<std::uint8_t>& bytes) {
 Report out;
 try {
  if (bytes.size() > max_file_size) throw Invalid("Dump exceeds 256 MiB resource limit");
  Reader r(bytes); r.range(0, 32);
  if (r.get(0, 4) != 0x504d444d) throw Invalid("Unsupported signature: expected MDMP minidump (kernel PAGE dumps are unsupported)");
  const auto version = r.get(4, 4);
  if ((version & 0xffffULL) != 0xa793) throw Invalid("Unsupported minidump format version");
  const auto count = r.get(8, 4), dir = r.get(12, 4), flags = r.get(24, 8);
  if (count > 4096) throw Invalid("Stream count limit exceeded");
  r.range(dir, count * 12);
  std::vector<Stream> streams; std::set<U> types; std::vector<std::string> stream_json;
  for (U i = 0; i < count; ++i) {
   const U p = dir + i * 12; Stream s{r.get(p, 4), r.get(p + 4, 4), r.get(p + 8, 4)}; r.range(s.rva, s.size);
   if (s.type != 0 && !types.insert(s.type).second) throw Invalid("Duplicate stream type");
   streams.push_back(s); Object j; j.num("type", s.type); j.num("size", s.size); j.num("rva", s.rva); stream_json.push_back(j.done());
  }
  Object d; d.num("size_bytes", bytes.size()); d.num("format_version", version); d.num("timestamp_unix", r.get(20, 4)); d.addr("flags", flags);
  d.num("checksum", r.get(16, 4)); d.field("streams", array(stream_json)); d.field("full_memory_requested", (flags & 2ULL) ? "true" : "false");
  out.sections.push_back({"dump", d.done(), {"Standard Windows minidump; " + std::to_string(bytes.size()) + " bytes; flags " + hex(flags), "Dump timestamp (Unix seconds): " + std::to_string(r.get(20, 4))}});
  std::vector<Module> modules; bool have_exception = false; U exception_address = 0, exception_thread = 0;
  std::vector<U> thread_ids; std::vector<std::string> memory_json, memory_lines, comments;
  for (const auto& s : streams) { const U p = s.rva;
   switch (s.type) {
   case 0: break;
   case 7: {
    need(s, 56); const auto a = r.get(p, 2); Object j;
    j.str("architecture", arch(a)); j.num("architecture_id", a); j.num("processor_count", r.get(p + 6, 1)); j.num("product_type", r.get(p + 7, 1));
    j.num("major", r.get(p + 8, 4)); j.num("minor", r.get(p + 12, 4)); j.num("build", r.get(p + 16, 4)); j.num("platform_id", r.get(p + 20, 4)); j.num("suite_mask", r.get(p + 28, 2)); j.str("service_pack", r.string(r.get(p + 24, 4)));
    out.sections.push_back({"system", j.done(), {"Architecture: " + arch(a), "Windows version/build: " + std::to_string(r.get(p + 8, 4)) + '.' + std::to_string(r.get(p + 12, 4)) + '.' + std::to_string(r.get(p + 16, 4)), "Product type: " + std::to_string(r.get(p + 7, 1)) + " (1=workstation, 2=domain controller, 3=server)"}});
    if (a != 0 && a != 9 && a != 12) out.warnings.push_back("Architecture is not a common x86/x64/ARM64 target; metadata only is decoded.");
    if (a != 9) out.findings.push_back("This x64 tool reads other target architectures; contexts and stacks are not interpreted. Use a matching debugger architecture for deeper diagnosis.");
    break;
   }
   case 6: {
    need(s, 168); have_exception = true; exception_thread = r.get(p, 4); exception_address = r.get(p + 24, 8);
    const auto code = r.get(p + 8, 4), n = r.get(p + 32, 4); if (n > 15) throw Invalid("Exception parameter count exceeds 15");
    r.location(p + 160); const auto decoded = exception(code); Object j;
    j.num("thread_id", exception_thread); j.addr("code", code); j.str("name", decoded.first); j.str("description", decoded.second);
    j.addr("flags", r.get(p + 12, 4)); j.addr("address", exception_address); j.addr("record_address", r.get(p + 16, 8)); j.num("context_bytes", r.get(p + 160, 4));
    std::vector<std::string> parameters; parameters.reserve(static_cast<std::size_t>(n)); for (U i = 0; i < n; ++i) parameters.push_back(quote(hex(r.get(p + 40 + i * 8, 8)))); j.field("parameters", array(parameters));
    std::vector<std::string> lines{"Exception: " + decoded.first + " (" + hex(code) + ')', decoded.second, "Address: " + hex(exception_address) + "; thread ID: " + std::to_string(exception_thread), "Exception flags: " + hex(r.get(p + 12, 4))};
    if ((code == 0xc0000005ULL || code == 0xc0000006) && n >= 2) {
     const auto operation = r.get(p + 40, 8); const std::string op = operation == 0 ? "read" : operation == 1 ? "write" : operation == 8 ? "execute" : "unknown";
     j.str("memory_operation", op); j.addr("referenced_address", r.get(p + 48, 8)); lines.push_back("Memory operation: " + op + "; referenced address: " + hex(r.get(p + 48, 8)));
    }
    if (code == 0xc0000409 && n) { j.addr("fast_fail_subcode", r.get(p + 40, 8)); lines.push_back("Fast-fail subcode: " + hex(r.get(p + 40, 8)) + " (as recorded)"); }
    out.sections.push_back({"exception", j.done(), lines}); out.findings.push_back("Dump identifies " + decoded.first + ": " + decoded.second); break;
   }
   case 4: case 14: {
    const bool unloaded = s.type == 14; need(s, unloaded ? 12 : 4);
    const U h = unloaded ? r.get(p, 4) : 4, stride = unloaded ? r.get(p + 4, 4) : 108;
    const U n = r.get(p + (unloaded ? 8 : 0), 4); table(n, h, stride, s.size, unloaded ? 12 : 4, unloaded ? 24 : 108, 16384);
    std::vector<std::string> entries, lines;
    for (U i = 0; i < n; ++i) {
     const U q = p + h + i * stride, base = r.get(q, 8), size = r.get(q + 8, 4); (void)add(base, size);
     const auto name = r.string(r.get(q + 20, 4)); Object j; j.addr("base", base); j.num("size", size); j.str("path", name);
     const auto slash = name.find_last_of("\\/"); j.str("name", slash == std::string::npos ? name : name.substr(slash + 1));
     j.num("checksum", r.get(q + 12, 4)); j.num("timestamp_unix", r.get(q + 16, 4)); std::string v;
     if (!unloaded) {
      r.location(q + 76); r.location(q + 84);
      if (r.get(q + 24, 4) == 0xfeef04bd) {
       const auto ms = r.get(q + 32, 4), ls = r.get(q + 36, 4);
       v = std::to_string(ms >> 16ULL) + '.' + std::to_string(ms & 65535ULL) + '.' + std::to_string(ls >> 16ULL) + '.' + std::to_string(ls & 65535ULL); j.str("file_version", v);
       const auto pms = r.get(q + 40, 4), pls = r.get(q + 44, 4);
       j.str("product_version", std::to_string(pms >> 16ULL) + '.' + std::to_string(pms & 65535ULL) + '.' + std::to_string(pls >> 16ULL) + '.' + std::to_string(pls & 65535ULL));
      } else { j.field("file_version", "null"); j.field("product_version", "null"); }
      modules.push_back({base, size, name});
     }
     entries.push_back(j.done()); lines.push_back(hex(base) + " + " + std::to_string(size) + " bytes " + quote(name) + (v.empty() ? "" : " version " + v));
    }
    out.sections.push_back({unloaded ? "unloaded_modules" : "modules", array(entries), lines}); break;
   }
   case 3: case 8: {
    const U stride = s.type == 8 ? 64 : 48; need(s, 4); const U n = r.get(p, 4); table(n, 4, stride, s.size, 4, stride, 16384);
    std::vector<std::string> entries, lines; std::set<U> ids;
    for (U i = 0; i < n; ++i) {
     const U q = p + 4 + i * stride, id = r.get(q, 4); if (!ids.insert(id).second) throw Invalid("Duplicate thread ID");
     thread_ids.push_back(id); r.location(q + 32); r.location(q + 40); (void)add(r.get(q + 24, 8), r.get(q + 32, 4));
     if (s.type == 8) { r.location(q + 56); (void)add(r.get(q + 48, 8), r.get(q + 56, 4)); }
     Object j; j.num("id", id); j.num("suspend_count", r.get(q + 4, 4)); j.num("priority_class", r.get(q + 8, 4)); j.num("priority", r.get(q + 12, 4));
     j.addr("teb", r.get(q + 16, 8)); j.addr("stack_base", r.get(q + 24, 8)); j.num("stack_bytes", r.get(q + 32, 4)); j.num("context_bytes", r.get(q + 40, 4));
     entries.push_back(j.done()); lines.push_back("Thread " + std::to_string(id) + "; stack " + hex(r.get(q + 24, 8)) + "; captured stack bytes " + std::to_string(r.get(q + 32, 4)));
    }
    out.sections.push_back({s.type == 8 ? "extended_threads" : "threads", array(entries), lines}); break;
   }
   case 15: {
    need(s, 24); const auto size = r.get(p, 4); if (size < 24 || size > s.size) throw Invalid("Invalid miscellaneous info size");
    const auto f = r.get(p + 4, 4); Object j; j.addr("flags", f); j.field("process_id", (f & 1ULL) ? std::to_string(r.get(p + 8, 4)) : "null");
    j.field("process_create_time_unix", (f & 2ULL) ? std::to_string(r.get(p + 12, 4)) : "null"); j.field("user_time_seconds", (f & 2ULL) ? std::to_string(r.get(p + 16, 4)) : "null"); j.field("kernel_time_seconds", (f & 2ULL) ? std::to_string(r.get(p + 20, 4)) : "null");
    std::vector<std::string> lines;
    if (f & 1ULL) lines.push_back("Process ID: " + std::to_string(r.get(p + 8, 4))); else out.unavailable.push_back("Process ID is not flagged valid.");
    if (f & 2ULL) lines.push_back("Process creation (Unix seconds): " + std::to_string(r.get(p + 12, 4))); else out.unavailable.push_back("Process creation time is not flagged valid.");
    out.sections.push_back({"process", j.done(), lines}); break;
   }
   case 5: case 9: case 16: {
    const bool full = s.type == 9, info = s.type == 16; need(s, (full || info) ? 16 : 4);
    const U n = r.get(p + (info ? 8 : 0), (full || info) ? 8 : 4);
    const U h = info ? r.get(p, 4) : full ? 16 : 4, stride = info ? r.get(p + 4, 4) : 16;
    table(n, h, stride, s.size, (full || info) ? 16 : 4, info ? 48 : 16, 100000);
    U total = 0, cursor = full ? r.get(p + 8, 8) : 0; if (full) r.range(cursor, 0);
    for (U i = 0; i < n; ++i) {
     const U q = p + h + i * stride, size = r.get(q + (info ? 24 : 8), (full || info) ? 8 : 4);
     (void)add(r.get(q, 8), size); total = add(total, size);
     if (!info) { if (full) { r.range(cursor, size); cursor = add(cursor, size); } else r.location(q + 8); }
    }
    Object j; j.num("stream_type", s.type); j.num("range_count", n); j.str(info ? "described_bytes" : "captured_bytes", std::to_string(total));
    memory_json.push_back(j.done()); memory_lines.push_back(std::string(info ? "Memory region metadata: " : "Captured memory: ") + std::to_string(n) + " ranges, " + std::to_string(total) + " bytes"); break;
   }
   case 10: case 11: {
    auto comment = s.type == 10 ? r.ansi(p, s.size) : r.unicode(p, s.size);
    if (!comment.empty() && comment.back() == '\0') comment.pop_back();
    comments.push_back(comment); break;
   }
   default: out.warnings.push_back("Stream type " + std::to_string(s.type) + " is bounded but not decoded by v1.0.0."); break;
   }
  }
  if (!memory_json.empty()) out.sections.push_back({"memory", array(memory_json), memory_lines});
  if (!comments.empty()) { std::vector<std::string> lines; lines.reserve(comments.size()); for (const auto& c : comments) lines.push_back(quote(c)); out.sections.push_back({"comments", strings(comments), lines}); }
  if (have_exception) {
   std::vector<std::string> matches;
   for (const auto& m : modules) if (exception_address >= m.base && exception_address - m.base < m.size) matches.push_back(m.name);
   if (matches.empty()) out.findings.push_back("Exception address is not mapped to a recorded loaded module; dynamic code or missing metadata may explain this.");
   for (const auto& m : matches) out.findings.push_back("Exception address falls within module " + quote(m) + ". This is crash location evidence, not proof of root cause.");
   Object j; j.field("module_paths", strings(matches)); j.field("ambiguous", matches.size() > 1 ? "true" : "false"); out.sections.push_back({"exception_location", j.done(), {}});
   if (matches.size() > 1) out.warnings.push_back("Overlapping module ranges make exception location ambiguous.");
   if ((types.count(3) || types.count(8)) && std::find(thread_ids.begin(), thread_ids.end(), exception_thread) == thread_ids.end()) out.warnings.push_back("Exception thread is absent from recorded thread lists.");
  }
  const std::pair<U, const char*> required[] = {{7,"System information"},{6,"Exception information"},{4,"Loaded module list"},{15,"Process information"}};
  for (const auto& req : required) if (!types.count(req.first)) out.unavailable.push_back(std::string(req.second) + " stream is absent.");
  if (!types.count(3) && !types.count(8)) out.unavailable.push_back("Thread list stream is absent.");
  if (!types.count(5) && !types.count(9)) out.unavailable.push_back("Captured memory list stream is absent (thread stacks may be present).");
  if (!types.count(14)) out.unavailable.push_back("Unloaded module list is absent; prior unloads cannot be assessed.");
  if (!(flags & 2ULL)) out.findings.push_back("Dump does not request full process memory; captured memory may be limited.");
  else out.findings.push_back("Full-memory flag records writer intent; it does not guarantee every useful page is available.");
  if (!have_exception) out.findings.push_back("Dump lacks exception information; it may be a manually captured snapshot. A crash cannot be established from this metadata alone.");
  out.findings.push_back("Crash location and root cause differ: bad input, prior memory corruption, or another component can lead to an exception here. No stack unwinding or debugger-quality root-cause analysis is performed.");
  out.unavailable.push_back("Symbols, CPU-context decoding, call stacks, and root-cause diagnosis are not provided by v1.0.0.");
  out.exit_code = (!types.count(7) || !have_exception || !types.count(4) || (!types.count(3) && !types.count(8)) || !out.warnings.empty()) ? 1 : 0;
 } catch (const Invalid& e) { out = Report{}; out.exit_code = 2; out.error = e.what(); }
 return out;
}
std::string json(const Report& r) {
 Object j; j.str("schema_version", "1.0.0"); j.str("tool_version", "1.0.0"); j.num("exit_code", static_cast<U>(r.exit_code));
 j.str("status", r.exit_code == 0 ? "complete" : r.exit_code == 1 ? "partial" : "error"); j.field("error", r.error.empty() ? "null" : quote(r.error));
 for (const std::string name : {"dump","system","process","exception","exception_location","threads","extended_threads","modules","unloaded_modules","memory","comments"}) {
  const auto found = std::find_if(r.sections.begin(), r.sections.end(), [&](const Section& s) { return s.name == name; });
  const bool list = name == "threads" || name == "extended_threads" || name == "modules" || name == "unloaded_modules" || name == "memory" || name == "comments";
  j.field(name, found == r.sections.end() ? (list ? "[]" : "null") : found->json);
 }
 j.field("findings", strings(r.findings)); j.field("warnings", strings(r.warnings)); j.field("unavailable_data", strings(r.unavailable)); return j.done() + '\n';
}
std::string text(const Report& r, const std::string& selection) {
 if (!r.error.empty()) return "Error: " + r.error + '\n';
 std::string out = "windows-crash-dump-explainer 1.0.0 - " + std::string(r.exit_code ? "partial" : "complete") + " triage\n";
 for (const auto& s : r.sections) {
  const bool summary = s.name == "dump" || s.name == "system" || s.name == "process" || s.name == "exception";
  if (!selection.empty() && !(selection == "summary" ? summary : s.name == selection || (selection == "modules" && s.name == "unloaded_modules") || (selection == "threads" && s.name == "extended_threads"))) continue;
  out += '\n' + s.name + ":\n"; for (const auto& line : s.lines) out += "  " + line + '\n';
 }
 for (const auto& group : {std::make_pair("Findings", &r.findings), std::make_pair("Warnings", &r.warnings), std::make_pair("Unavailable data", &r.unavailable)}) {
  if (!group.second->empty()) { out += '\n' + std::string(group.first) + ":\n"; for (const auto& line : *group.second) out += "  " + line + '\n'; }
 }
 return out;
}
}


