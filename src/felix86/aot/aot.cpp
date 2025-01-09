#include "felix86/aot/aot.hpp"
#include "felix86/common/disk_cache.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/progress.hpp"
#include "felix86/emulator.hpp"
#include "felix86/frontend/frontend.hpp"

AOT::AOT(Emulator& emulator, std::shared_ptr<Elf> elf) : emulator(emulator), elf(elf) {}

struct CompilerPool {
    CompilerPool(Emulator& emulator, ProgressBar& bar, std::unordered_set<u64>& addresses) : emulator(emulator), bar(bar), addresses(addresses) {}

    void Run() {
        std::vector<std::thread> threads;
        u32 concurrency = std::thread::hardware_concurrency();
        if (concurrency == 0) {
            concurrency = 1;
        }
        for (u32 i = 0; i < concurrency; i++) {
            threads.push_back(std::thread(&CompilerPool::threadLoop, this));
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

private:
    void threadLoop() {
        while (true) {
            u64 address;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (addresses.empty()) {
                    return;
                }

                address = *addresses.begin();
                addresses.erase(address);
            }

            Emulator::CompileFunction(&emulator, address);
            bar.tick();
        }
    }

    Emulator& emulator;
    ProgressBar& bar;
    std::unordered_set<u64>& addresses;
    std::mutex mutex;
};

void AOT::CompileAll() {
    runAnalysis();

    bool quiet = g_quiet;
    g_quiet = true;
    ProgressBar bar("Compiling functions", addresses.size());
    CompilerPool pool(emulator, bar, addresses);
    pool.Run();
    bar.completed();
    g_quiet = quiet;
}

void AOT::PreloadAll() {
    runAnalysis();

    for (u64 address : addresses) {
        IRFunction function(address);
        frontend_compile_function(function);

        Hash hash = function.GetHash();
        std::string hex_hash = hash.ToString();

        if (DiskCache::Has(hex_hash)) {
            emulator.LoadFromCache(address, hex_hash);
        }
    }
}

void AOT::runAnalysis() {
    if (analyzed) {
        return;
    }

    controlFlowAnalysis();
    functionStartFinder();
    analyzed = true;

    LOG("AOT: Found %lu functions", addresses.size());
}