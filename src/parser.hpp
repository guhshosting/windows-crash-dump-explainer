#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace wcde {
constexpr std::uint64_t max_file_size = 256ULL * 1024 * 1024;
struct Section { std::string name, json; std::vector<std::string> lines; };
struct Report {
 int exit_code = 0;
 std::string error;
 std::vector<Section> sections;
 std::vector<std::string> findings, warnings, unavailable;
};
Report parse(const std::vector<std::uint8_t>& bytes);
std::string json(const Report& report);
std::string text(const Report& report, const std::string& selection);
std::string quote(const std::string& value);
std::string hex(std::uint64_t value);
}
