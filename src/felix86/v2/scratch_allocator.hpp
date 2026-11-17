#pragma once

#include <algorithm>
#include <array>
#include "biscuit/registers.hpp"
#include "felix86/common/log.hpp"

struct ScratchAllocator {
    constexpr static std::array gprs = {
        biscuit::x1, biscuit::x6, biscuit::x7, biscuit::x28, biscuit::x29, biscuit::x30, biscuit::x31,
    };

    // TODO: is the below comment still true? make it not true
    // TODO: to remove "if changed" comment, go to places that regs are hardcoded and add static asserts that they are scratches
    // TODO: For better or for worst (definitely for worst) we rely on the fact that we start with an even
    // register and go sequentially like this
    // This has to do with the fact we want even registers sometimes so widening operations can use
    // the register group. In the future with a proper allocator we can make it so the order here doesn't
    // matter and the order picks an available group.
    constexpr static std::array vecs = {biscuit::v2, biscuit::v3, biscuit::v4, biscuit::v5, biscuit::v6, biscuit::v7, biscuit::v1};

    constexpr static std::array fprs = {biscuit::ft8, biscuit::ft9, biscuit::ft10, biscuit::ft11};

    constexpr static bool isScratch(biscuit::GPR reg) {
        return std::find(gprs.begin(), gprs.end(), reg) != gprs.end();
    }

    bool hasScratch() const {
        return gpr_index != (int)gprs.size();
    }

    biscuit::GPR scratch() {
        ASSERT(hasScratch());
        return gprs[gpr_index++];
    }

    biscuit::GPR popScratch() {
        gpr_index--;
        ASSERT(gpr_index >= 0);
        return gprs[gpr_index];
    }

    biscuit::Vec scratchVec() {
        ASSERT(vec_index != (int)vecs.size());
        return vecs[vec_index++];
    }

    biscuit::Vec scratchVecM2() {
        ASSERT(vec_index != (int)vecs.size());
        if (vecs[vec_index].Index() % 2 != 0) {
            vec_index++;
        }
        ASSERT(vec_index != (int)vecs.size());
        biscuit::Vec ret = vecs[vec_index++];
        ASSERT(vec_index != (int)vecs.size());
        ASSERT(ret.Index() % 2 == 0);
        vec_index++;
        return ret;
    }

    void popScratchVec() {
        vec_index--;
        ASSERT(vec_index >= 0);
    }

    biscuit::FPR scratchFPR() {
        ASSERT(fpr_index != (int)fprs.size());
        return fprs[fpr_index++];
    }

    void popScratchFPR() {
        fpr_index--;
        ASSERT(fpr_index >= 0);
    }

    void resetScratch() {
        gpr_index = 0;
        vec_index = 0;
        fpr_index = 0;
    }

private:
    int gpr_index = 0;
    int vec_index = 0;
    int fpr_index = 0;
};
