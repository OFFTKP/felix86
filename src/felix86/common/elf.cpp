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
#include <sys/sysinfo.h>
#include "felix86/common/elf.hpp"
#include "felix86/common/global.hpp"
#include "felix86/common/log.hpp"
#include "felix86/common/perf.hpp"
#include "felix86/common/types.hpp"
#include "felix86/hle/mmap.hpp"

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

    Elf_Ehdr(bool mode32, void* data) : mode32(mode32) {
        if (mode32) {
            inner = Elf32_Ehdr{};
            memcpy(&inner32(), data, sizeof(Elf32_Ehdr));
        } else {
            inner = Elf64_Ehdr{};
            memcpy(&inner64(), data, sizeof(Elf64_Ehdr));
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

    u64 shoff() {
        return mode32 ? inner32().e_shoff : inner64().e_shoff;
    }

    u64 shnum() {
        return mode32 ? inner32().e_shnum : inner64().e_shnum;
    }

    u64 shentsize() {
        return mode32 ? inner32().e_shentsize : inner64().e_shentsize;
    }

    u64 shstrindex() {
        return mode32 ? inner32().e_shstrndx : inner64().e_shstrndx;
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

    Elf_Phdr(bool mode32, void* data) : mode32(mode32) {
        if (mode32) {
            inner = Elf32_Phdr{};
            memcpy(&inner32(), data, sizeof(Elf32_Phdr));
        } else {
            inner = Elf64_Phdr{};
            memcpy(&inner64(), data, sizeof(Elf64_Phdr));
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

struct Elf_Shdr {
    Elf_Shdr(bool mode32, FILE* file) : mode32(mode32) {
        size_t read;
        if (mode32) {
            inner = Elf32_Shdr{};
            read = fread(&inner32(), sizeof(Elf32_Shdr), 1, file);
        } else {
            inner = Elf64_Shdr{};
            read = fread(&inner64(), sizeof(Elf64_Shdr), 1, file);
        }

        if (read != 1) {
            ERROR("Failed to read ELF program header from file");
        }
    }

    Elf_Shdr(bool mode32, void* data) : mode32(mode32) {
        if (mode32) {
            inner = Elf32_Shdr{};
            memcpy(&inner32(), data, sizeof(Elf32_Shdr));
        } else {
            inner = Elf64_Shdr{};
            memcpy(&inner64(), data, sizeof(Elf64_Shdr));
        }
    }

    u64 name_offset() {
        return mode32 ? inner32().sh_name : inner64().sh_name;
    }

    u64 type() {
        return mode32 ? inner32().sh_type : inner64().sh_type;
    }

    u64 address() {
        return mode32 ? inner32().sh_addr : inner64().sh_addr;
    }

    u64 offset() {
        return mode32 ? inner32().sh_offset : inner64().sh_offset;
    }

    u64 size() {
        return mode32 ? inner32().sh_size : inner64().sh_size;
    }

private:
    bool mode32;

    Elf32_Shdr& inner32() {
        return std::get<Elf32_Shdr>(inner);
    }

    Elf64_Shdr& inner64() {
        return std::get<Elf64_Shdr>(inner);
    }

    std::variant<Elf64_Shdr, Elf32_Shdr> inner;
};

struct Elf_Sym {
    Elf_Sym(bool mode32, FILE* file) : mode32(mode32) {
        size_t read;
        if (mode32) {
            inner = Elf32_Sym{};
            read = fread(&inner32(), sizeof(Elf32_Sym), 1, file);
        } else {
            inner = Elf64_Sym{};
            read = fread(&inner64(), sizeof(Elf64_Sym), 1, file);
        }

        if (read != 1) {
            ERROR("Failed to read ELF program header from file");
        }
    }

    Elf_Sym(bool mode32, void* data) : mode32(mode32) {
        if (mode32) {
            inner = Elf32_Sym{};
            memcpy(&inner32(), data, sizeof(Elf32_Sym));
        } else {
            inner = Elf64_Sym{};
            memcpy(&inner64(), data, sizeof(Elf64_Sym));
        }
    }

    u64 offset() {
        return mode32 ? inner32().st_name : inner64().st_name;
    }

    u64 size() {
        return mode32 ? inner32().st_size : inner64().st_size;
    }

    u64 address() {
        return mode32 ? inner32().st_value : inner64().st_value;
    }

    u64 bind() {
        return ELF64_ST_BIND(mode32 ? inner32().st_info : inner64().st_info);
    }

    u64 type() {
        // Macro is the same for 32-bit and 64-bit
        return ELF64_ST_TYPE(mode32 ? inner32().st_info : inner64().st_info);
    }

private:
    bool mode32;

    Elf32_Sym& inner32() {
        return std::get<Elf32_Sym>(inner);
    }

    Elf64_Sym& inner64() {
        return std::get<Elf64_Sym>(inner);
    }

    std::variant<Elf64_Sym, Elf32_Sym> inner;
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
            if ((u64)result != phdr.filesz()) {
                ERROR("Failed to read interpreter from file %s", path.c_str());
            }

            ASSERT(!interpreter_str.empty() && interpreter_str[0] == '/');
            interpreter = g_config.rootfs_path / (&interpreter_str[1]);
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
    u64 base_hint = is_interpreter ? g_config.interpreter_base : g_config.executable_base;
    if ((base_hint & 0xFFF) != 0) {
        ERROR("Base hint is not page aligned for: %s", is_interpreter ? "Interpreter" : "Executable");
    }

    if (ehdr.type() == ET_DYN) {
        // In 32-bit mode the Mapper will properly choose a 32-bit address
        if (base_hint) {
            base_ptr = (u8*)g_mapper->map((u8*)base_hint, highest_vaddr, 0, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        } else {
            base_ptr = (u8*)g_mapper->map(nullptr, highest_vaddr, 0, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }

        unmap_me.push_back({base_ptr, highest_vaddr});
    } else {
        base_ptr = 0;
    }

    if (base_ptr == MAP_FAILED) {
        ERROR("Failed to allocate memory for ELF file %s", path.c_str());
    }

    VERBOSE("Allocated memory at %p-%p", base_ptr, base_ptr + highest_vaddr);

    for (Elf64_Half i = 0; i < ehdr.phnum(); i += 1) {
        Elf_Phdr& phdr = phdrtable[i];
        switch (phdr.type()) {
        case PT_LOAD: {
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

            if (segment_size) {
                void* addr = g_mapper->map((void*)segment_base, segment_size, prot, MAP_PRIVATE | MAP_FIXED, fd, offset);
                if (addr == MAP_FAILED) {
                    ERROR("Failed to allocate memory for segment in file %s. Error: %s", path.c_str(), strerror(errno));
                } else if (addr != (void*)segment_base) {
                    ERROR("Failed to allocate memory at requested address for segment in file %s", path.c_str());
                }
            } else if (phdr.memsz() == 0) {
                // Both filesz and memsz are 0?
                // filesz can be 0 as long as memsz is not 0, such as for bss
                ERROR("Loadable segment has zero size in file %s", path.c_str());
            }

            unmap_me.push_back({(void*)segment_base, segment_size});

            if (phdr.memsz() > phdr.filesz()) {
                u64 bss_start = (u64)base_ptr + phdr.vaddr() + phdr.filesz();
                u64 bss_page_start = PAGE_ALIGN(bss_start);
                u64 bss_page_end = PAGE_ALIGN((u64)base_ptr + phdr.vaddr() + phdr.memsz());
                if (phdr.filesz() != 0) {
                    // This is probably a segment that contains a .data and a .bss right after, so after
                    // the file size starts the bss, the part that should be zeroed
                    if (phdr.flags() & PF_W) {
                        memset((void*)bss_start, 0, bss_page_start - bss_start);
                    }
                }

                if (bss_page_start != bss_page_end) {
                    size_t excess_size = bss_page_end - bss_page_start;
                    void* bss_excess = g_mapper->map((void*)bss_page_start, excess_size, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
                    if (bss_excess == MAP_FAILED) {
                        ERROR("Failed to allocate memory for BSS in file %s", path.c_str());
                    }

                    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, bss_page_start, bss_page_end - bss_page_start, "felix86-bss");
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
        u64 end = (u64)(base_ptr + PAGE_ALIGN(highest_vaddr));
        // The interpreter may be allocated before or after the program. We want to place the brk definitely
        // after both so it has ample room to grow
        g_program_end = std::max(end, g_program_end);
        g_executable_start = (u64)(base_ptr + lowest_vaddr);
        g_executable_end = PAGE_ALIGN((u64)(base_ptr + highest_vaddr));
        g_process_globals.perf->addToFile(g_executable_start, g_executable_end - g_executable_start, path.filename());
    } else {
        u64 end = (u64)(base_ptr + PAGE_ALIGN(highest_vaddr));
        g_program_end = std::max(end, g_program_end);
        g_interpreter_start = (u64)(base_ptr + lowest_vaddr);
        g_interpreter_end = (u64)(base_ptr + highest_vaddr);
        program_base = (u8*)base_ptr;
        g_process_globals.perf->addToFile(g_interpreter_start, g_interpreter_end - g_interpreter_start, path.filename());
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
        return Elf::PeekResult::NotElf;
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

    return PeekResult::NotElf;
}

void Elf::AddSymbols(std::map<u64, Symbol>& symbols, const std::filesystem::path& path, u8* start_of_data, u8* end_of_data) {
    // g_mode32 has already been set at this point
    // Load static symbols first
    size_t dynsym_size = 0;
    do {
        std::string spath = path.string();

        FILE* file = fopen(spath.c_str(), "rb");
        if (!file) {
            WARN("Could not open file for symbols: %s (full path: %s)", spath.c_str(), path.c_str());
            return;
        }

        fseek(file, 0, SEEK_END);
        volatile size_t file_size = ftell(file); // debugger access
        (void)file_size;
        fseek(file, 0, SEEK_SET);

        Elf_Ehdr ehdr(g_mode32, file);

        if (ehdr.shnum() == 0) {
            WARN("Empty section table for file: %s", path.c_str());
            return;
        }

        std::vector<std::unique_ptr<Elf_Phdr>> phdrtable;
        u64 ehdr_phoff = ehdr.phoff();
        fseek(file, ehdr_phoff, SEEK_SET);
        for (u64 i = 0; i < ehdr.phnum(); i++) {
            phdrtable.push_back(std::make_unique<Elf_Phdr>(g_mode32, file));
        }

        std::vector<std::unique_ptr<Elf_Shdr>> shdrtable;
        u64 ehdr_shoff = ehdr.shoff();
        fseek(file, ehdr_shoff, SEEK_SET);
        for (u64 i = 0; i < ehdr.shnum(); i++) {
            shdrtable.push_back(std::make_unique<Elf_Shdr>(g_mode32, file));
        }

        u64 shstrindex = ehdr.shstrindex();
        Elf_Shdr* shstrtable = shdrtable[shstrindex].get();
        ASSERT(shstrtable->type() == SHT_STRTAB);

        std::vector<char> sh_string_table(shstrtable->size());
        u64 shstroff = shstrtable->offset();
        fseek(file, shstroff, SEEK_SET);
        size_t read = fread(sh_string_table.data(), shstrtable->size(), 1, file);
        if (read != 1) {
            ERROR("Failed to read string table?");
        }

        Elf_Shdr *symtab = nullptr, *strtab = nullptr;
        Elf_Shdr* dynsym = nullptr;
        for (u64 i = 0; i < ehdr.shnum(); i++) {
            Elf_Shdr* current = shdrtable[i].get();
            const char* name = sh_string_table.data() + current->name_offset();
            if (strcmp(name, ".symtab") == 0) {
                symtab = current;
            } else if (strcmp(name, ".strtab") == 0) {
                strtab = current;
            } else if (strcmp(name, ".dynsym") == 0) {
                dynsym = current;
            }

            if (symtab && strtab && dynsym) {
                break;
            }
        }

        if (dynsym) {
            // Get the size, because getting it without having access to the sections
            // becomes 5 times harder for whatever reason (see DT_GNU_HASH business)
            // http://deroko.phearless.org/dt_gnu_hash.txt
            // So we get the size here while we can and use it later
            dynsym_size = dynsym->size();
        } else {
            VERBOSE("Could not get dynsym size");
        }

        if (symtab && strtab) {
            std::vector<char> string_table(strtab->size());
            u64 strtab_off = strtab->offset();
            fseek(file, strtab_off, SEEK_SET);
            read = fread(string_table.data(), strtab->size(), 1, file);
            if (read != 1) {
                ERROR("Failed to read the .strtab string table");
            }

            size_t symbol_count = symtab->size() / (g_mode32 ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym));
            std::vector<std::unique_ptr<Elf_Sym>> elf_symbols;
            u64 symtab_off = symtab->offset();
            fseek(file, symtab_off, SEEK_SET);
            for (u64 i = 0; i < symbol_count; i++) {
                elf_symbols.push_back(std::make_unique<Elf_Sym>(g_mode32, file));
            }

            for (u64 i = 0; i < symbol_count; i++) {
                u64 index = elf_symbols[i]->offset();
                const char* symbol = string_table.data() + index;
                if (elf_symbols[i]->address() == 0 || elf_symbols[i]->type() != STT_FUNC) {
                    // We don't care about this symbol
                    // VERBOSE("Skipping symbol %s (address: %lx)", symbol, elf_symbols[i]->address());
                    continue;
                }

                u64 address = elf_symbols[i]->address();
                if (ehdr.type() != ET_EXEC) {
                    // Position independent code (I think this is correct)
                    address += (u64)start_of_data;
                }
                u64 size = elf_symbols[i]->size();
                u64 end = address + size;
                int status;
                const char* output = abi::__cxa_demangle(symbol, nullptr, nullptr, &status);
                Symbol new_symbol = {};
                new_symbol.size = size;
                new_symbol.start = address;
                new_symbol.name = status == 0 ? output : symbol;

                if (output)
                    free((void*)output);

                // For finding with lower_bound
                symbols[end - 1] = new_symbol;
                // VERBOSE("Added new static symbol `%s` at %lx-%lx", new_symbol.name.c_str(), new_symbol.start, new_symbol.start + new_symbol.size);
            }
        } else {
            // VERBOSE("symtab and strtab not found for file %s", path.c_str());
        }

        fclose(file);
    } while (0);

    // Find the dynamic segment, load dynamic symbols that have now been loaded by the interpreter hopefully
    Elf_Ehdr ehdr(g_mode32, start_of_data);

    std::vector<std::unique_ptr<Elf_Phdr>> phdrtable;
    u8* start_of_phdr = start_of_data + ehdr.phoff();
    for (u64 i = 0; i < ehdr.phnum(); i++) {
        void* current_phdr = start_of_phdr + (i * ehdr.phentsize());
        phdrtable.push_back(std::make_unique<Elf_Phdr>(g_mode32, current_phdr));
    }

    Elf_Phdr* dynamic = nullptr;
    for (u64 i = 0; i < ehdr.phnum(); i++) {
        if (phdrtable[i]->type() == PT_DYNAMIC) {
            dynamic = phdrtable[i].get();
            break;
        }
    }

    if (dynamic && !dynsym_size) {
        WARN(".dynamic section found, but couldn't deduce .dynsym size...");
        return;
    }

    if (dynamic) {
        u8* symtab = nullptr;
        const char* strtab = nullptr;
        u8* dynamic_ptr = start_of_data + dynamic->vaddr();

        if (dynamic_ptr > end_of_data) {
            // Probably the mapped file hasn't fully loaded yet, skip
            return;
        }

        size_t count = dynamic->memsz() / (g_mode32 ? sizeof(Elf32_Dyn) : sizeof(Elf64_Dyn));
        for (size_t i = 0; i < count; i++) {
            if (g_mode32) {
                Elf32_Dyn* dyn = (Elf32_Dyn*)(dynamic_ptr + (i * sizeof(Elf32_Dyn)));
                if (dyn->d_tag == DT_SYMTAB) {
                    symtab = (u8*)(u64)dyn->d_un.d_ptr;
                } else if (dyn->d_tag == DT_STRTAB) {
                    strtab = (const char*)(u64)dyn->d_un.d_ptr;
                }
            } else {
                Elf64_Dyn* dyn = (Elf64_Dyn*)(dynamic_ptr + (i * sizeof(Elf64_Dyn)));
                if (dyn->d_tag == DT_SYMTAB) {
                    symtab = (u8*)dyn->d_un.d_ptr;
                } else if (dyn->d_tag == DT_STRTAB) {
                    strtab = (const char*)dyn->d_un.d_ptr;
                }
            }

            if (symtab && strtab)
                break;
        }

        if (symtab > start_of_data && (u8*)strtab > start_of_data) {
            size_t sym_size = g_mode32 ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym);
            size_t dynsym_count = dynsym_size / sym_size;
            size_t mod = dynsym_size % sym_size;
            if (mod != 0) {
                WARN("Couldn't deduce dynamic symbol count, doesn't divide neatly");
                return;
            }

            if (symtab + (sym_size * dynsym_count) > end_of_data || (u8*)strtab > end_of_data) {
                return;
            }

            for (size_t i = 0; i < dynsym_count; i++) {
                u8* data = symtab + (sym_size * i);
                Elf_Sym elf_symbol(g_mode32, data);
                size_t index = elf_symbol.offset();
                const char* symbol = strtab + index;
                if ((u8*)symbol > end_of_data) {
                    // Just in case...
                    WARN("symbol > end_of_data in %s", path.c_str());
                    continue;
                }

                if (elf_symbol.type() != STT_FUNC) {
                    // We don't care about this symbol
                    continue;
                }

                if (elf_symbol.address() == 0) { // not yet resolved? skip
                    // VERBOSE("Found symbol %s but address is 0?", symbol);
                    continue;
                }

                u64 address = (u64)start_of_data + elf_symbol.address();
                u64 end = address + elf_symbol.size();
                int status;
                const char* output = abi::__cxa_demangle(symbol, nullptr, nullptr, &status);
                Symbol new_symbol;
                new_symbol.name = status == 0 ? output : symbol;
                new_symbol.start = address;
                new_symbol.size = elf_symbol.size();
                new_symbol.strong = elf_symbol.bind() != STB_WEAK;

                if (output)
                    free((void*)output);

                auto old_symbol = symbols.find(end - 1);
                if (old_symbol != symbols.end() && old_symbol->second.strong) {
                    // Not weak symbol, don't replace
                    continue;
                }

                symbols[end - 1] = new_symbol;
                // VERBOSE("Added new dynamic symbol `%s` at %lx-%lx", new_symbol.name.c_str(), new_symbol.start, new_symbol.start + new_symbol.size);
            }
        } else {
            // VERBOSE("symtab > start_of_data && (u8*)strtab > start_of_data failed: %p > %p && %p > %p", symtab, start_of_data, strtab,
            // start_of_data);
        }
    } else {
        VERBOSE("dynamic section not found for file %s", path.c_str());
    }
}