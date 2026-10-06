#include "parser.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
// Optional libFuzzer harness. No filesystem/system APIs. Compile with an offline
// clang-cl libFuzzer+ASan toolchain and instrument src/parser.cpp as well.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
 if (size > 1024 * 1024) return 0;
 const std::vector<std::uint8_t> bytes(data, data + size);
 const auto report = wcde::parse(bytes);
 (void)wcde::json(report);
 return 0;
}
