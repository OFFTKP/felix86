#pragma once

#include <string>
#include <fcntl.h>
#include <sys/syscall.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"
#include "fmt/format.h"

struct Perf {
    Perf() {
        if (g_config.perf_blocks || g_config.perf_libs || g_config.perf_global) {
            std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
            fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
            if (fd < 0) {
                WARN("Failed to open perf map file for process %d", getpid());
                return;
            }
            fd = FD::moveToHighNumber(fd);
            FD::protect(fd);
            f = fdopen(fd, "a");
            ASSERT(f);
        }
    }

    ~Perf() {
        if (f) {
            fclose(f);
        }
    }

    void addToFile(unsigned long address, unsigned long size, const std::string& symbol) {
        if (f) {
            std::string full = fmt::format("{:x} {:x} {}\n", address, size, symbol);
            int written = syscall(SYS_write, fd, full.data(), full.size());
            ASSERT_MSG(written == (int)full.size(), "%lx != %lx (errno: %d)", written, full.size(), errno);
        }
    }

private:
    FILE* f = nullptr;
    int fd = -1;
};