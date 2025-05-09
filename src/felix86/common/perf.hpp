#pragma once

#include <string>
#include "felix86/common/log.hpp"
#include "fmt/format.h"

struct Perf {
    Perf() {
        std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
        f = fopen(path.c_str(), "a");
        ASSERT(f);
        fd = fileno(f);
        ASSERT(fd > 0);
    }

    ~Perf() {
        fclose(f);
    }

    void addToFile(unsigned long address, unsigned long size, const std::string& symbol) {
        std::string full = fmt::format("{:x} {:x} {}\n", address, size, symbol);
        int written = syscall(SYS_write, fd, full.data(), full.size());
        ASSERT_MSG(written == full.size(), "%lx != %lx (errno: %d)", written, full.size(), errno);
    }

private:
    FILE* f = nullptr;
    int fd = -1;
};