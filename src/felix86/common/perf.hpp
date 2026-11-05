#pragma once

#include <atomic>
#include <string>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <time.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/fd.hpp"
#include "fmt/format.h"

struct Perf {
    Perf() {
        static bool first_in_image = true;
        bool append = g_execve_process && first_in_image;
        first_in_image = false;

        if (g_config.perf_blocks || g_config.perf_libs || g_config.perf_global) {
            std::string path = "/tmp/perf-" + std::to_string(getpid()) + ".map";
            fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | (append ? 0 : O_TRUNC), 0600);
            if (fd < 0) {
                WARN("Failed to open perf map file for process %d", getpid());
            } else {
                fd = FD::moveToHighNumber(fd);
                FD::protect(fd);
                f = fdopen(fd, "a");
                ASSERT(f);
            }
        }

        if (g_config.perf_jitdump) {
            openJitdump(append);
        }
    }

    ~Perf() {
        if (f) {
            fclose(f);
        }

        if (jit_marker) {
            munmap(jit_marker, 4096);
        }

        if (jit_fd >= 0) {
            int old_fd = jit_fd;
            jit_fd = -1;
            FD::unprotectAndClose(old_fd);
        }
    }

    void addToFile(unsigned long address, unsigned long size, const std::string& symbol) {
        if (f) {
            std::string full = fmt::format("{:x} {:x} {}\n", address, size, symbol);
            ssize_t written = syscall(SYS_write, fd, full.data(), full.size());
            ASSERT_MSG(written == full.size(), "%zu != %zu (errno: %d)", written, full.size(), errno);
        }
    }

    void addCode(u64 address, u64 size, const std::string& symbol) {
        if (jit_fd < 0) {
            return;
        }

        JitCodeLoad record{};
        record.id = 0;
        record.total_size = sizeof(record) + symbol.size() + 1 + size;
        record.timestamp = timestamp();
        record.pid = getpid();
        record.tid = gettid();
        record.vma = address;
        record.code_addr = address;
        record.code_size = size;
        record.code_index = code_index.fetch_add(1, std::memory_order_relaxed);

        iovec iov[3] = {
            {&record, sizeof(record)},
            {(void*)symbol.c_str(), symbol.size() + 1},
            {(void*)address, size},
        };
        ssize_t written = syscall(SYS_writev, jit_fd, iov, 3);
        if (written != (ssize_t)record.total_size) {
            WARN_ONCE("Failed to write perf jitdump record (%zd != %u, errno: %d)", written, record.total_size, errno);
        }
    }

private:
    struct JitHeader {
        u32 magic;
        u32 version;
        u32 total_size;
        u32 elf_mach;
        u32 pad1;
        u32 pid;
        u64 timestamp;
        u64 flags;
    };
    static_assert(sizeof(JitHeader) == 40);

    struct JitCodeLoad {
        u32 id;
        u32 total_size;
        u64 timestamp;
        u32 pid;
        u32 tid;
        u64 vma;
        u64 code_addr;
        u64 code_size;
        u64 code_index;
    };
    static_assert(sizeof(JitCodeLoad) == 56);

    static u64 timestamp() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (u64)ts.tv_sec * 1'000'000'000ull + (u64)ts.tv_nsec;
    }

    void openJitdump(bool append) {
        std::string path = "/tmp/jit-" + std::to_string(getpid()) + ".dump";
        int new_fd = open(path.c_str(), O_RDWR | O_CREAT | O_APPEND | (append ? 0 : O_TRUNC), 0600);
        if (new_fd < 0) {
            WARN("Failed to open perf jitdump %s (errno: %d)", path.c_str(), errno);
            return;
        }
        new_fd = FD::moveToHighNumber(new_fd);
        FD::protect(new_fd);

        struct stat st;
        if (fstat(new_fd, &st) != 0 || st.st_size == 0) {
            JitHeader header{};
            header.magic = 0x4A695444;
            header.version = 1;
            header.total_size = sizeof(header);
            header.elf_mach = EM_RISCV;
            header.pid = getpid();
            header.timestamp = timestamp();
            ssize_t written = syscall(SYS_write, new_fd, &header, sizeof(header));
            if (written != sizeof(header)) {
                WARN("Failed to write perf jitdump header (errno: %d)", errno);
                FD::unprotectAndClose(new_fd);
                return;
            }
        }

        void* marker = mmap(nullptr, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE, new_fd, 0);
        if (marker == MAP_FAILED) {
            WARN("Failed to map perf jitdump %s (errno: %d)", path.c_str(), errno);
        } else {
            jit_marker = marker;
        }

        jit_fd = new_fd;
    }

    FILE* f = nullptr;
    int fd = -1;

    int jit_fd = -1;
    void* jit_marker = nullptr;
    std::atomic<u64> code_index{0};
};
