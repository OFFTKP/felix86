#include <deque>
#include <unordered_set>
#include "felix86/ir/passes/passes.hpp"

static bool should_consider(const BackendInstruction* inst, bool is_vec) {
    if (is_vec && inst->GetDesiredType() == AllocationType::Vec) {
        return true;
    }

    if (!is_vec && inst->GetDesiredType() == AllocationType::GPR) {
        return true;
    }

    return false;
}

static bool should_consider_op(const BackendInstruction* inst, u8 index, bool is_vec) {
    if (is_vec && inst->GetOperandDesiredType(index) == AllocationType::Vec) {
        return true;
    }

    if (!is_vec && inst->GetOperandDesiredType(index) == AllocationType::GPR) {
        return true;
    }

    return false;
}

static void liveness_worklist2(BackendFunction& function, std::vector<const BackendBlock*> blocks, std::vector<std::unordered_set<u32>>& in,
                               std::vector<std::unordered_set<u32>>& out, const std::vector<std::unordered_set<u32>>& use,
                               const std::vector<std::unordered_set<u32>>& def) {
    std::deque<u32> worklist;
    for (u32 i = 0; i < blocks.size(); i++) {
        worklist.push_back(blocks[i]->GetIndex());
    }

    while (!worklist.empty()) {
        const BackendBlock* block = &function.GetBlock(worklist.front());
        worklist.pop_front();

        size_t i = block->GetIndex();
        auto in_old = in[i];

        out[i].clear();
        // out[b] = U (in[s]) for all s in succ[b]
        for (u8 k = 0; k < block->GetSuccessorCount(); k++) {
            u32 succ_index = block->GetSuccessor(k)->GetIndex();
            for (u32 item : in[succ_index]) {
                out[i].insert(item);
            }
        }

        in[i].clear();
        // in[b] = use[b] U (out[b] - def[b])
        for (u32 item : out[i]) {
            if (!def[i].contains(item)) {
                in[i].insert(item);
            }
        }

        for (u32 item : use[i]) {
            in[i].insert(item);
        }

        if (in[i] != in_old) {
            for (u8 k = 0; k < block->GetPredecessorCount(); k++) {
                u32 pred_index = block->GetPredecessor(k)->GetIndex();
                if (std::find(worklist.begin(), worklist.end(), pred_index) == worklist.end()) {
                    worklist.push_back(pred_index);
                }
            }
        }
    }
}

AllocationMap run(BackendFunction& function, std::vector<const BackendBlock*> blocks, std::vector<u32>& available_colors, bool is_vec,
                  u32& spill_location) {
    AllocationMap allocations;

    std::vector<std::unordered_set<u32>> in(blocks.size());
    std::vector<std::unordered_set<u32>> out(blocks.size());
    std::vector<std::unordered_set<u32>> use(blocks.size());
    std::vector<std::unordered_set<u32>> def(blocks.size());

    struct LiveInterval {
        u32 start = UINT32_MAX;
        u32 end = 0;
        u32 register_id = UINT32_MAX;
        u32 spill_location = 0;
        u32 name = 0;
        bool spilled = false;
    };
    std::vector<LiveInterval> sorted_intervals;

    // Get intervals then put them in sorted_intervals
    {
        std::unordered_map<u32, LiveInterval> intervals;

        for (size_t counter = 0; counter < blocks.size(); counter++) {
            const BackendBlock* block = blocks[counter];
            size_t i = block->GetIndex();
            for (const BackendInstruction& inst : block->GetInstructions()) {
                for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                    if (should_consider_op(&inst, j, is_vec) && !def[i].contains(inst.GetOperand(j))) {
                        // Not defined in this block ie. upwards exposed, live range goes outside current block
                        use[i].insert(inst.GetOperand(j));
                    }
                }

                if (should_consider(&inst, is_vec)) {
                    def[i].insert(inst.GetName());
                }
            }
        }

        liveness_worklist2(function, blocks, in, out, use, def);

        u32 position = 0;
        for (size_t counter = 0; counter < blocks.size(); counter++) {
            const BackendBlock* block = blocks[counter];
            size_t i = block->GetIndex();

            for (u32 input : in[i]) {
                intervals[input].start = std::min(intervals[input].start, position);
                intervals[input].end = std::max(intervals[input].end, position);
            }

            for (const BackendInstruction& inst : block->GetInstructions()) {
                for (u8 j = 0; j < inst.GetOperandCount(); j++) {
                    if (should_consider_op(&inst, j, is_vec)) {
                        intervals[inst.GetOperand(j)].start = std::min(intervals[inst.GetOperand(j)].start, position);
                        intervals[inst.GetOperand(j)].end = std::max(intervals[inst.GetOperand(j)].end, position);
                    }
                }

                if (should_consider(&inst, is_vec)) {
                    intervals[inst.GetName()].start = std::min(intervals[inst.GetName()].start, position);
                    intervals[inst.GetName()].end = std::max(intervals[inst.GetName()].end, position);
                }

                position += 1;
            }

            for (u32 output : out[i]) {
                intervals[output].start = std::min(intervals[output].start, position);
                intervals[output].end = std::max(intervals[output].end, position);
            }
        }

        // Sort them based on start position
        for (const auto& [id, interval] : intervals) {
            sorted_intervals.push_back(interval);
            sorted_intervals.back().name = id;
        }
        std::sort(sorted_intervals.begin(), sorted_intervals.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    }

    std::list<LiveInterval*> active_intervals;

    auto add_to_active = [&](LiveInterval& intr) {
        for (auto it = active_intervals.begin(); it != active_intervals.end(); it++) {
            if ((*it)->end > intr.end) {
                active_intervals.insert(it, &intr);
                return;
            }
        }

        active_intervals.push_back(&intr);
    };

    auto expire_old_intervals = [&](LiveInterval& intr) {
        auto it = active_intervals.begin();
        while (it != active_intervals.end()) {
            if ((*it)->end >= intr.start) {
                return;
            }

            ASSERT((*it)->register_id != UINT32_MAX);
            available_colors.push_back((*it)->register_id);
            it = active_intervals.erase(it);
        }
    };

    auto spill_at_interval = [&](LiveInterval& intr) {
        ASSERT(!active_intervals.empty());
        LiveInterval* spill_interval = active_intervals.back();

        if (spill_interval->end > intr.start) {
            intr.register_id = spill_interval->register_id;
            spill_interval->register_id = UINT32_MAX;
            spill_interval->spilled = true;
            spill_interval->spill_location = spill_location;
            spill_location += is_vec ? 16 : 8;
            active_intervals.pop_back();
            add_to_active(intr);
        } else {
            intr.spilled = true;
            intr.spill_location = spill_location;
        }
    };

    for (auto& interval : sorted_intervals) {
        expire_old_intervals(interval);
        if (available_colors.empty()) {
            spill_at_interval(interval);
        } else {
            interval.register_id = available_colors.back();
            available_colors.pop_back();
            add_to_active(interval);
        }
    }

    for (auto& interval : sorted_intervals) {
        if (!interval.spilled) {
            if (is_vec) {
                allocations.Allocate(interval.name, AllocationType::Vec, interval.register_id);
            } else {
                allocations.Allocate(interval.name, AllocationType::GPR, interval.register_id);
            }
        } else {
            if (is_vec) {
                allocations.Spill(interval.name, AllocationType::StaticSpillVec, interval.spill_location);
            } else {
                allocations.Spill(interval.name, AllocationType::StaticSpillGPR, interval.spill_location);
            }
        }
    }

    return allocations;
}

AllocationMap ir_linear_scan_pass(BackendFunction& function) {
    std::vector<const BackendBlock*> blocks = function.GetBlocksPostorder();

    g_spilled_count = 0;

    std::vector<u32> available_gprs = Registers::GetAllocatableGPRsLinear();
    std::vector<u32> available_vecs = Registers::GetAllocatableVecsLinear();

    u32 spill_location = 0;

    AllocationMap gpr_map = run(function, blocks, available_gprs, false, spill_location);
    AllocationMap vec_map = run(function, blocks, available_vecs, true, spill_location);

    AllocationMap allocations;

    for (auto& [name, allocation] : gpr_map) {
        if (allocation.GetAllocationType() == AllocationType::GPR) {
            allocations.Allocate(name, biscuit::GPR(allocation));
        } else if (allocation.GetAllocationType() == AllocationType::StaticSpillGPR) {
            allocations.Spill(name, AllocationType::StaticSpillGPR, allocation.GetSpillLocation());
        } else {
            UNREACHABLE();
        }
    }

    for (auto& [name, allocation] : vec_map) {
        if (allocation.GetAllocationType() == AllocationType::Vec) {
            allocations.Allocate(name, biscuit::Vec(allocation));
        } else if (allocation.GetAllocationType() == AllocationType::StaticSpillVec) {
            allocations.Spill(name, AllocationType::StaticSpillVec, allocation.GetSpillLocation());
        } else {
            UNREACHABLE();
        }
    }

    for (BackendBlock* block : function.GetBlocks()) {
        for (BackendInstruction& inst : block->GetInstructions()) {
            if (inst.GetOpcode() == IROpcode::GetThreadStatePointer) {
                allocations.Allocate(inst.GetName(), Registers::ThreadStatePointer());
            } else if (inst.GetOpcode() == IROpcode::Immediate && inst.GetImmediateData() == 0) {
                allocations.Allocate(inst.GetName(), Registers::Zero());
            }
        }
    }

    allocations.SetSpillSize(spill_location);

    VERBOSE("Register allocation done");

    return allocations;
}