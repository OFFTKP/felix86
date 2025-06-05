#pragma once

#include <string>
#include "felix86/common/log.hpp"
#include "fmt/format.h"

struct Perf {
    Perf() {
        std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
        f = fopen(path.c_str(), "a");
        if (f) {
            fd = fileno(f);
            ASSERT(fd > 0);
        } else if (g_config.perf_blocks || g_config.perf_libs || g_config.perf_global) {
            WARN("Failed to open perf map file for process %d", getpid());
            g_config.perf_blocks = false;
            g_config.perf_libs = false;
            g_config.perf_global = false;
        } else {
            WARN("File: %s", path.c_str());
        }
    }

    ~Perf() {
        fclose(f);
    }

    void addToFile(unsigned long address, unsigned long size, const std::string& symbol) {
        std::string full = fmt::format("{:x} {:x} {}\n", address, size, symbol);
        int written = syscall(SYS_write, fd, full.data(), full.size());
        ASSERT_MSG(written == (int)full.size(), "%lx != %lx (errno: %d)", written, full.size(), errno);
    }

private:
    FILE* f = nullptr;
    int fd = -1;
};