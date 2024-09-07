#include "felix86/felix86.h"
#include "felix86/common/global.h"
#include "felix86/common/log.h"
#include "felix86/common/state.h"
#include "felix86/frontend/frontend.h"
#include "felix86/ir/function_cache.h"
#include "felix86/ir/passes.h"
#include "felix86/ir/print.h"
#include "felix86/ir/interpreter.h"
#include <stdlib.h>

struct felix86_recompiler_s {
    ir_function_cache_t* function_cache;
    x86_state_t state;
    bool testing;
    bool optimize;
    bool print_blocks;
    bool use_interpreter;
    u64 base_address;
};

felix86_recompiler_t* felix86_recompiler_create(felix86_recompiler_config_t* config) {
    felix86_recompiler_t* recompiler = calloc(1, sizeof(felix86_recompiler_t));
    recompiler->function_cache = ir_function_cache_create();
    recompiler->testing = config->testing;
    recompiler->optimize = config->optimize;
    recompiler->print_blocks = config->print_blocks;
    recompiler->base_address = config->base_address;
    recompiler->use_interpreter = config->use_interpreter;

    g_base_address = recompiler->base_address;

    return recompiler;
}

void felix86_recompiler_destroy(felix86_recompiler_t* recompiler) {
    // ir_function_cache_destroy(recompiler->function_cache);
    free(recompiler);
}

u64 felix86_get_guest(felix86_recompiler_t* recompiler, x86_ref_e ref) {
    switch (ref) {
        case X86_REF_RAX:
            return recompiler->state.gprs[0];
        case X86_REF_RCX:
            return recompiler->state.gprs[1];
        case X86_REF_RDX:
            return recompiler->state.gprs[2];
        case X86_REF_RBX:
            return recompiler->state.gprs[3];
        case X86_REF_RSP:
            return recompiler->state.gprs[4];
        case X86_REF_RBP:
            return recompiler->state.gprs[5];
        case X86_REF_RSI:
            return recompiler->state.gprs[6];
        case X86_REF_RDI:
            return recompiler->state.gprs[7];
        case X86_REF_R8:
            return recompiler->state.gprs[8];
        case X86_REF_R9:
            return recompiler->state.gprs[9];
        case X86_REF_R10:
            return recompiler->state.gprs[10];
        case X86_REF_R11:
            return recompiler->state.gprs[11];
        case X86_REF_R12:
            return recompiler->state.gprs[12];
        case X86_REF_R13:
            return recompiler->state.gprs[13];
        case X86_REF_R14:
            return recompiler->state.gprs[14];
        case X86_REF_R15:
            return recompiler->state.gprs[15];
        case X86_REF_RIP:
            return recompiler->state.rip;
        case X86_REF_GS:
            return recompiler->state.gs;
        case X86_REF_FS:
            return recompiler->state.fs;
        case X86_REF_CF:
            return recompiler->state.cf;
        case X86_REF_PF:
            return recompiler->state.pf;
        case X86_REF_AF:
            return recompiler->state.af;
        case X86_REF_ZF:
            return recompiler->state.zf;
        case X86_REF_SF:
            return recompiler->state.sf;
        case X86_REF_OF:
            return recompiler->state.of;
        default:
            ERROR("Invalid GPR reference");
    }
}

void felix86_set_guest(felix86_recompiler_t* recompiler, x86_ref_e ref, u64 value) {
    switch (ref) {
        case X86_REF_RAX:
            recompiler->state.gprs[0] = value;
            break;
        case X86_REF_RCX:
            recompiler->state.gprs[1] = value;
            break;
        case X86_REF_RDX:
            recompiler->state.gprs[2] = value;
            break;
        case X86_REF_RBX:
            recompiler->state.gprs[3] = value;
            break;
        case X86_REF_RSP:
            recompiler->state.gprs[4] = value;
            break;
        case X86_REF_RBP:
            recompiler->state.gprs[5] = value;
            break;
        case X86_REF_RSI:
            recompiler->state.gprs[6] = value;
            break;
        case X86_REF_RDI:
            recompiler->state.gprs[7] = value;
            break;
        case X86_REF_R8:
            recompiler->state.gprs[8] = value;
            break;
        case X86_REF_R9:
            recompiler->state.gprs[9] = value;
            break;
        case X86_REF_R10:
            recompiler->state.gprs[10] = value;
            break;
        case X86_REF_R11:
            recompiler->state.gprs[11] = value;
            break;
        case X86_REF_R12:
            recompiler->state.gprs[12] = value;
            break;
        case X86_REF_R13:
            recompiler->state.gprs[13] = value;
            break;
        case X86_REF_R14:
            recompiler->state.gprs[14] = value;
            break;
        case X86_REF_R15:
            recompiler->state.gprs[15] = value;
            break;
        case X86_REF_RIP:
            recompiler->state.rip = value;
            break;
        case X86_REF_GS:
            recompiler->state.gs = value;
            break;
        case X86_REF_FS:
            recompiler->state.fs = value;
            break;
        case X86_REF_CF:
            recompiler->state.cf = value;
            break;
        case X86_REF_PF:
            recompiler->state.pf = value;
            break;
        case X86_REF_AF:
            recompiler->state.af = value;
            break;
        case X86_REF_ZF:
            recompiler->state.zf = value;
            break;
        case X86_REF_SF:
            recompiler->state.sf = value;
            break;
        case X86_REF_OF:
            recompiler->state.of = value;
            break;
        default:
            ERROR("Invalid GPR reference");
    }
}

xmm_reg_t felix86_get_guest_xmm(felix86_recompiler_t* recompiler, x86_ref_e ref) {
    if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
        ERROR("Invalid XMM reference");
    }

    return recompiler->state.xmm[ref - X86_REF_XMM0];
}

void felix86_set_guest_xmm(felix86_recompiler_t* recompiler, x86_ref_e ref, xmm_reg_t value) {
    if (ref < X86_REF_XMM0 || ref > X86_REF_XMM15) {
        ERROR("Invalid XMM reference");
    }

    recompiler->state.xmm[ref - X86_REF_XMM0] = value;
}

felix86_exit_reason_e felix86_recompiler_run(felix86_recompiler_t* recompiler) {
    if (!recompiler->use_interpreter) {
        ERROR("Interpreter not enabled");
    }

    while (true) {
        u64 address = recompiler->state.rip;
        ir_function_t* function = ir_function_cache_get_function(recompiler->function_cache, address);

        if (!function->compiled) {
            frontend_compile_function(function);
            ir_naming_pass(function);

            if (recompiler->print_blocks)
                ir_print_function_graphviz(recompiler->base_address, function);
        }

        ir_interpret_function(function, &recompiler->state);

        if (recompiler->testing)
            break;
    }

    return DoneTesting;
}

ir_function_t* felix86_get_function(felix86_recompiler_t* recompiler, u64 address) {
    return ir_function_cache_get_function(recompiler->function_cache, address);
}