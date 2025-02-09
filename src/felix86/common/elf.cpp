#include <vector>
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
#include "felix86/hle/thread.hpp"

// Not a full ELF implementation, but one that suits our needs as a loader of
// both the executable and the dynamic linker, and one that only supports x86_64
// little-endian

#define PAGE_START(x) ((x) & ~(uintptr_t)(4095))
#define PAGE_OFFSET(x) ((x) & 4095)
#define PAGE_ALIGN(x) (((x) + 4095) & ~(uintptr_t)(4095))

Elf::Elf(bool is_interpreter) : is_interpreter(is_interpreter) {}

Elf::~Elf() {
    if (program) {
        munmap(program, 0);
    }

    if (stack_pointer) {
        munmap(stack_pointer, 0);
    }

    if (brk_base) {
        munmap(brk_base, 0);
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
    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
    }

    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    if (size < sizeof(Elf64_Ehdr)) {
        ERROR("File %s is too small to be an ELF file", path.c_str());
    }
    fseek(file, 0, SEEK_SET);

    Elf64_Ehdr ehdr;
    ssize_t result = fread(&ehdr, sizeof(Elf64_Ehdr), 1, file);
    if (result != 1) {
        ERROR("Failed to read ELF header from file %s", path.c_str());
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        ERROR("File %s is not an ELF file", path.c_str());
    }

    if (ehdr.e_ident[4] != ELFCLASS64) {
        ERROR("File %s is not a 64-bit ELF file", path.c_str());
    }

    if (ehdr.e_ident[5] != ELFDATA2LSB) {
        ERROR("File %s is not a little-endian ELF file", path.c_str());
    }

    if (ehdr.e_ident[6] != 1 || ehdr.e_version != 1) {
        ERROR("File %s has an invalid version", path.c_str());
    }

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) {
        ERROR("File %s is not an executable or shared object", path.c_str());
    }

    if (ehdr.e_machine != EM_X86_64) {
        ERROR("File %s is not an x86_64 ELF file", path.c_str());
    }

    if (ehdr.e_entry == 0 && ehdr.e_type == ET_EXEC) {
        ERROR("File %s is an executable but has no entry point", path.c_str());
    }

    if (ehdr.e_phoff == 0) {
        ERROR("File %s has no program header table, thus has no loadable segments", path.c_str());
    }

    if (ehdr.e_phnum == 0xFFFF) {
        ERROR("If the number of program headers is greater than or equal to PN_XNUM "
              "(0xffff) "
              "this member has the value PN_XNUM (0xffff). The actual number of "
              "program header "
              "table entries is contained in the sh_info field of the section "
              "header at index 0");
    }

    entry = ehdr.e_entry;

    if (ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
        ERROR("File %s has an invalid program header size", path.c_str());
    }

    std::vector<Elf64_Phdr> phdrtable(ehdr.e_phnum);
    fseek(file, ehdr.e_phoff, SEEK_SET);
    result = fread(phdrtable.data(), sizeof(Elf64_Phdr), ehdr.e_phnum, file);
    if (result != ehdr.e_phnum) {
        ERROR("Failed to read program header table from file %s", path.c_str());
    }

    for (Elf64_Half i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr& phdr = phdrtable[i];
        switch (phdr.p_type) {
        case PT_INTERP: {
            std::string interpreter_str;
            interpreter_str.resize(phdr.p_filesz);
            fseek(file, phdr.p_offset, SEEK_SET);
            result = fread(interpreter_str.data(), 1, phdr.p_filesz, file);
            if (result != phdr.p_filesz) {
                ERROR("Failed to read interpreter from file %s", path.c_str());
            }

            // C++ decided it's a good idea to let a absolute path rhs override the lhs
            // so we convert to relative path
            interpreter = g_rootfs_path / std::filesystem::path(interpreter_str).relative_path();
            break;
        }
        case PT_GNU_STACK: {
            if (phdr.p_flags & PF_X) {
                WARN("Executable stack");
            }
            break;
        }
        case PT_LOAD: {
            if (phdr.p_filesz == 0) {
                break;
            }

            if (phdr.p_vaddr + phdr.p_memsz > highest_vaddr) {
                highest_vaddr = phdr.p_vaddr + phdr.p_memsz;
            }

            if (phdr.p_vaddr < lowest_vaddr) {
                lowest_vaddr = phdr.p_vaddr;
            }
            break;
        }
        }
    }

    // TODO: this allocates it twice interpreter and executable, fix me.
    stack_pointer = (u8*)Threads::AllocateStack().first;

    u64 base_address = 0;
    if (ehdr.e_type == ET_DYN) {
        u64 base_hint = is_interpreter ? g_interpreter_base_hint : g_executable_base_hint;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
        if (base_hint) {
            ASSERT(g_interpreter_base_hint != g_executable_base_hint);
            flags |= MAP_FIXED;
        }
        program = (u8*)mmap((void*)base_hint, highest_vaddr, PROT_NONE, flags, -1, 0);
        base_address = (u64)program;
        if (program == MAP_FAILED) {
            ERROR("Failed to allocate memory for ELF file %s, errno: %d", path.c_str(), -errno);
        }
    } else {
        program = NULL;
        base_address = 0;
    }
    VERBOSE("Allocated program at %p", program);

    for (Elf64_Half i = 0; i < ehdr.e_phnum; i += 1) {
        Elf64_Phdr& phdr = phdrtable[i];
        switch (phdr.p_type) {
        case PT_LOAD: {
            if (phdr.p_filesz == 0) {
                ERROR("Loadable segment has no data in file %s", path.c_str());
                break;
            }

            u64 segment_base = base_address + PAGE_START(phdr.p_vaddr);
            u64 segment_size = phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr);
            u8* addr = (u8*)mmap((void*)segment_base, segment_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

            if (addr == MAP_FAILED) {
                ERROR("Failed to allocate memory for segment in file %s", path.c_str());
            } else {
                VERBOSE("Mapping segment with vaddr %p to %p - %p (%p - %p) (file offset: %08lx)", (void*)phdr.p_vaddr, addr, addr + segment_size,
                        addr - base_address, addr + segment_size - base_address, phdr.p_offset);
                if (addr != (void*)segment_base) {
                    ERROR("Failed to allocate memory at requested address for segment in file %s", path.c_str());
                }
            }

            static int seg_name = 0;
            std::string name = "segment" + std::to_string(seg_name++);
            prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, addr, segment_size, name.c_str());

            u8 prot = 0;
            if (phdr.p_flags & PF_R) {
                prot |= PROT_READ;
            }

            if (phdr.p_flags & PF_W) {
                prot |= PROT_WRITE;
            }

            if (phdr.p_flags & PF_X) {
                prot |= PROT_EXEC;
                executable_segments.push_back({addr, segment_size});
            }

            if (phdr.p_filesz > 0) {
                u64 offset = phdr.p_offset - PAGE_OFFSET(phdr.p_vaddr);
                u64 size = phdr.p_filesz + PAGE_OFFSET(phdr.p_vaddr);
                fseek(file, offset, SEEK_SET);
                result = fread(addr, 1, size, file);
                if (result != size) {
                    ERROR("Failed to read segment from file %s", path.c_str());
                }
            }

            mprotect(addr, segment_size, prot);

            if (phdr.p_memsz > phdr.p_filesz) {
                u64 bss_start = (u64)base_address + phdr.p_vaddr + phdr.p_filesz;
                u64 bss_page_start = PAGE_ALIGN(bss_start);
                u64 bss_page_end = PAGE_ALIGN((u64)base_address + phdr.p_vaddr + phdr.p_memsz);

                // Only clear padding bytes if the section is writable (why does FEX-Emu
                // do this?)
                if (phdr.p_flags & PF_W) {
                    memset((void*)bss_start, 0, bss_page_start - bss_start);
                }

                if (bss_page_start != bss_page_end) {
                    u8* bss = (u8*)mmap((void*)bss_page_start, bss_page_end - bss_page_start, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
                    if (bss == MAP_FAILED) {
                        ERROR("Failed to allocate memory for BSS in file %s", path.c_str());
                    }
                    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, bss, bss_page_end - bss_page_start, "bss");

                    VERBOSE("BSS segment at %p-%p", (void*)bss_page_start, (void*)bss_page_end);

                    memset(bss, 0, bss_page_end - bss_page_start);
                }
            }

            mprotect(addr, phdr.p_memsz, prot);
            break;
        }
        default: {
            break;
        }
        }
    }

    if (!is_interpreter) {
        brk_base = (u8*)mmap((void*)PAGE_ALIGN(highest_vaddr), brk_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (brk_base == MAP_FAILED) {
            ERROR("Failed to allocate memory for brk in file %s", path.c_str());
        }
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, brk_base, brk_size, "brk");
        VERBOSE("BRK base at %p", brk_base);

        g_executable_start = base_address + lowest_vaddr;
        g_executable_end = base_address + highest_vaddr;
        MemoryMetadata::AddRegion("Executable", g_executable_start, g_executable_end);
        LoadSymbols("Executable", path, (void*)g_executable_start);
    } else {
        g_interpreter_start = base_address + lowest_vaddr;
        g_interpreter_end = base_address + highest_vaddr;
        MemoryMetadata::AddInterpreterRegion(g_interpreter_start, g_interpreter_end);
        LoadSymbols("Interpreter", path, (void*)g_interpreter_start);
    }

    phdr = (u8*)(base_address + lowest_vaddr + ehdr.e_phoff);
    phnum = ehdr.e_phnum;
    phent = ehdr.e_phentsize;

    fclose(file);

    ok = true;
}

void Elf::LoadSymbols(const std::string& name, const std::filesystem::path& path, void* base) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        ERROR("Failed to open file %s", path.c_str());
    }

    fseek(file, 0, SEEK_END);
    u64 size = ftell(file);
    if (size < sizeof(Elf64_Ehdr)) {
        fclose(file);
        return;
    }
    fseek(file, 0, SEEK_SET);

    Elf64_Ehdr ehdr;
    size_t result = fread(&ehdr, sizeof(Elf64_Ehdr), 1, file);
    if (result != 1) {
        ERROR("Failed to read ELF header from file %s", path.c_str());
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        fclose(file); // silently return, not an ELF file
        return;
    }

    if (ehdr.e_shnum == 0) {
        fclose(file); // no sections, return
        return;
    }

    std::vector<Elf64_Shdr> shdrtable(ehdr.e_shnum);
    fseek(file, ehdr.e_shoff, SEEK_SET);
    result = fread(shdrtable.data(), sizeof(Elf64_Shdr), ehdr.e_shnum, file);
    if (result != ehdr.e_shnum) {
        ERROR("Failed to read section header table from file %s", path.c_str());
    }

    Elf64_Shdr shstrtab = shdrtable[ehdr.e_shstrndx];
    std::vector<char> shstrtab_data(shstrtab.sh_size);
    fseek(file, shstrtab.sh_offset, SEEK_SET);
    result = fread(shstrtab_data.data(), shstrtab.sh_size, 1, file);
    if (result != 1) {
        ERROR("Failed to read section header string table from file %s", path.c_str());
    }

    Elf64_Shdr dynstr{};
    for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr& shdr = shdrtable[i];
        if (shdr.sh_type == SHT_STRTAB && strcmp(&shstrtab_data[shdr.sh_name], ".dynstr") == 0) {
            dynstr = shdr;
            break;
        }
    }

    if (dynstr.sh_type == SHT_STRTAB) {
        std::vector<char> dynstr_data(dynstr.sh_size);
        fseek(file, dynstr.sh_offset, SEEK_SET);
        result = fread(dynstr_data.data(), dynstr.sh_size, 1, file);
        if (result != 1) {
            ERROR("Failed to read dynamic string table from file %s", path.c_str());
        }

        for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
            Elf64_Shdr& shdr = shdrtable[i];
            if (shdr.sh_type == SHT_DYNSYM) {
                std::vector<Elf64_Sym> dynsym(shdr.sh_size / shdr.sh_entsize);
                fseek(file, shdr.sh_offset, SEEK_SET);
                result = fread(dynsym.data(), shdr.sh_entsize, dynsym.size(), file);
                if (result != dynsym.size()) {
                    ERROR("Failed to read dynamic symbol table from file %s", path.c_str());
                }

                std::string mangle_buffer;
                mangle_buffer.resize(4096);

                FELIX86_LOCK;
                for (Elf64_Sym& sym : dynsym) {
                    if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) {
                        continue;
                    }

                    int status;
                    const char* demangled = abi::__cxa_demangle(&dynstr_data[sym.st_name], NULL, NULL, &status);
                    std::string sym_name;
                    if (demangled) {
                        sym_name = demangled;
                        free((void*)demangled);
                    } else {
                        sym_name = &dynstr_data[sym.st_name];
                    }
                    void* sym_addr = (void*)((u8*)base + sym.st_value);
                    VERBOSE("Dynamic symbol %s at %p", sym_name.c_str(), sym_addr);
                    g_symbols[(u64)sym_addr] = sym_name;
                }
                FELIX86_UNLOCK;
                break;
            }
        }
    }

    Elf64_Shdr strtab{};
    for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr& shdr = shdrtable[i];
        if (shdr.sh_type == SHT_STRTAB && strcmp(&shstrtab_data[shdr.sh_name], ".strtab") == 0) {
            strtab = shdr;
            break;
        }
    }

    if (strtab.sh_type == SHT_STRTAB) {
        std::vector<char> strtab_data(strtab.sh_size);
        fseek(file, strtab.sh_offset, SEEK_SET);
        result = fread(strtab_data.data(), strtab.sh_size, 1, file);
        if (result != 1) {
            ERROR("Failed to read string table from file %s", path.c_str());
        }

        for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
            Elf64_Shdr& shdr = shdrtable[i];
            if (shdr.sh_type == SHT_SYMTAB) {
                std::vector<Elf64_Sym> symtab(shdr.sh_size / shdr.sh_entsize);
                fseek(file, shdr.sh_offset, SEEK_SET);
                result = fread(symtab.data(), shdr.sh_entsize, symtab.size(), file);
                if (result != symtab.size()) {
                    ERROR("Failed to read symbol table from file %s", path.c_str());
                }

                std::string mangle_buffer;
                mangle_buffer.resize(4096);

                FELIX86_LOCK;
                for (Elf64_Sym& sym : symtab) {
                    if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) {
                        continue;
                    }

                    int status;
                    const char* demangled = abi::__cxa_demangle(&strtab_data[sym.st_name], NULL, NULL, &status);
                    std::string sym_name;
                    if (demangled) {
                        sym_name = demangled;
                        free((void*)demangled);
                    } else {
                        sym_name = &strtab_data[sym.st_name];
                    }
                    void* sym_addr = (void*)((u8*)base + sym.st_value);
                    VERBOSE("Symbol %s at %p", sym_name.c_str(), sym_addr);
                    g_symbols[(u64)sym_addr] = sym_name;
                }
                FELIX86_UNLOCK;
                break;
            }
        }
    }

    fclose(file);
}