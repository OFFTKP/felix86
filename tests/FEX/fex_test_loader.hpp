#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "felix86/common/utility.hpp"
#include "felix86/common/x86.hpp"
#include "felix86/emulator.hpp"

/*
    Loads tests from the FEX test suite at external/FEX/ASM
    Thanks to FEX for providing the test suite:
    https://github.com/FEX-Emu/FEX
*/

struct FEXTestLoader {
    FEXTestLoader(const std::filesystem::path& path);
    ~FEXTestLoader();

    void Run();

    void Validate();

    static void RunTest(const std::filesystem::path& path);

private:
    std::unique_ptr<Emulator> emulator{};
    ThreadState* state = nullptr;
    std::vector<u8> buffer{};
    std::string json{};
    std::array<std::optional<u64>, 16> expected_gpr{};
    std::array<std::optional<XmmReg>, 16> expected_xmm{};
    std::vector<std::pair<u64, u64>> memory_mappings{};
    std::vector<std::pair<void*, u64>> munmap_me;
};