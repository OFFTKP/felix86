#include <cstddef>
#include <cstdio>
#include <vector>
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/pe.hpp"
#include "felix86/common/volatile.hpp"

// Code relating to VolatileMetadata was inspired by this code https://github.com/saferwall/pe/blob/main/loadconfig.go

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

struct DataDirectory {
    u32 VirtualAddress;
    u32 Size;
};

struct OptionalHeader32 {
    u16 Magic;
    u8 MajorLinkerVersion;
    u8 MinorLinkerVersion;
    u32 SizeOfCode;
    u32 SizeOfInitializedData;
    u32 SizeOfUninitializedData;
    u32 AddressOfEntryPoint;
    u32 BaseOfCode;
    u32 BaseOfData;
    u32 ImageBase;
    u32 SectionAlignment;
    u32 FileAlignment;
    u16 MajorOperatingSystemVersion;
    u16 MinorOperatingSystemVersion;
    u16 MajorImageVersion;
    u16 MinorImageVersion;
    u16 MajorSubsystemVersion;
    u16 MinorSubsystemVersion;
    u32 Win32VersionValue;
    u32 SizeOfImage;
    u32 SizeOfHeaders;
    u32 CheckSum;
    u16 Subsystem;
    u16 DllCharacteristics;
    u32 SizeOfStackReserve;
    u32 SizeOfStackCommit;
    u32 SizeOfHeapReserve;
    u32 SizeOfHeapCommit;
    u32 LoaderFlags;
    u32 NumberOfRvaAndSizes;
    DataDirectory Directory[16];
};

struct OptionalHeader64 {
    u16 Magic;
    u8 MajorLinkerVersion;
    u8 MinorLinkerVersion;
    u32 SizeOfCode;
    u32 SizeOfInitializedData;
    u32 SizeOfUninitializedData;
    u32 AddressOfEntryPoint;
    u32 BaseOfCode;
    u64 ImageBase;
    u32 SectionAlignment;
    u32 FileAlignment;
    u16 MajorOperatingSystemVersion;
    u16 MinorOperatingSystemVersion;
    u16 MajorImageVersion;
    u16 MinorImageVersion;
    u16 MajorSubsystemVersion;
    u16 MinorSubsystemVersion;
    u32 Win32VersionValue;
    u32 SizeOfImage;
    u32 SizeOfHeaders;
    u32 CheckSum;
    u16 Subsystem;
    u16 DllCharacteristics;
    u64 SizeOfStackReserve;
    u64 SizeOfStackCommit;
    u64 SizeOfHeapReserve;
    u64 SizeOfHeapCommit;
    u32 LoaderFlags;
    u32 NumberOfRvaAndSizes;
    DataDirectory Directory[16];
};

struct SectionHeader {
    u8 Name[8];
    union {
        u32 PhysicalAddress;
        u32 VirtualSize;
    };
    u32 VirtualAddress;
    u32 SizeOfRawData;
    u32 PointerToRawData;
    u32 PointerToRelocations;
    u32 PointerToLinenumbers;
    u16 NumberOfRelocations;
    u16 NumberOfLinenumbers;
    u32 Characteristics;
};

// The minimum amount of size necessary for volatime metadata pointer.
// More fields could exist trailing the end of this structure.
struct LoadConfig32 {
    u32 Size;
    u32 TimeDateStamp;
    u16 MajorVersion;
    u16 MinorVersion;
    u32 GlobalFlagsClear;
    u32 GlobalFlagsSet;
    u32 CriticalSectionDefaultTimeout;
    u32 DeCommitFreeBlockThreshold;
    u32 DeCommitTotalFreeThreshold;
    u32 LockPrefixTable;
    u32 MaximumAllocationSize;
    u32 VirtualMemoryThreshold;
    u32 DynamicValueRelocTable;
    u32 CHPEMetadataPointer;
    u32 GuardRFFailureRoutine;
    u32 GuardRFFailureRoutineFunctionPointer;
    u32 DynamicValueRelocTableOffset;
    u16 DynamicValueRelocTableSection;
    u16 Reserved2;
    u32 GuardRFVerifyStackPointerFunctionPointer;
    u32 HotPatchTableOffset;
    u32 Reserved3;
    u32 EnclaveConfigurationPointer;
    u32 VolatileMetadataPointer;
};

// The minimum amount of size necessary for volatime metadata pointer.
// More fields could exist trailing the end of this structure.
struct LoadConfig64 {
    u32 Size;
    u32 TimeDateStamp;
    u16 MajorVersion;
    u16 MinorVersion;
    u32 GlobalFlagsClear;
    u32 GlobalFlagsSet;
    u32 CriticalSectionDefaultTimeout;
    u64 DeCommitFreeBlockThreshold;
    u64 DeCommitTotalFreeThreshold;
    u64 LockPrefixTable;
    u64 MaximumAllocationSize;
    u64 VirtualMemoryThreshold;
    u64 DynamicValueRelocTable;
    u64 CHPEMetadataPointer;
    u64 GuardRFFailureRoutine;
    u64 GuardRFFailureRoutineFunctionPointer;
    u32 DynamicValueRelocTableOffset;
    u16 DynamicValueRelocTableSection;
    u16 Reserved2;
    u64 GuardRFVerifyStackPointerFunctionPointer;
    u32 HotPatchTableOffset;
    u32 Reserved3;
    u64 EnclaveConfigurationPointer;
    u64 VolatileMetadataPointer;
};

struct VolatileData {
    u32 Size;
    u32 Version;
    u32 VolatileAccessTable;
    u32 VolatileAccessTableSize;
    u32 VolatileInfoRangeTable;
    u32 VolatileInfoRangeTableSize;
};

struct Range {
    u32 Rva;
    u32 Size;
};

constexpr static u16 IMAGE_FILE_MACHINE_I386 = 0x14c;
constexpr static u16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;

constexpr static u16 IMAGE_OPTIONAL_MAGIC_I386 = 0x10b;
constexpr static u16 IMAGE_OPTIONAL_MAGIC_AMD64 = 0x20b;
constexpr static u16 IMAGE_OPTIONAL_LOAD_CONFIG = 10;
constexpr static u16 IMAGE_MAX_SECTIONS = 96;

FILE* CheckedOpenPE(const std::filesystem::path& path, CoffHeader& coff) {
    FILE* file = fopen(path.c_str(), "r");

    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
        return nullptr;
    }

    size_t size;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0x40) {
        fclose(file);
        return nullptr;
    }

    u8 msdos_stub[0x40];
    int result = fread(msdos_stub, sizeof(msdos_stub), 1, file);

    if (result != 1 || msdos_stub[0] != 'M' || msdos_stub[1] != 'Z') {
        fclose(file);
        return nullptr;
    }

    u32* pe_offset_ptr = (u32*)&msdos_stub[0x3c];
    u32 pe_offset = *pe_offset_ptr;

    if (size < pe_offset + 4 + sizeof(CoffHeader)) {
        fclose(file);
        return nullptr;
    }

    u32 pe_signature;
    fseek(file, pe_offset, SEEK_SET);
    result = fread(&pe_signature, sizeof(u32), 1, file);

    if (result != 1 || pe_signature != 0x00004550) {
        fclose(file);
        return nullptr;
    }

    result = fread(&coff, sizeof(CoffHeader), 1, file);
    if (result != 1) {
        fclose(file);
        return nullptr;
    }

    return file;
}

PE::PeekResult PE::Peek(const std::filesystem::path& path) {

    CoffHeader coff;
    FILE* file = CheckedOpenPE(path, coff);
    fclose(file);
    if (!file) {
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

void PE::RegisterVolatileMemory(const std::filesystem::path& path) {
    CoffHeader coff;
    FILE* file = CheckedOpenPE(path, coff);
    if (!file)
        return;

    u16 magic = 0;
    long cursor = ftell(file);
    if (fread(&magic, sizeof(magic), 1, file) != 1) {
        fclose(file);
        return;
    }
    fseek(file, cursor, SEEK_SET);

    u64 image_base = 0;
    u64 adr = 0;
    u64 end = 0;

    bool is32 = false;

    switch (magic) {
    case IMAGE_OPTIONAL_MAGIC_I386: {
        OptionalHeader32 header;
        if (fread(&header, sizeof(header), 1, file) != 1) {
            fclose(file);
            return;
        }

        image_base = header.ImageBase;
        adr = header.Directory[IMAGE_OPTIONAL_LOAD_CONFIG].VirtualAddress;
        end = adr + header.Directory[IMAGE_OPTIONAL_LOAD_CONFIG].Size;
        is32 = true;
    } break;
    case IMAGE_OPTIONAL_MAGIC_AMD64: {
        OptionalHeader64 header;
        if (fread(&header, sizeof(header), 1, file) != 1) {
            fclose(file);
            return;
        }

        image_base = header.ImageBase;
        adr = header.Directory[IMAGE_OPTIONAL_LOAD_CONFIG].VirtualAddress;
        end = adr + header.Directory[IMAGE_OPTIONAL_LOAD_CONFIG].Size;
    } break;
    default: {
        fclose(file);
        return;
    }
    }

    SectionHeader sections[IMAGE_MAX_SECTIONS];
    if (fread(&sections, sizeof(SectionHeader), coff.NumberOfSections, file) != coff.NumberOfSections) {
        fclose(file);
        return;
    }

    bool found = false;
    u64 volatile_virt_adr = 0;
    for (int s = 0; s < coff.NumberOfSections && !found; ++s) {
        if (adr >= sections[s].VirtualAddress && end <= sections[s].VirtualAddress + sections[s].SizeOfRawData) {
            u64 offset = adr - sections[s].VirtualAddress;
            offset += sections[s].PointerToRawData;

            fseek(file, offset, SEEK_SET);
            cursor = ftell(file);
            u32 size = 0;
            if (fread(&size, sizeof(size), 1, file) != 1) {
                fclose(file);
                return;
            }
            fseek(file, cursor, SEEK_SET);

            if (is32) {
                if (size >= sizeof(LoadConfig32)) {
                    LoadConfig32 config;
                    if (fread(&config, sizeof(LoadConfig32), 1, file) != 1) {
                        fclose(file);
                        return;
                    }

                    found = true;
                    volatile_virt_adr = config.VolatileMetadataPointer;
                } else {
                    fclose(file);
                    return;
                }
            } else {
                if (size >= sizeof(LoadConfig64)) {
                    LoadConfig64 config;
                    if (fread(&config, sizeof(LoadConfig64), 1, file) != 1) {
                        fclose(file);
                        return;
                    }

                    found = true;
                    volatile_virt_adr = config.VolatileMetadataPointer;
                } else {
                    fclose(file);
                    return;
                }
            }
        }
    }

    if (volatile_virt_adr == 0) {
        fclose(file);
        return;
    }

    // Volatime metadata is not stored as RVA. Subtract image base.
    volatile_virt_adr -= image_base;

    for (int s = 0; s < coff.NumberOfSections && !found; ++s) {
        if (volatile_virt_adr >= sections[s].VirtualAddress && volatile_virt_adr < sections[s].VirtualAddress + sections[s].VirtualSize) {
            u64 offset = adr - sections[s].VirtualAddress;
            offset += sections[s].PointerToRawData;

            fseek(file, offset, SEEK_SET);
            u32 size = 0;
            if (fread(&size, sizeof(size), 1, file) != 1) {
                fclose(file);
                return;
            }

            fseek(file, offset, SEEK_SET);
            VolatileData data{};
            if (fread(&data, size, 1, file) != 1) {
                fclose(file);
                return;
            }

            bool found = false;
            for (int s = 0; s < coff.NumberOfSections; ++s) {
                if (data.VolatileInfoRangeTable >= sections[s].VirtualAddress &&
                    data.VolatileInfoRangeTable < sections[s].VirtualAddress + sections[s].SizeOfRawData) {
                    offset = data.VolatileInfoRangeTable - sections[s].VirtualAddress;
                    offset += sections[s].PointerToRawData;
                    found = true;
                }
            }

            if (!found) {
                fclose(file);
                return;
            }

            fseek(file, offset, SEEK_SET);
            for (u32 i = 0; i < data.VolatileInfoRangeTableSize / sizeof(Range); ++i) {
                Range range;
                if (fread(&range, sizeof(Range), 1, file) != 1) {
                    fclose(file);
                    return;
                }

                g_volatile->RegisterRegion(image_base + range.Rva, range.Size);
            }

            fclose(file);
            return;
        }
    }

    fclose(file);
}
