
#include "biscuit/assembler.hpp"
#include "felix86/common/config.hpp"

struct OptimizationGuard {
    OptimizationGuard(biscuit::Assembler& as, int& counter) : as(as), counter(counter) {
        if (g_config.auto_compress) {
            as.DisableOptimization(biscuit::Optimization::AutoCompress);
            counter++;
        }
    }

    ~OptimizationGuard() {
        if (g_config.auto_compress) {
            counter--;
            if (counter == 0) {
                as.EnableOptimization(biscuit::Optimization::AutoCompress);
            }
        }
    }

private:
    biscuit::Assembler& as;
    int& counter; // only used so we can guard recursively, doesn't need to be atomic
};
