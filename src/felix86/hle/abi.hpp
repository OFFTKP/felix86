#pragma once

#include <string>
#include <biscuit/assembler.hpp>

struct ABIMarshaller {
    ABIMarshaller(const std::string& signature);
    void emitPrologue(biscuit::Assembler& as);
    void emitEpilogue(biscuit::Assembler& as);

private:
    std::string signature;
    int stack_size;
};