#pragma once

#include <list>
#include <vector>
#include "felix86/ir/block.h"

struct ir_ssa_block_t
{
    ir_block_t* actual_block = nullptr;
    std::vector<ir_ssa_block_t*> predecessors = {};
    std::vector<ir_ssa_block_t*> dominance_frontiers = {};
    ir_ssa_block_t* immediate_dominator = nullptr;

    std::list<ir_instruction_t> instructions = {};
    std::vector<ir_instruction_t*> phi_instructions = {};
    ir_ssa_block_t* successor1 = nullptr;
    ir_ssa_block_t* successor2 = nullptr;
    bool visited = false;
    int list_index = 0;
    int postorder_index = 0;
};
