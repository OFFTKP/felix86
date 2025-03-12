#ifndef _RV64_PRINTER_H_
#define _RV64_PRINTER_H_
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* rv64_print(uint32_t data, uint64_t addr);

#ifdef __cplusplus
}
#endif

#endif //_RV64_PRINTER_H_
