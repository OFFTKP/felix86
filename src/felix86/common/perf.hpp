#pragma once

#include <string>
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"
#include "fmt/format.h"

struct Perf {
    Perf() {
        if (g_config.perf_blocks || g_config.perf_libs || g_config.perf_global) {
            std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
            f = fopen(path.c_str(), "a");
            if (f) {
                fd = fileno(f);
                ASSERT(fd > 0);
                // fd = FD::moveToHighNumber(fd); -- TODO: doing this isn't safe probably with FILE*, rewrite to use syscalls instead of FILE*
                FD::protect(fd);
            } else {
                // TODO: for some reason, running a setuid app through binfmt_misc after being forked
                // loves failing the fopen and idk why, but game processes aren't setuid so we can perf them fine
                WARN("Failed to open perf map file for process %d", getpid());
            }
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