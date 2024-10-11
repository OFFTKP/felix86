#include "fmt/format.h"
#include "utility.hpp"

namespace {
std::string GetBlockName(u32 name) {
    u32 block_index = name >> 20;
    if (block_index == 0) {
        return "Entry";
    } else if (block_index == 1) {
        return "Exit";
    } else {
        return fmt::format("{}", block_index - 2);
    }
}

std::string ToVarName(u32 name) {
    return std::to_string(name & ((1 << 20) - 1));
}
} // namespace

std::string GetNameString(u32 name) {
    return fmt::format("%{}@{}", ToVarName(name), GetBlockName(name));
}