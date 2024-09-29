#include <stdlib.h>
#include "felix86/emulator.hpp"

// felix86_exit_reason_e felix86_recompiler_run(felix86_recompiler_t* recompiler) {
//     if (!recompiler->use_interpreter) {
//         ERROR("Interpreter not enabled");
//     }

//     while (true) {
//         u64 address = recompiler->state.rip;
//         IRFunction* function = ir_function_cache_get_function(recompiler->function_cache, address);

//         if (!function->IsCompiled()) {
//             frontend_compile_function(function, address);
//             ir_ssa_pass(function);

//             if (recompiler->print_blocks)
//                 ir_print_function_graphviz(function);
//         }

//         ir_interpret_function(recompiler, function, &recompiler->state);

//         if (recompiler->testing)
//             break;
//     }

//     return DoneTesting;
// }
