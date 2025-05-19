#include "felix86/common/log.hpp"
#include "felix86/common/pe.hpp"

struct CoffHeader {
    u16 Machine;
    u16 NumberOfSections;
    u32 TimeDateStamp;
    u32 PointerToSymbolTable;
    u32 NumberOfSymbols;
    u16 SizeOfOptionalHeader;
    u16 Characteristics;
};

static_assert(sizeof(CoffHeader) == 20);

constexpr static u16 IMAGE_FILE_MACHINE_I386 = 0x14c;
constexpr static u16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;

PE::PeekResult PE::Peek(const std::filesystem::path& path) {
    FILE* file = fopen(path.c_str(), "r");

    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
        return PeekResult::NotPE;
    }

    size_t size;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0x40) {
        fclose(file);
        return PeekResult::NotPE;
    }

    u8 msdos_stub[0x40];
    int result = fread(msdos_stub, sizeof(msdos_stub), 1, file);

    if (result != 1 || msdos_stub[0] != 'M' || msdos_stub[1] != 'Z') {
        fclose(file);
        return PeekResult::NotPE;
    }

    u32* pe_offset_ptr = (u32*)&msdos_stub[0x3c];
    u32 pe_offset = *pe_offset_ptr;

    if (size < pe_offset + 4 + sizeof(CoffHeader)) {
        fclose(file);
        return PeekResult::NotPE;
    }

    u32 pe_signature;
    fseek(file, pe_offset, SEEK_SET);
    result = fread(&pe_signature, sizeof(u32), 1, file);

    if (result != 1 || pe_signature != 0x00004550) {
        fclose(file);
        return PeekResult::NotPE;
    }

    CoffHeader coff;
    result = fread(&coff, sizeof(CoffHeader), 1, file);
    fclose(file);

    if (result != 1) {
        return PeekResult::NotPE;
    }

    switch (coff.Machine) {
    case IMAGE_FILE_MACHINE_I386: {
        return PeekResult::PE_i386;
    }
    case IMAGE_FILE_MACHINE_AMD64: {
        return PeekResult::PE_x64;
    }
    default: {
        return PeekResult::NotPE;
    }
    }
}