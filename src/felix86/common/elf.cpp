#include <variant>
#include <cxxabi.h>
#include <elf.h>
#include <linux/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include "felix86/common/debug.hpp"
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"

// Not a full ELF implementation, but one that suits our needs as a loader of
// both the executable and the dynamic linker, and one that only supports x86/x86_64
// little-endian

#define PAGE_START(x) ((x) & ~(uintptr_t)(4095))
#define PAGE_OFFSET(x) ((x) & 4095)
#define PAGE_ALIGN(x) (((x) + 4095) & ~(uintptr_t)(4095))

struct Elf_Ehdr {
    Elf_Ehdr(bool mode32, FILE* file) : mode32(mode32) {
        size_t read;
        if (mode32) {
            inner = Elf32_Ehdr{};
            read = fread(&inner32(), sizeof(Elf32_Ehdr), 1, file);
        } else {
            inner = Elf64_Ehdr{};
            read = fread(&inner64(), sizeof(Elf64_Ehdr), 1, file);
        }

        if (read != 1) {
            ERROR("Failed to read ELF header from file");
        }
    }

    u64 version() {
        return mode32 ? inner32().e_version : inner64().e_version;
    }

    u64 machine() {
        return mode32 ? inner32().e_machine : inner64().e_machine;
    }

    u64 entry() {
        return mode32 ? inner32().e_entry : inner64().e_entry;
    }

    u64 type() {
        return mode32 ? inner32().e_type : inner64().e_type;
    }

    u64 phoff() {
        return mode32 ? inner32().e_phoff : inner64().e_phoff;
    }

    u64 phnum() {
        return mode32 ? inner32().e_phnum : inner64().e_phnum;
    }

    u64 phentsize() {
        return mode32 ? inner32().e_phentsize : inner64().e_phentsize;
    }

private:
    bool mode32;

    Elf32_Ehdr& inner32() {
        return std::get<Elf32_Ehdr>(inner);
    }

    Elf64_Ehdr& inner64() {
        return std::get<Elf64_Ehdr>(inner);
    }

    std::variant<Elf64_Ehdr, Elf32_Ehdr> inner;
};

struct Elf_Phdr {
    Elf_Phdr(bool mode32, FILE* file) : mode32(mode32) {
        size_t read;
        if (mode32) {
            inner = Elf32_Phdr{};
            read = fread(&inner32(), sizeof(Elf32_Phdr), 1, file);
        } else {
            inner = Elf64_Phdr{};
            read = fread(&inner64(), sizeof(Elf64_Phdr), 1, file);
        }

        if (read != 1) {
            ERROR("Failed to read ELF program header from file");
        }
    }

    u64 type() {
        return mode32 ? inner32().p_type : inner64().p_type;
    }

    u64 flags() {
        return mode32 ? inner32().p_flags : inner64().p_flags;
    }

    u64 offset() {
        return mode32 ? inner32().p_offset : inner64().p_offset;
    }

    u64 vaddr() {
        return mode32 ? inner32().p_vaddr : inner64().p_vaddr;
    }

    u64 filesz() {
        return mode32 ? inner32().p_filesz : inner64().p_filesz;
    }

    u64 memsz() {
        return mode32 ? inner32().p_memsz : inner64().p_memsz;
    }

private:
    bool mode32;

    Elf32_Phdr& inner32() {
        return std::get<Elf32_Phdr>(inner);
    }

    Elf64_Phdr& inner64() {
        return std::get<Elf64_Phdr>(inner);
    }

    std::variant<Elf64_Phdr, Elf32_Phdr> inner;
};

Elf::Elf(bool is_interpreter) : is_interpreter(is_interpreter) {}

Elf::~Elf() {
    for (auto [addr, size] : unmap_me) {
        munmap(addr, size);
    }
}

void Elf::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        WARN("File %s does not exist", path.c_str());
        return;
    }

    if (!std::filesystem::is_regular_file(path)) {
        WARN("File %s is not a regular file", path.c_str());
        return;
    }

    u64 lowest_vaddr = 0xFFFFFFFFFFFFFFFF;
    u64 highest_vaddr = 0;

    FILE* file = fopen(path.c_str(), "rb");
    int fd = fileno(file);

    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
    }

    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    if (size < sizeof(Elf64_Ehdr)) {
        ERROR("File %s is too small to be an ELF file", path.c_str());
    }
    fseek(file, 0, SEEK_SET);

    // Peek the header to find out if we're 32-bit or 64-bit
    u8 e_ident[EI_NIDENT];
    ssize_t result = fread(&e_ident, EI_NIDENT, sizeof(u8), file);

    if (result != 1) {
        ERROR("Failed to read ELF header from file %s", path.c_str());
    }

    // Check if it's a 32-bit executable
    bool mode32 = e_ident[4] == ELFCLASS32;
    ASSERT(g_mode32 == mode32); // same mode as the one we're configured to execute for

    // Go back to start to read the full header
    fseek(file, 0, SEEK_SET);
    Elf_Ehdr ehdr(mode32, file);

    if (e_ident[0] != 0x7F || e_ident[1] != 'E' || e_ident[2] != 'L' || e_ident[3] != 'F') {
        ERROR("File %s is not an ELF file", path.c_str());
    }

    if (e_ident[4] != ELFCLASS64 && e_ident[4] != ELFCLASS32) {
        ERROR("File %s is not a 64-bit or 32-bit ELF file", path.c_str());
    }

    if (e_ident[5] != ELFDATA2LSB) {
        ERROR("File %s is not a little-endian ELF file", path.c_str());
    }

    if (e_ident[6] != 1 || ehdr.version() != 1) {
        ERROR("File %s has an invalid version", path.c_str());
    }

    if (ehdr.machine() != EM_X86_64 && ehdr.machine() != EM_386) {
        ERROR("File %s is not an x86 or x86_64 ELF file", path.c_str());
    }

    if (ehdr.entry() == 0 && ehdr.type() == ET_EXEC) {
        ERROR("File %s is an executable but has no entry point", path.c_str());
    }

    if (ehdr.phoff() == 0) {
        ERROR("File %s has no program header table, thus has no loadable segments", path.c_str());
    }

    if (ehdr.phnum() == 0xFFFF) {
        ERROR("If the number of program headers is greater than or equal to PN_XNUM "
              "(0xffff) "
              "this member has the value PN_XNUM (0xffff). The actual number of "
              "program header "
              "table entries is contained in the sh_info field of the section "
              "header at index 0");
    }

    if (ehdr.type() != ET_EXEC && ehdr.type() != ET_DYN) {
        ERROR("File %s is not an executable or shared object", path.c_str());
    }

    entry = ehdr.entry();

    u64 expected_phentsize = mode32 ? sizeof(Elf32_Phdr) : sizeof(Elf64_Phdr);
    if (ehdr.phentsize() != expected_phentsize) {
        ERROR("File %s has an invalid program header size: %d", path.c_str(), (int)ehdr.phentsize());
    }

    // Avoiding heap allocations for when I must do that in the future
    Elf_Phdr* phdrtable = (Elf_Phdr*)alloca(ehdr.phnum() * sizeof(Elf_Phdr));
    fseek(file, ehdr.phoff(), SEEK_SET);
    for (Elf64_Half i = 0; i < ehdr.phnum(); i++) {
        // Placement new to run the constructor
        new (&phdrtable[i]) Elf_Phdr(mode32, file);
    }

    for (Elf64_Half i = 0; i < ehdr.phnum(); i++) {
        Elf_Phdr& phdr = phdrtable[i];
        switch (phdr.type()) {
        case PT_INTERP: {
            std::string interpreter_str;
            interpreter_str.resize(phdr.filesz());
            fseek(file, phdr.offset(), SEEK_SET);
            result = fread(interpreter_str.data(), 1, phdr.filesz(), file);
            if (result != phdr.filesz()) {
                ERROR("Failed to read interpreter from file %s", path.c_str());
            }

            interpreter = std::filesystem::path(interpreter_str);
            break;
        }
        case PT_GNU_STACK: {
            if (phdr.flags() & PF_X) {
                WARN("Executable stack");
            }
            break;
        }
        case PT_LOAD: {
            if (phdr.filesz() == 0) {
                break;
            }

            if (phdr.vaddr() + phdr.memsz() > highest_vaddr) {
                highest_vaddr = phdr.vaddr() + phdr.memsz();
            }

            if (phdr.vaddr() < lowest_vaddr) {
                lowest_vaddr = phdr.vaddr();
            }
            break;
        }
        }
    }

    VERBOSE("Highest vaddr: %lx", highest_vaddr);

    u8* base_ptr;
    u64 base_hint = is_interpreter ? g_interpreter_base_hint : g_executable_base_hint;
    if ((base_hint & 0xFFF) != 0) {
        ERROR("Base hint is not page aligned for: %s", is_interpreter ? "Interpreter" : "Executable");
    }

    if (ehdr.type() == ET_DYN) {
        // TODO: fix this hack
        if (mode32 && !is_interpreter) {
            WARN("Setting base hint to 0x100000");
            base_hint = 0x100000 + g_address_space_base;
        } else if (mode32 && is_interpreter) {
            WARN("Setting base hint to 0x2000000");
            base_hint = 0x2000000 + g_address_space_base;
        }

        // In 32-bit mode the 4GiB address space was already allocated at these addresses so we use MAP_FIXED instead of NOREPLACE
        auto fixed_flag = mode32 ? MAP_FIXED : MAP_FIXED_NOREPLACE;
        if (base_hint) {
            base_ptr = (u8*)mmap((u8*)base_hint, highest_vaddr, 0, MAP_PRIVATE | MAP_ANONYMOUS | fixed_flag, -1, 0);
        } else {
            base_ptr = (u8*)mmap(nullptr, highest_vaddr, 0, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }

        unmap_me.push_back({base_ptr, highest_vaddr});
    } else {
        // Start at the address space base. 0 in 64-bit, some address in 32-bit mode.
        base_ptr = (u8*)g_address_space_base;
    }

    if (base_ptr == MAP_FAILED) {
        ERROR("Failed to allocate memory for ELF file %s", path.c_str());
    }

    VERBOSE("Allocated memory at %p-%p", base_ptr, base_ptr + highest_vaddr);

    for (Elf64_Half i = 0; i < ehdr.phnum(); i += 1) {
        Elf_Phdr& phdr = phdrtable[i];
        switch (phdr.type()) {
        case PT_LOAD: {
            if (phdr.filesz() == 0) {
                ERROR("Loadable segment has no data in file %s", path.c_str());
                break;
            }

            VERBOSE("Segment %d: %lx-%lx", i, phdr.vaddr(), phdr.vaddr() + phdr.memsz());

            u8* segment_base = base_ptr + PAGE_START(phdr.vaddr());
            u64 segment_size = phdr.filesz() + PAGE_OFFSET(phdr.vaddr());
            u64 offset = phdr.offset() - PAGE_OFFSET(phdr.vaddr());

            u8 prot = 0;
            if (phdr.flags() & PF_R) {
                prot |= PROT_READ;
            }

            if (phdr.flags() & PF_W) {
                prot |= PROT_WRITE;
            }

            if (phdr.flags() & PF_X) {
                prot |= PROT_EXEC;
            }

            void* addr = mmap((void*)segment_base, segment_size, prot, MAP_PRIVATE | MAP_FIXED, fd, offset);
            VERBOSE("Running mmap(%p, %lx, 0, MAP_PRIVATE | MAP_FIXED, %d, %lx)", (void*)segment_base, segment_size, fd, offset);

            if (addr == MAP_FAILED) {
                ERROR("Failed to allocate memory for segment in file %s. Error: %s", path.c_str(), strerror(errno));
            } else if (addr != (void*)segment_base) {
                ERROR("Failed to allocate memory at requested address for segment in file %s", path.c_str());
            }

            unmap_me.push_back({(void*)segment_base, segment_size});

            if (phdr.memsz() > phdr.filesz()) {
                // This is probably a segment that contains a .data and a .bss right after, so after
                // the file size starts the bss, the part that should be zeroed
                u64 bss_start = (u64)base_ptr + phdr.vaddr() + phdr.filesz();
                u64 bss_page_start = PAGE_ALIGN(bss_start);
                u64 bss_page_end = PAGE_ALIGN((u64)base_ptr + phdr.vaddr() + phdr.memsz());

                if (phdr.flags() & PF_W) {
                    memset((void*)bss_start, 0, bss_page_start - bss_start);
                }

                if (bss_page_start != bss_page_end) {
                    size_t excess_size = bss_page_end - bss_page_start;
                    void* bss_excess = mmap((void*)bss_page_start, excess_size, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
                    if (bss_excess == MAP_FAILED) {
                        ERROR("Failed to allocate memory for BSS in file %s", path.c_str());
                    }

                    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, bss_page_start, bss_page_end - bss_page_start, "bss");
                    memset((void*)bss_page_start, 0, bss_page_end - bss_page_start);
                    VERBOSE("BSS segment at %p-%p", (void*)bss_page_start, (void*)bss_page_end);
                }
            }
            break;
        }
        default: {
            break;
        }
        }
    }

    if (!is_interpreter) {
        // Don't add to unmap_me, unmapped elsewhere
        program_base = base_ptr;
        g_current_brk = (u64)mmap(base_ptr + PAGE_ALIGN(highest_vaddr), brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((void*)g_current_brk == MAP_FAILED) {
            ERROR("Failed to allocate memory for brk in file %s", path.c_str());
        }
        g_initial_brk = g_current_brk;
        g_current_brk_size = brk_size;
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, g_current_brk, brk_size, "brk");
        VERBOSE("BRK base at %p", (void*)g_current_brk);

        g_executable_start = HostAddress{(u64)(base_ptr + lowest_vaddr)};
        g_executable_end = HostAddress{PAGE_ALIGN((u64)(base_ptr + highest_vaddr))};
        MemoryMetadata::AddRegion("Executable", g_executable_start.raw(), g_executable_end.raw());
        // LoadSymbols("Executable", path, (void*)g_executable_start);
    } else {
        g_interpreter_start = HostAddress{(u64)(base_ptr + lowest_vaddr)};
        g_interpreter_end = HostAddress{(u64)(base_ptr + highest_vaddr)};
        program_base = (u8*)base_ptr;
        MemoryMetadata::AddInterpreterRegion(g_interpreter_start.raw(), g_interpreter_end.raw());
        // LoadSymbols("Interpreter", path, (void*)g_interpreter_start);
    }

    phdr = base_ptr + lowest_vaddr + ehdr.phoff();
    phnum = ehdr.phnum();
    phent = ehdr.phentsize();

    fclose(file);

    ok = true;
}

Elf::PeekResult Elf::Peek(const std::filesystem::path& path) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
    }

    u8 e_ident[EI_NIDENT];
    ssize_t result = fread(&e_ident, EI_NIDENT, sizeof(u8), file);
    if (result != 1) {
        ERROR("Failed to read ELF header from file %s", path.c_str());
    }

    fclose(file);

    if (e_ident[0] != 0x7F || e_ident[1] != 'E' || e_ident[2] != 'L' || e_ident[3] != 'F') {
        return PeekResult::NotElf;
    }

    if (e_ident[4] == ELFCLASS32) {
        return PeekResult::Elf32;
    } else if (e_ident[4] == ELFCLASS64) {
        return PeekResult::Elf64;
    }

    ERROR("File %s is an ELF but not a 64-bit or 32-bit ELF file", path.c_str());
    return PeekResult::NotElf;
}